#include "media/video.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "mp4/mpd.h"
#include "mp4/remux.h"
#include "util/json.h"
#include "util/log.h"

namespace phx {
namespace {

using Clock = std::chrono::steady_clock;

constexpr auto kProgressEvery = std::chrono::milliseconds(250);  // at most 4 a second

bool default_remux(const std::string &v, const std::string &a, const std::string &out, RemuxResult &res,
                   std::string &err) {
    mp4::RemuxInfo info;
    if (!mp4::remux(v, a, out, info, err)) return false;
    res.width = info.width;
    res.height = info.height;
    res.duration_s = info.duration_s;
    res.has_audio = info.has_audio;
    return true;
}

// The size the server says, or -1 (HEAD refused, no Content-Length...).
int64_t head_size(Http &http, const std::string &url, const Source &src, const std::atomic<bool> &cancel) {
    HttpRequest req;
    req.method = "HEAD";
    req.url = url;
    req.headers = src.headers;
    req.timeout_s = 10;
    req.cancel = &cancel;
    const HttpResponse r = http.send(req);
    if (!r.ok()) return -1;
    const std::string len = r.header("content-length");
    return len.empty() ? -1 : std::atoll(len.c_str());
}

bool looks_like_mp4(const std::string &head) { return head.size() >= 8 && head.compare(4, 4, "ftyp") == 0; }

std::string code_for_status(long status) {
    return status == 403 || status == 404 || status == 410 ? "not_found" : "network";
}

}  // namespace

Video::Video(Cache &cache, Emit emit, VideoDeps deps) : cache_(cache), emit_(std::move(emit)), deps_(std::move(deps)) {
    if (!deps_.resolve)
        deps_.resolve = [this](const std::string &url, int mh, Http &http, const std::atomic<bool> &cancel,
                               Source &out, ResolveError &err) { return resolver_.resolve(url, mh, http, cancel, out, err); };
    if (!deps_.remux) deps_.remux = default_remux;
    if (!deps_.is_fragmented) deps_.is_fragmented = [](const std::string &p) { return mp4::is_fragmented(p); };
    thread_ = std::thread([this] { run(); });
}

Video::~Video() {
    {
        std::lock_guard<std::mutex> lk(m_);
        quit_ = true;
        if (cur_) cur_->cancel = true;
    }
    cv_.notify_all();
    thread_.join();
}

Video::Started Video::start(const std::string &url, int max_height) {
    const std::string key = url + "|" + std::to_string(max_height);
    Started s;
    if (const std::string f = cache_.find(key); !f.empty()) {
        s.ready = true;
        s.info.file = f;
        std::lock_guard<std::mutex> lk(m_);
        if (const auto it = known_.find(key); it != known_.end() && it->second.file == f) s.info = it->second;
        return s;
    }
    std::shared_ptr<Job> dropped;
    {
        std::lock_guard<std::mutex> lk(m_);
        if (cur_ && cur_->key == key && !cur_->cancel) return s;
        if (next_ && next_->key == key) return s;
        dropped = std::move(next_);
        next_ = std::make_shared<Job>();
        next_->url = url;
        next_->key = key;
        next_->max_height = max_height;
        if (cur_) cur_->cancel = true;
    }
    cv_.notify_all();
    if (dropped) emit_error(dropped->url, "cancelled", "replaced by another video");
    return s;
}

void Video::cancel(const std::string &url) {
    std::shared_ptr<Job> dropped;
    {
        std::lock_guard<std::mutex> lk(m_);
        if (cur_ && cur_->url == url) cur_->cancel = true;  // the worker says so itself
        if (next_ && next_->url == url) dropped = std::move(next_);
    }
    if (dropped) emit_error(dropped->url, "cancelled", "cancelled");
}

void Video::emit_error(const std::string &url, const std::string &code, const std::string &msg) {
    emit_(JsonOut().str("url", url).str("state", "error").str("code", code).str("message", msg).fields());
}

void Video::run() {
    std::unique_lock<std::mutex> lk(m_);
    while (!quit_) {
        if (!next_) {
            cv_.wait(lk);
            continue;
        }
        cur_ = std::move(next_);
        next_.reset();
        const std::shared_ptr<Job> job = cur_;
        lk.unlock();
        VideoInfo info;
        std::string msg;
        cache_.hold(job->key);
        std::string code = work(*job, info, msg);
        cache_.release(job->key);
        if (code.empty() && job->cancel) code = "cancelled";  // asked to stop right as it finished
        lk.lock();
        if (code.empty()) known_[job->key] = info;
        cur_.reset();
        const bool quitting = quit_;
        lk.unlock();
        if (!quitting) {
            if (code.empty())
                emit_(JsonOut().str("url", job->url).str("state", "ready").str("file", info.file).num("w", info.w)
                          .num("h", info.h).real("duration_s", info.duration_s).boolean("audio", info.audio).fields());
            else
                emit_error(job->url, code, msg.empty() ? code : msg);
        }
        lk.lock();
    }
}

std::string Video::work(Job &job, VideoInfo &info, std::string &msg) {
    Http http;
    const std::atomic<bool> &cancel = job.cancel;
    struct Piece {
        std::string url, part;
        std::string head;  // its first bytes
    };
    std::vector<Piece> pieces;
    auto cleanup = [&] {
        for (const Piece &p : pieces) std::remove(p.part.c_str());
    };
    auto fail = [&](const std::string &code, const std::string &m) {
        msg = m;
        cleanup();
        return cancel ? std::string("cancelled") : code;
    };

    Source src;
    ResolveError rerr;
    if (!deps_.resolve(job.url, job.max_height, http, cancel, src, rerr)) return fail(rerr.code, rerr.message);

    bool has_audio_piece = false;
    if (src.dash) {
        HttpRequest req;
        req.url = src.url;
        req.headers = src.headers;
        req.timeout_s = 15;
        req.cancel = &cancel;
        const HttpResponse r = http.send(req);
        if (cancel) return fail("cancelled", "cancelled");
        if (!r.ok()) return fail(r.error.empty() ? code_for_status(r.status) : "network", "manifest: " + (r.error.empty() ? "status " + std::to_string(r.status) : r.error));
        mp4::Mpd mpd;
        std::string err;
        if (!mp4::parse_mpd(r.body, src.url, mpd, err)) return fail("unsupported", "manifest: " + err);
        const mp4::Rendition *v = mp4::pick_video(mpd, job.max_height);
        if (!v) return fail("unsupported", "no H.264 video in the manifest");
        const mp4::Rendition *a = mp4::pick_audio(mpd, 128000);
        pieces.push_back({v->url, cache_.part_path(job.key, "video"), ""});
        if (a) {
            pieces.push_back({a->url, cache_.part_path(job.key, "audio"), ""});
            has_audio_piece = true;
        }
    } else {
        pieces.push_back({src.url, cache_.part_path(job.key, "file"), ""});
    }

    // Sizes first: `total` for the progress, and a file over the cap is refused before a byte moves.
    uint64_t total = 0;
    bool known = true;
    for (const Piece &p : pieces) {
        const int64_t n = head_size(http, p.url, src, cancel);
        if (n < 0) known = false;
        else total += uint64_t(n);
    }
    if (cancel) return fail("cancelled", "cancelled");
    if (total > kMaxVideoBytes) return fail("unsupported", "video is over 300 MB");
    if (!known) total = 0;

    uint64_t done = 0;
    Clock::time_point last{};
    auto progress = [&](bool force) {
        const auto now = Clock::now();
        if (!force && now - last < kProgressEvery) return;
        last = now;
        emit_(JsonOut().str("url", job.url).str("state", "progress").num("done", int64_t(done)).num("total", int64_t(total)).fields());
    };
    progress(true);
    for (Piece &p : pieces) {
        FILE *f = std::fopen(p.part.c_str(), "wb");
        if (!f) return fail("internal", "cannot write " + p.part);
        bool over = false, write_failed = false;
        HttpRequest req;
        req.url = p.url;
        req.headers = src.headers;
        req.timeout_s = 20;
        req.cancel = &cancel;
        req.on_data = [&](const uint8_t *d, size_t n) {
            if (p.head.size() < 12) p.head.append(reinterpret_cast<const char *>(d), std::min<size_t>(n, 12 - p.head.size()));
            if (done + n > kMaxVideoBytes) return over = true, false;
            if (std::fwrite(d, 1, n, f) != n) return write_failed = true, false;
            done += n;
            progress(false);
            return true;
        };
        const HttpResponse r = http.send(req);
        const bool close_failed = std::fclose(f) != 0;
        if (cancel) return fail("cancelled", "cancelled");
        if (r.status >= 400) return fail(code_for_status(r.status), "status " + std::to_string(r.status));
        if (over) return fail("unsupported", "video is over 300 MB");
        if (write_failed || close_failed) return fail("internal", "cannot write " + p.part);
        if (!r.error.empty() || r.status < 200 || r.status >= 300)
            return fail("network", r.error.empty() ? "status " + std::to_string(r.status) : r.error);
        if (!looks_like_mp4(p.head)) return fail("unsupported", "not an MP4 file");
    }
    progress(true);

    const std::string fin = cache_.final_path(job.key, ".mp4");
    RemuxResult res;
    std::string err;
    if (src.dash) {
        const std::string audio = has_audio_piece ? pieces[1].part : "";
        if (deps_.remux(pieces[0].part, audio, fin, res, err)) {
            cache_.added(fin);
            info = {fin, res.width, res.height, res.duration_s, res.has_audio};
        } else if (err == "not fragmented" && !has_audio_piece) {
            info.file = cache_.commit(job.key, ".mp4", pieces[0].part);  // video only, already playable
            info.audio = false;
        } else {
            return fail("unsupported", "remux: " + err);
        }
    } else if (deps_.is_fragmented(pieces[0].part)) {
        if (deps_.remux(pieces[0].part, "", fin, res, err)) {
            cache_.added(fin);
            info = {fin, res.width, res.height, res.duration_s, res.has_audio};
        } else if (err == "not fragmented") {
            info.file = cache_.commit(job.key, ".mp4", pieces[0].part);
        } else {
            return fail("unsupported", "remux: " + err);
        }
    } else {
        info.file = cache_.commit(job.key, ".mp4", pieces[0].part);
    }
    cleanup();
    if (info.file.empty()) return fail("internal", "cannot write the cache");
    return "";
}

}  // namespace phx
