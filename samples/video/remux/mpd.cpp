#include "mp4/mpd.h"

#include <charconv>
#include <cstdint>

namespace phx::mp4 {
namespace {

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string_view trim(std::string_view s) {
    while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
    while (!s.empty() && is_space(s.back())) s.remove_suffix(1);
    return s;
}

void put_utf8(std::string &o, uint32_t c) {
    if (c < 0x80) {
        o += char(c);
    } else if (c < 0x800) {
        o += char(0xC0 | c >> 6); o += char(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        o += char(0xE0 | c >> 12); o += char(0x80 | (c >> 6 & 0x3F)); o += char(0x80 | (c & 0x3F));
    } else {
        o += char(0xF0 | c >> 18); o += char(0x80 | (c >> 12 & 0x3F));
        o += char(0x80 | (c >> 6 & 0x3F)); o += char(0x80 | (c & 0x3F));
    }
}

// XML character references and the five predefined entities; anything else is kept as written.
std::string unescape(std::string_view s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        size_t semi;
        if (s[i] != '&' || (semi = s.find(';', i)) == std::string_view::npos || semi - i > 10) {
            o += s[i];
            continue;
        }
        std::string_view e = s.substr(i + 1, semi - i - 1);
        uint32_t c = 0;
        if (e == "amp") c = '&';
        else if (e == "lt") c = '<';
        else if (e == "gt") c = '>';
        else if (e == "quot") c = '"';
        else if (e == "apos") c = '\'';
        else if (e.size() > 1 && e[0] == '#') {
            bool hex = e[1] == 'x' || e[1] == 'X';
            std::string_view d = e.substr(hex ? 2 : 1);
            auto r = std::from_chars(d.data(), d.data() + d.size(), c, hex ? 16 : 10);
            if (r.ec != std::errc() || r.ptr != d.data() + d.size() || c == 0 || c > 0x10FFFF) c = 0;
        }
        if (!c) {
            o += s[i];
            continue;
        }
        put_utf8(o, c);
        i = semi;
    }
    return o;
}

int to_int(std::string_view s) {
    int v = 0;
    s = trim(s);
    auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    return r.ec == std::errc() && v > 0 ? v : 0;
}

// ISO 8601 duration as used by DASH: PnDTnHnMn.nS (years and months are not a fixed length: 0).
double parse_duration(std::string_view s) {
    s = trim(s);
    if (s.empty() || s[0] != 'P') return 0;
    double total = 0;
    bool time = false;
    for (size_t i = 1; i < s.size();) {
        if (s[i] == 'T') {
            time = true;
            ++i;
            continue;
        }
        double v = 0, scale = 1;
        bool digits = false, frac = false;
        for (; i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.'); ++i) {
            if (s[i] == '.') {
                if (frac) return 0;
                frac = true;
            } else if (frac) {
                v += (scale /= 10) * (s[i] - '0');
                digits = true;
            } else {
                v = v * 10 + (s[i] - '0');
                digits = true;
            }
        }
        if (!digits || i >= s.size()) return 0;
        char u = s[i++];
        if (!time && u == 'D') total += v * 86400;
        else if (!time && u == 'W') total += v * 604800;
        else if (time && u == 'H') total += v * 3600;
        else if (time && u == 'M') total += v * 60;
        else if (time && u == 'S') total += v;
        else return 0;
    }
    return total;
}

bool has_scheme(std::string_view u) {
    if (u.empty() || !((u[0] >= 'a' && u[0] <= 'z') || (u[0] >= 'A' && u[0] <= 'Z'))) return false;
    for (char c : u) {
        if (c == ':') return true;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '+' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return false;
}

// RFC 3986 5.2.4 on the path part; the query of `path` (if any) is kept as is.
std::string remove_dots(std::string_view path) {
    size_t q = path.find_first_of("?#");
    std::string_view tail = q == std::string_view::npos ? "" : path.substr(q);
    path = path.substr(0, q);
    std::vector<std::string_view> segs;
    size_t start = 0;
    bool absolute = !path.empty() && path[0] == '/';
    if (absolute) start = 1;
    bool trailing = false;
    while (start <= path.size()) {
        size_t slash = path.find('/', start);
        std::string_view seg = path.substr(start, slash == std::string_view::npos ? std::string_view::npos
                                                                                 : slash - start);
        trailing = seg == "." || seg == "..";
        if (seg == "..") {
            if (!segs.empty()) segs.pop_back();
        } else if (seg != ".") {
            segs.push_back(seg);
        }
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    std::string o = absolute ? "/" : "";
    for (size_t i = 0; i < segs.size(); ++i) {
        if (i) o += '/';
        o += segs[i];
    }
    if (trailing && !segs.empty()) o += '/';
    return o + std::string(tail);
}

std::string resolve(std::string_view base, std::string_view ref) {
    if (ref.empty()) return std::string(base);
    if (has_scheme(ref)) return std::string(ref);
    std::string_view scheme, authority, path = base;
    if (size_t c = base.find("://"); c != std::string_view::npos && has_scheme(base)) {
        scheme = base.substr(0, c);
        std::string_view rest = base.substr(c + 3);
        size_t p = rest.find_first_of("/?#");
        authority = rest.substr(0, p);
        path = p == std::string_view::npos ? "" : rest.substr(p);
    }
    std::string prefix = scheme.empty() ? "" : std::string(scheme) + "://" + std::string(authority);
    if (ref.substr(0, 2) == "//") return scheme.empty() ? std::string(ref) : std::string(scheme) + ":" + std::string(ref);
    if (ref[0] == '/') return prefix + remove_dots(ref);
    path = path.substr(0, path.find_first_of("?#"));
    if (ref[0] == '?') return prefix + std::string(path) + std::string(ref);
    if (ref[0] == '#') return prefix + std::string(path);
    size_t slash = path.rfind('/');
    std::string dir = slash == std::string_view::npos ? (scheme.empty() ? "" : "/")
                                                      : std::string(path.substr(0, slash + 1));
    return prefix + remove_dots(dir + std::string(ref));
}

struct Attrs {
    std::string content_type, mime, codecs, base;
    int width = 0, height = 0, bandwidth = 0;
    bool has_base = false;
};

struct Tag {
    std::string_view name;   // local name: a namespace prefix is dropped
    bool closing = false, empty = false;
    std::vector<std::pair<std::string_view, std::string_view>> attrs;   // values still escaped
    std::string attr(std::string_view n) const {
        for (auto &a : attrs)
            if (a.first == n) return unescape(a.second);
        return {};
    }
};

// Parses the tag starting at xml[i] == '<'. Returns the index after '>' or npos if malformed.
size_t read_tag(std::string_view xml, size_t i, Tag &t) {
    t = Tag();
    size_t n = xml.size(), j = i + 1;
    if (j < n && xml[j] == '/') t.closing = true, ++j;
    size_t s = j;
    while (j < n && !is_space(xml[j]) && xml[j] != '>' && xml[j] != '/') ++j;
    t.name = xml.substr(s, j - s);
    if (size_t c = t.name.rfind(':'); c != std::string_view::npos) t.name.remove_prefix(c + 1);
    if (t.name.empty()) return std::string_view::npos;
    for (;;) {
        while (j < n && is_space(xml[j])) ++j;
        if (j >= n) return std::string_view::npos;
        if (xml[j] == '>') return j + 1;
        if (xml[j] == '/') {
            if (j + 1 < n && xml[j + 1] == '>') return t.empty = true, j + 2;
            return std::string_view::npos;
        }
        s = j;
        while (j < n && !is_space(xml[j]) && xml[j] != '=' && xml[j] != '>' && xml[j] != '/') ++j;
        std::string_view an = xml.substr(s, j - s);
        while (j < n && is_space(xml[j])) ++j;
        if (j >= n || xml[j] != '=') return std::string_view::npos;
        ++j;
        while (j < n && is_space(xml[j])) ++j;
        if (j >= n || (xml[j] != '"' && xml[j] != '\'')) return std::string_view::npos;
        char q = xml[j++];
        size_t e = xml.find(q, j);
        if (e == std::string_view::npos) return std::string_view::npos;
        t.attrs.emplace_back(an, xml.substr(j, e - j));
        j = e + 1;
    }
}

void take(Attrs &a, const Tag &t) {
    if (auto v = t.attr("contentType"); !v.empty()) a.content_type = v;
    if (auto v = t.attr("mimeType"); !v.empty()) a.mime = v;
    if (auto v = t.attr("codecs"); !v.empty()) a.codecs = v;
    if (int v = to_int(t.attr("width"))) a.width = v;
    if (int v = to_int(t.attr("height"))) a.height = v;
    if (int v = to_int(t.attr("bandwidth"))) a.bandwidth = v;
}

bool starts(std::string_view s, std::string_view p) { return s.substr(0, p.size()) == p; }

}  // namespace

bool parse_mpd(std::string_view xml, std::string_view mpd_url, Mpd &out, std::string &err) {
    out = Mpd();
    err.clear();
    try {
        bool mpd = false, in_period = false, in_as = false, in_rep = false;
        std::string mpd_base, period_base;
        bool mpd_has_base = false, period_has_base = false;
        Attrs as, rep;
        auto finish_rep = [&] {
            in_rep = false;
            if (!rep.has_base && !as.has_base) return;
            std::string base = resolve(mpd_url, mpd_base);
            base = resolve(base, period_base);
            base = resolve(base, as.base);
            Rendition r;
            r.url = resolve(base, rep.has_base ? rep.base : std::string());
            r.codecs = rep.codecs;
            r.bandwidth = rep.bandwidth;
            std::string type = rep.content_type;
            if (type.empty() && starts(rep.mime, "video/")) type = "video";
            if (type.empty() && starts(rep.mime, "audio/")) type = "audio";
            if (type.empty() && starts(rep.codecs, "avc")) type = "video";
            if (type.empty() && starts(rep.codecs, "mp4a")) type = "audio";
            if (type == "video") {
                r.width = rep.width;
                r.height = rep.height;
                out.video.push_back(std::move(r));
            } else if (type == "audio") {
                out.audio.push_back(std::move(r));
            }
        };
        Tag t;
        for (size_t i = 0; (i = xml.find('<', i)) != std::string_view::npos;) {
            std::string_view rest = xml.substr(i);
            if (starts(rest, "<!--")) {
                size_t e = xml.find("-->", i + 4);
                if (e == std::string_view::npos) break;
                i = e + 3;
                continue;
            }
            if (starts(rest, "<?") || starts(rest, "<!")) {
                size_t e = xml.find('>', i);
                if (e == std::string_view::npos) break;
                i = e + 1;
                continue;
            }
            size_t next = read_tag(xml, i, t);
            if (next == std::string_view::npos) break;   // malformed or cut short: keep what we have
            i = next;
            if (t.name == "MPD" && !t.closing) {
                mpd = true;
                out.duration_s = parse_duration(t.attr("mediaPresentationDuration"));
            }
            if (!mpd) continue;
            if (t.name == "Period") {
                if (t.closing) break;   // only the first Period
                in_period = true;
            } else if (t.name == "AdaptationSet") {
                in_as = !t.closing && !t.empty;
                in_rep = false;
                as = Attrs();
                if (in_as) take(as, t);
            } else if (t.name == "Representation") {
                if (t.closing) {
                    if (in_rep) finish_rep();
                } else {
                    rep = as;   // inherited from the AdaptationSet, then overridden
                    rep.has_base = false;
                    rep.base.clear();
                    take(rep, t);
                    in_rep = true;
                    if (t.empty) finish_rep();
                }
            } else if (t.name == "BaseURL" && !t.closing && !t.empty) {
                size_t e = xml.find("</", i);
                if (e == std::string_view::npos) break;
                std::string_view text = trim(xml.substr(i, e - i));
                std::string url;
                if (starts(text, "<![CDATA[") && text.size() >= 12 && text.substr(text.size() - 3) == "]]>")
                    url = std::string(trim(text.substr(9, text.size() - 12)));
                else
                    url = unescape(text);
                i = e;
                // The first BaseURL of an element wins (later ones are alternatives).
                if (in_rep) {
                    if (!rep.has_base) rep.base = url, rep.has_base = true;
                } else if (in_as) {
                    if (!as.has_base) as.base = url, as.has_base = true;
                } else if (in_period) {
                    if (!period_has_base) period_base = url, period_has_base = true;
                } else if (!mpd_has_base) {
                    mpd_base = url, mpd_has_base = true;
                }
            }
        }
        if (!mpd) {
            err = "not an MPD";
            return false;
        }
        if (out.video.empty()) {
            out = Mpd();
            err = "no video Representation";
            return false;
        }
        return true;
    } catch (...) {
        out = Mpd();
        err = "out of memory";
        return false;
    }
}

// "480p" is the shorter side: Reddit's DASH_480 of a portrait video is 480x854.
static int short_side(const Rendition &r) { return r.width > 0 && r.width < r.height ? r.width : r.height; }

const Rendition *pick_video(const Mpd &m, int max_short_side) {
    const Rendition *fit = nullptr, *low = nullptr;
    for (const auto &r : m.video) {
        if (!starts(r.codecs, "avc1")) continue;
        const int side = short_side(r);
        if (side <= max_short_side &&
            (!fit || side > short_side(*fit) || (side == short_side(*fit) && r.bandwidth > fit->bandwidth)))
            fit = &r;
        if (!low || side < short_side(*low) || (side == short_side(*low) && r.bandwidth < low->bandwidth))
            low = &r;
    }
    return fit ? fit : low;
}

const Rendition *pick_audio(const Mpd &m, int max_bps) {
    const Rendition *fit = nullptr, *low = nullptr;
    for (const auto &r : m.audio) {
        if (!starts(r.codecs, "mp4a")) continue;
        if (r.bandwidth <= max_bps && (!fit || r.bandwidth > fit->bandwidth)) fit = &r;
        if (!low || r.bandwidth < low->bandwidth) low = &r;
    }
    return fit ? fit : low;
}

}  // namespace phx::mp4
