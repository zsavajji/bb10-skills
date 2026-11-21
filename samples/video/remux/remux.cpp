// Fragmented MP4 -> progressive MP4 (see remux.h).
//
// Pass 1 reads every top-level box header, loads only moov and each moof (capped), and builds a
// sample table: file offset, size, duration, composition offset, sync flag. mdat is never read.
// Pass 2 writes ftyp, moov and one mdat, copying sample bytes by range through a fixed buffer.
//
// Choices made for the phone's player (mm-renderer, 2014), after bbtube's remuxer, which plays
// there (bbtube/src/utils/mp4_stream_remux.cpp):
// - ftyp is a plain progressive one, major "isom". bbtube found that a copied DASH ftyp (major
//   "dash") makes mm-renderer reject the whole file; we write ffmpeg's usual isom/iso2/avc1/mp41.
// - ctts is always version 0 (unsigned). Signed (v1) offsets came with the 2012 edition of the
//   spec and a 2014 parser may read them as huge unsigned values; bbtube only ever writes v0 for
//   this player. Negative source offsets (trun v1) are shifted up by the smallest one and the
//   edit list's media_time absorbs the shift, so presentation times stay exact; only the decode
//   times move earlier by that shift.
// - edts/elst only when needed (bbtube writes none; a player that ignores it is off by at most
//   the shift, a frame or two): it carries the source's start (audio priming such as v.redd.it's
//   media_time 1024, a B-frame delay) and a late first tfdt as an empty edit.
// - Movie timescale 1000 like bbtube and ffmpeg's long-time default: small numbers for a 32-bit
//   parser. Empty edits are therefore rounded to the millisecond; the media edit runs to the end
//   of the composition timeline, rounded up, so that no frame falls outside it.
// - Samples are grouped in chunks of half a second of one track, alternating between the tracks
//   in presentation order (bbtube wrote all audio then all video, which worked, but a player
//   reading sequentially from slow flash then seeks across the whole file).
// - Plain 32-bit stco unless an offset needs co64; mdat gets a 64-bit size only above 4 GB.
// - Box order as ffmpeg writes it: mvhd, trak(tkhd, edts, mdia(mdhd, hdlr, minf(vmhd|smhd, dinf,
//   stbl(stsd stts stss ctts stsc stsz stco)))). No mvex, no udta.
//
// QNX/ARMv7: off_t is 32-bit unless _FILE_OFFSET_BITS is 64 (then pread/fstat/open map to the
// *64 versions of QNX 6.6's libc). Defined here for this file only; no off_t crosses the API.
#define _FILE_OFFSET_BITS 64

#include "mp4/remux.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

static_assert(sizeof(off_t) == 8, "need 64-bit file offsets");

namespace phx::mp4 {
namespace {

constexpr uint64_t kMaxHeadBox = 16u << 20;   // moov or moof held in memory
// Reddit caps videos at 15 min: ~27k video and ~42k audio samples. 1M per track is hours of
// media and bounds the tables of a hostile file to ~24 MB per track.
constexpr size_t kMaxSamples = 1000000;
constexpr uint32_t kMovieTimescale = 1000;
constexpr double kChunkSeconds = 0.5;
constexpr size_t kCopyBuffer = 256u << 10;

struct Error {
    std::string msg;
};
[[noreturn]] void fail(std::string m) { throw Error{std::move(m)}; }

constexpr uint32_t fc(const char (&s)[5]) {
    return uint32_t(uint8_t(s[0])) << 24 | uint32_t(uint8_t(s[1])) << 16 | uint32_t(uint8_t(s[2])) << 8 |
           uint32_t(uint8_t(s[3]));
}

std::string name(uint32_t t) {
    std::string s(4, '?');
    for (int i = 0; i < 4; ++i) {
        char c = char(t >> (24 - 8 * i));
        if (c >= 0x20 && c < 0x7f) s[i] = c;
    }
    return s;
}

uint64_t add(uint64_t a, uint64_t b) {
    uint64_t r;
    if (__builtin_add_overflow(a, b, &r)) fail("value out of range");
    return r;
}
uint64_t mul(uint64_t a, uint64_t b) {
    uint64_t r;
    if (__builtin_mul_overflow(a, b, &r)) fail("value out of range");
    return r;
}

// ---- reading: a bounds-checked cursor over one box's bytes ------------------------------------

struct Cur {
    const uint8_t *p = nullptr, *e = nullptr;
    uint32_t box = 0;   // for error messages
    size_t left() const { return size_t(e - p); }
    void need(size_t n) const {
        if (left() < n) fail(name(box) + " box truncated");
    }
    uint8_t u8() { need(1); return *p++; }
    uint16_t u16() { need(2); uint16_t v = uint16_t(p[0] << 8 | p[1]); p += 2; return v; }
    uint32_t u32() {
        need(4);
        uint32_t v = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        p += 4;
        return v;
    }
    uint64_t u64() { uint64_t h = u32(); return h << 32 | u32(); }
    void skip(size_t n) { need(n); p += n; }
};

// The next child box of `c` (consumed from it). A size of 0 means "to the end of the parent".
bool child(Cur &c, uint32_t &type, Cur &body) {
    if (c.left() == 0) return false;
    if (c.left() < 8) {   // a few zero bytes closing a container (old QuickTime writers)
        if (std::all_of(c.p, c.e, [](uint8_t x) { return x == 0; })) return c.p = c.e, false;
        fail(name(c.box) + ": truncated child box");
    }
    uint64_t size = c.u32();
    type = c.u32();
    uint64_t hdr = 8;
    if (size == 1) {
        size = c.u64();
        hdr = 16;
    } else if (size == 0) {
        size = hdr + c.left();
    }
    if (size < hdr || size - hdr > c.left()) fail(name(type) + " box larger than its parent " + name(c.box));
    body.p = c.p;
    body.e = c.p + size_t(size - hdr);
    body.box = type;
    c.p = body.e;
    return true;
}

// ---- files ------------------------------------------------------------------------------------

struct Fd {
    int fd = -1;
    Fd() = default;
    Fd(const Fd &) = delete;
    Fd &operator=(const Fd &) = delete;
    ~Fd() {
        if (fd >= 0) ::close(fd);
    }
};

void pread_all(int fd, uint8_t *dst, size_t n, uint64_t off, const char *what) {
    while (n) {
        if (off > uint64_t(std::numeric_limits<off_t>::max())) fail(std::string(what) + ": offset too large");
        ssize_t r = ::pread(fd, dst, n, off_t(off));
        if (r < 0 && errno == EINTR) continue;
        if (r < 0) fail(std::string(what) + ": read error: " + std::strerror(errno));
        if (r == 0) fail(std::string(what) + ": unexpected end of file");
        dst += r;
        n -= size_t(r);
        off += uint64_t(r);
    }
}

void write_all(int fd, const uint8_t *src, size_t n) {
    while (n) {
        ssize_t r = ::write(fd, src, n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) fail(std::string("write error: ") + std::strerror(r < 0 ? errno : EIO));
        src += r;
        n -= size_t(r);
    }
}

uint64_t open_input(Fd &f, const std::string &path, const char *what) {
    f.fd = ::open(path.c_str(), O_RDONLY);
    if (f.fd < 0) fail(std::string(what) + ": cannot open: " + std::strerror(errno));
    struct stat st;
    if (::fstat(f.fd, &st) != 0 || !S_ISREG(st.st_mode)) fail(std::string(what) + ": not a regular file");
    return uint64_t(st.st_size);
}

struct TopBox {
    uint32_t type = 0;
    uint64_t off = 0, hdr = 0, size = 0;   // size includes the header
    bool truncated = false;                // the box runs past the end of the file
};

// Header of the top-level box at `off` (< fsize).
TopBox top_box(int fd, uint64_t off, uint64_t fsize, const char *what) {
    uint8_t h[16];
    if (fsize - off < 8) fail(std::string(what) + ": truncated box header at end of file");
    pread_all(fd, h, 8, off, what);
    TopBox b;
    b.off = off;
    b.hdr = 8;
    b.size = uint64_t(h[0]) << 24 | uint64_t(h[1]) << 16 | uint64_t(h[2]) << 8 | h[3];
    b.type = uint32_t(h[4]) << 24 | uint32_t(h[5]) << 16 | uint32_t(h[6]) << 8 | h[7];
    if (b.size == 1) {
        if (fsize - off < 16) fail(std::string(what) + ": truncated box header at end of file");
        pread_all(fd, h + 8, 8, off + 8, what);
        b.size = 0;
        for (int i = 8; i < 16; ++i) b.size = b.size << 8 | h[i];
        b.hdr = 16;
    } else if (b.size == 0) {
        b.size = fsize - off;
    }
    if (b.size < b.hdr) fail(std::string(what) + ": bad size of " + name(b.type) + " box");
    b.truncated = b.size > fsize - off;
    return b;
}

void load_box(int fd, const TopBox &b, std::vector<uint8_t> &buf, Cur &c, const char *what) {
    if (b.truncated) fail(std::string(what) + ": " + name(b.type) + " box truncated");
    uint64_t n = b.size - b.hdr;
    if (n > kMaxHeadBox) fail(std::string(what) + ": " + name(b.type) + " box too large");
    buf.resize(size_t(n));
    if (n) pread_all(fd, buf.data(), size_t(n), b.off + b.hdr, what);
    c.p = buf.data();
    c.e = buf.data() + n;
    c.box = b.type;
}

// 'F': has movie fragments. 'P': a plain MP4. Throws on anything that is not an MP4.
char probe(int fd, uint64_t fsize, const char *what) {
    std::vector<uint8_t> buf;
    bool moov = false;
    for (uint64_t off = 0; off < fsize;) {
        TopBox b = top_box(fd, off, fsize, what);
        if (b.type == fc("moof")) return 'F';
        if (b.type == fc("moov")) {
            Cur c, body;
            load_box(fd, b, buf, c, what);
            for (uint32_t t; child(c, t, body);)
                if (t == fc("mvex")) return 'F';
            moov = true;
        }
        if (b.truncated) break;
        off += b.size;
    }
    if (!moov) fail(std::string(what) + ": no moov box (not an MP4 file)");
    return 'P';
}

// ---- the source track -------------------------------------------------------------------------

struct Sample {
    uint64_t off;
    uint32_t size, dur;
    int32_t cts;
    uint16_t sdi;
    bool sync;
};

struct Track {
    const char *what = "";     // "video" / "audio"
    uint32_t handler = 0;      // vide / soun
    uint32_t id = 0, timescale = 0, movie_timescale = 0;
    uint16_t language = 0x55c4;   // "und"
    uint32_t matrix[9] = {0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000};
    uint32_t width = 0, height = 0;   // tkhd, 16.16
    int entry_w = 0, entry_h = 0;     // visual sample entry
    uint32_t pasp_h = 0, pasp_v = 0;  // its pixel aspect ratio, if given
    std::vector<uint8_t> hdlr, stsd;  // box bodies, copied as they are
    uint32_t stsd_count = 0;
    bool moov_samples = false;
    int traks = 0;
    // trex
    uint32_t d_sdi = 1, d_dur = 0, d_size = 0, d_flags = 0;
    // edit list: empty edits (movie timescale) before the first media edit's start (media timescale)
    uint64_t src_empty = 0;
    int64_t src_media_time = 0;
    // fragments
    std::vector<Sample> s;
    uint64_t first_dts = 0, next_dts = 0;
};

[[noreturn]] void tfail(const Track &t, const std::string &m) { fail(std::string(t.what) + ": " + m); }

void parse_stsd(Track &t, Cur c) {
    c.skip(4);
    t.stsd_count = c.u32();
    if (t.stsd_count == 0 || t.stsd_count > 0xFFFF) tfail(t, "bad sample description count");
    for (uint32_t i = 0; i < t.stsd_count; ++i) {
        uint32_t type;
        Cur e;
        if (!child(c, type, e)) tfail(t, "sample description truncated");
        if (name(type).compare(0, 3, "enc") == 0) tfail(t, "encrypted track (" + name(type) + ")");
        size_t hdr = t.handler == fc("vide") ? 78 : 28;
        if (e.left() < hdr) tfail(t, "sample description " + name(type) + " truncated");
        if (t.handler == fc("vide") && i == 0) {
            Cur d = e;
            d.skip(24);
            t.entry_w = d.u16();
            t.entry_h = d.u16();
        }
        if (t.handler == fc("soun")) {
            Cur d = e;
            d.skip(8);
            uint16_t v = d.u16();   // QuickTime sound description versions add fields
            hdr += v == 1 ? 16 : v == 2 ? 36 : 0;
        }
        if (e.left() < hdr) continue;
        e.skip(hdr);
        try {   // codec boxes; a trailing pad that does not parse is not our business
            Cur b;
            for (uint32_t ct; child(e, ct, b);) {
                if (ct == fc("sinf")) tfail(t, "encrypted track (sinf)");
                if (ct == fc("pasp") && i == 0 && t.handler == fc("vide")) {
                    t.pasp_h = b.u32();
                    t.pasp_v = b.u32();
                }
            }
        } catch (const Error &err) {
            if (err.msg.find("encrypted") != std::string::npos) throw;
        }
    }
}

void parse_stbl(Track &t, Cur c) {
    Cur b;
    bool have_stsd = false;
    for (uint32_t type; child(c, type, b);) {
        if (type == fc("stsd")) {
            if (have_stsd) tfail(t, "two stsd boxes");
            have_stsd = true;
            t.stsd.assign(b.p, b.e);
            parse_stsd(t, b);
        } else if (type == fc("stts") || type == fc("stco") || type == fc("co64") || type == fc("stsc")) {
            b.skip(4);
            if (b.u32()) t.moov_samples = true;
        } else if (type == fc("stsz") || type == fc("stz2")) {
            b.skip(8);
            if (b.u32()) t.moov_samples = true;
        }
    }
    if (!have_stsd) tfail(t, "no stsd box");
}

void parse_dref(Track &t, Cur c) {
    c.skip(4);
    uint32_t n = c.u32();
    Cur e;
    for (uint32_t i = 0, type; i < n && child(c, type, e); ++i) {
        uint32_t flags = e.u32() & 0xFFFFFF;
        if (!(flags & 1)) tfail(t, "external data reference");
    }
}

void parse_mdia(Track &t, Cur c) {
    Cur b;
    bool mdhd = false, minf = false;
    for (uint32_t type; child(c, type, b);) {
        if (type == fc("mdhd")) {
            uint8_t v = b.u8();
            b.skip(3);
            b.skip(v == 1 ? 16 : 8);
            t.timescale = b.u32();
            b.skip(v == 1 ? 8 : 4);
            t.language = b.u16() & 0x7FFF;
            mdhd = true;
        } else if (type == fc("hdlr")) {
            t.hdlr.assign(b.p, b.e);
            b.skip(8);
            t.handler = b.u32();
        } else if (type == fc("minf")) {
            minf = true;
            Cur m = b, mb;
            for (uint32_t mt; child(m, mt, mb);) {
                if (mt == fc("dinf")) {
                    Cur d;
                    for (uint32_t dt; child(mb, dt, d);)
                        if (dt == fc("dref")) parse_dref(t, d);
                }
            }
            m = b;   // stbl after dinf: the handler (from hdlr) is known by now
            for (uint32_t mt; child(m, mt, mb);)
                if (mt == fc("stbl")) {
                    if (!t.handler) tfail(t, "no hdlr before minf");
                    parse_stbl(t, mb);
                }
        }
    }
    if (!mdhd || !t.handler || !minf || t.stsd.empty()) tfail(t, "incomplete track (mdhd/hdlr/minf/stsd)");
    if (t.timescale == 0) tfail(t, "zero timescale");
}

void parse_trak(Track &t, Cur c) {
    Cur b;
    bool tkhd = false, mdia = false;
    for (uint32_t type; child(c, type, b);) {
        if (type == fc("tkhd")) {
            uint8_t v = b.u8();
            b.skip(3);
            b.skip(v == 1 ? 16 : 8);
            t.id = b.u32();
            b.skip(4);
            b.skip(v == 1 ? 8 : 4);
            b.skip(16);   // reserved, layer, alternate_group, volume, reserved
            for (auto &m : t.matrix) m = b.u32();
            t.width = b.u32();
            t.height = b.u32();
            tkhd = true;
        } else if (type == fc("mdia")) {
            parse_mdia(t, b);
            mdia = true;
        } else if (type == fc("edts")) {
            Cur e;
            for (uint32_t et; child(b, et, e);) {
                if (et != fc("elst")) continue;
                uint8_t v = e.u8();
                e.skip(3);
                uint32_t n = e.u32();
                for (uint32_t i = 0; i < n; ++i) {
                    uint64_t dur = v == 1 ? e.u64() : e.u32();
                    int64_t mt = v == 1 ? int64_t(e.u64()) : int64_t(int32_t(e.u32()));
                    e.skip(4);   // rate
                    if (mt == -1) {
                        t.src_empty = add(t.src_empty, dur);
                        continue;
                    }
                    if (mt < 0) tfail(t, "bad edit list");
                    t.src_media_time = mt;
                    break;   // later edits (rare: trims, repeats) are not reproduced
                }
            }
        }
    }
    if (!tkhd || !mdia) tfail(t, "incomplete trak");
}

void parse_moov(Track &t, Cur c) {
    Cur b;
    std::vector<uint32_t> trex;
    for (uint32_t type; child(c, type, b);) {
        if (type == fc("mvhd")) {
            uint8_t v = b.u8();
            b.skip(3);
            b.skip(v == 1 ? 16 : 8);
            t.movie_timescale = b.u32();
        } else if (type == fc("trak")) {
            if (++t.traks > 1) tfail(t, "more than one track in the file");
            parse_trak(t, b);
        } else if (type == fc("pssh")) {
            tfail(t, "encrypted (pssh)");
        } else if (type == fc("mvex")) {
            Cur x;
            for (uint32_t xt; child(b, xt, x);) {
                if (xt != fc("trex")) continue;
                x.skip(4);
                for (int i = 0; i < 5; ++i) trex.push_back(x.u32());
            }
        }
    }
    if (t.traks == 0) tfail(t, "no track");
    for (size_t i = 0; i + 5 <= trex.size(); i += 5)
        if (trex[i] == t.id) t.d_sdi = trex[i + 1], t.d_dur = trex[i + 2], t.d_size = trex[i + 3], t.d_flags = trex[i + 4];
}

bool is_sync(uint32_t flags) { return ((flags >> 24) & 3) == 2 || !(flags & 0x10000); }

void parse_moof(Track &t, Cur c, uint64_t moof_off, uint64_t fsize) {
    uint64_t implicit = moof_off;   // data base of a traf without base flags (8.8.7.1)
    Cur traf;
    for (uint32_t type; child(c, type, traf);) {
        if (type != fc("traf")) continue;
        bool tfhd = false, first_trun = true;
        uint64_t base = 0, data_end = 0;
        uint32_t sdi = t.d_sdi, dur = t.d_dur, size = t.d_size, dflags = t.d_flags;
        Cur b;
        for (uint32_t bt; child(traf, bt, b);) {
            if (bt == fc("tfhd")) {
                uint32_t f = b.u32() & 0xFFFFFF;
                if (b.u32() != t.id) tfail(t, "fragment of another track");
                base = (f & 0x20000) ? moof_off : implicit;
                if (f & 0x1) {
                    uint64_t v = b.u64();
                    if (v) base = v;   // bbtube: some muxers write 0 here meaning "unset"
                }
                if (f & 0x2) sdi = b.u32();
                if (f & 0x8) dur = b.u32();
                if (f & 0x10) size = b.u32();
                if (f & 0x20) dflags = b.u32();
                tfhd = true;
                data_end = base;
            } else if (bt == fc("tfdt")) {
                uint8_t v = b.u8();
                b.skip(3);
                uint64_t dts = v == 1 ? b.u64() : b.u32();
                if (t.s.empty()) {
                    t.first_dts = t.next_dts = dts;
                } else if (dts != t.next_dts) {
                    // A gap or overlap: the previous sample absorbs it, so later decode times
                    // stay those of the source. One that cannot be absorbed is ignored.
                    Sample &last = t.s.back();
                    uint64_t start = t.next_dts - last.dur;
                    if (dts >= start && dts - start <= 0xFFFFFFFFu) {
                        last.dur = uint32_t(dts - start);
                        t.next_dts = dts;
                    }
                }
            } else if (bt == fc("trun")) {
                if (!tfhd) tfail(t, "trun before tfhd");
                if (sdi == 0 || sdi > t.stsd_count) tfail(t, "bad sample description index");
                uint32_t vf = b.u32(), f = vf & 0xFFFFFF;
                uint32_t n = b.u32();
                uint64_t pos = first_trun ? base : data_end;
                if (f & 0x1) {
                    int64_t off = int32_t(b.u32());
                    if (off < 0 && uint64_t(-off) > base) tfail(t, "bad data offset");
                    pos = off < 0 ? base - uint64_t(-off) : add(base, uint64_t(off));
                }
                bool first_flags = f & 0x4;
                uint32_t ff = first_flags ? b.u32() : 0;
                size_t per = 4 * ((f >> 8 & 1) + (f >> 9 & 1) + (f >> 10 & 1) + (f >> 11 & 1));
                if (n > kMaxSamples - t.s.size()) tfail(t, "more than 1000000 samples");
                if (per && mul(n, per) > b.left()) tfail(t, "trun box truncated");
                for (uint32_t i = 0; i < n; ++i) {
                    Sample s;
                    s.off = pos;
                    s.dur = (f & 0x100) ? b.u32() : dur;
                    s.size = (f & 0x200) ? b.u32() : size;
                    uint32_t sf = (f & 0x400) ? b.u32() : (i == 0 && first_flags) ? ff : dflags;
                    // Signed whatever the version, as ffmpeg reads it: some muxers write negative
                    // offsets in a v0 trun, and no real offset needs the 32nd bit.
                    s.cts = (f & 0x800) ? int32_t(b.u32()) : 0;
                    s.sdi = uint16_t(sdi);
                    s.sync = is_sync(sf);
                    pos = add(pos, s.size);
                    if (pos > fsize) tfail(t, "sample data beyond the end of the file (truncated?)");
                    t.next_dts = add(t.next_dts, s.dur);
                    t.s.push_back(s);
                }
                data_end = pos;
                first_trun = false;
            }
        }
        if (tfhd) implicit = data_end;
    }
}

void read_track(Track &t, int fd, uint64_t fsize, uint32_t want_handler) {
    std::vector<uint8_t> buf;
    bool moov = false;
    for (uint64_t off = 0; off < fsize;) {
        TopBox b = top_box(fd, off, fsize, t.what);
        if (b.type == fc("moov") || b.type == fc("moof")) {
            Cur c;
            load_box(fd, b, buf, c, t.what);
            if (b.type == fc("moov")) {
                if (moov) tfail(t, "two moov boxes");
                parse_moov(t, c);
                if (t.handler != want_handler)
                    tfail(t, "track is " + name(t.handler) + ", not " + name(want_handler));
                if (t.moov_samples) tfail(t, "samples in moov as well as in fragments (unsupported)");
                moov = true;
            } else {
                if (!moov) tfail(t, "moof before moov");
                parse_moof(t, c, b.off, fsize);
            }
        }
        if (b.truncated) break;   // a cut mdat: its samples fail the end-of-file check
        off += b.size;
    }
    if (t.s.empty()) tfail(t, "no samples");
}

// ---- the output -------------------------------------------------------------------------------

struct W {
    std::vector<uint8_t> b;
    void u8(uint8_t v) { b.push_back(v); }
    void u16(uint16_t v) { u8(uint8_t(v >> 8)); u8(uint8_t(v)); }
    void u32(uint32_t v) { u16(uint16_t(v >> 16)); u16(uint16_t(v)); }
    void u64(uint64_t v) { u32(uint32_t(v >> 32)); u32(uint32_t(v)); }
    void zeros(size_t n) { b.insert(b.end(), n, 0); }
    void bytes(const std::vector<uint8_t> &v) { b.insert(b.end(), v.begin(), v.end()); }
    size_t open(uint32_t type) {
        size_t at = b.size();
        u32(0);
        u32(type);
        return at;
    }
    size_t full(uint32_t type, uint8_t version, uint32_t flags) {
        size_t at = open(type);
        u32(uint32_t(version) << 24 | flags);
        return at;
    }
    void close(size_t at) {
        uint64_t n = b.size() - at;
        if (n > 0xFFFFFFFFu) fail("moov too large");
        for (int i = 0; i < 4; ++i) b[at + i] = uint8_t(n >> (24 - 8 * i));
    }
};

struct Chunk {
    uint32_t first, count;   // samples of that track
    uint64_t off;            // in the output file
    uint8_t track;
};

struct Out {   // what the moov says about one track
    uint64_t media_dur = 0;   // media timescale
    uint32_t shift = 0;       // added to every composition offset (ctts v0)
    uint64_t empty_ms = 0;    // empty edit, movie timescale
    uint64_t media_time = 0;  // first media edit's start, media timescale
    uint64_t seg_ms = 0;      // first media edit's length
    bool elst = false;
    uint64_t tkhd_dur() const { return empty_ms + seg_ms; }
};

uint64_t ceil_div(uint64_t a, uint64_t b) { return a / b + (a % b != 0); }

Out plan(const Track &t) {
    Out o;
    int32_t min_cts = 0;
    for (const auto &s : t.s) {
        o.media_dur += s.dur;
        min_cts = std::min(min_cts, s.cts);
    }
    o.shift = uint32_t(-int64_t(min_cts));
    // End of the composition timeline (with the shift): B-frame delay runs past media_dur.
    uint64_t comp_end = 0, dts = 0;
    for (const auto &s : t.s) {
        comp_end = std::max(comp_end, uint64_t(int64_t(dts) + s.cts + o.shift) + s.dur);
        dts += s.dur;
    }
    if (comp_end == 0) tfail(t, "zero duration");
    // Where the first sample sits on the presentation timeline, in media ticks: source empty
    // edits + first tfdt - source media_time. Negative: the start is cut (priming).
    if (t.src_empty && !t.movie_timescale) tfail(t, "zero movie timescale");
    uint64_t empty = t.src_empty ? mul(t.src_empty, t.timescale) / t.movie_timescale : 0;
    uint64_t ahead = add(empty, t.first_dts);
    uint64_t mt = uint64_t(t.src_media_time);
    uint64_t cut = 0;
    if (ahead >= mt) {
        o.empty_ms = add(mul(ahead - mt, kMovieTimescale), t.timescale / 2) / t.timescale;
    } else {
        cut = mt - ahead;
    }
    o.media_time = add(cut, o.shift);
    if (o.media_time >= comp_end) tfail(t, "edit list starts after the end of the media");
    o.seg_ms = ceil_div(mul(comp_end - o.media_time, kMovieTimescale), t.timescale);
    o.elst = o.empty_ms || o.media_time;
    return o;
}

// Presentation-aligned decode time of a sample, in chunk slots.
int64_t slot(const Track &t, const Out &o, uint64_t dts) {
    double sec = (double(dts) - double(o.media_time)) / t.timescale + double(o.empty_ms) / kMovieTimescale;
    double k = std::floor(sec / kChunkSeconds);
    return k < 0 ? 0 : k > 1e15 ? int64_t(1e15) : int64_t(k);
}

std::vector<Chunk> make_chunks(const std::vector<const Track *> &tr, const std::vector<Out> &out) {
    std::vector<Chunk> ch;
    size_t i[2] = {0, 0};
    uint64_t dts[2] = {0, 0};
    for (;;) {
        int pick = -1;
        int64_t best = 0;
        for (int k = 0; k < int(tr.size()); ++k) {
            if (i[k] >= tr[k]->s.size()) continue;
            int64_t sl = slot(*tr[k], out[k], dts[k]);
            if (pick < 0 || sl < best) pick = k, best = sl;
        }
        if (pick < 0) break;
        const auto &s = tr[pick]->s;
        Chunk c{uint32_t(i[pick]), 0, 0, uint8_t(pick)};
        uint16_t sdi = s[i[pick]].sdi;
        while (i[pick] < s.size() && s[i[pick]].sdi == sdi && slot(*tr[pick], out[pick], dts[pick]) == best) {
            dts[pick] += s[i[pick]].dur;
            ++i[pick];
        }
        c.count = uint32_t(i[pick] - c.first);
        ch.push_back(c);
    }
    return ch;
}

void write_trak(W &w, const Track &t, const Out &o, uint32_t id, const std::vector<Chunk> &chunks,
                int index, bool co64) {
    const bool video = t.handler == fc("vide");
    size_t trak = w.open(fc("trak"));

    uint64_t td = o.tkhd_dur();
    bool v1 = td > 0xFFFFFFFFu;
    size_t tkhd = w.full(fc("tkhd"), v1, 3);   // enabled | in movie
    if (v1) w.zeros(16); else w.zeros(8);      // creation, modification
    w.u32(id);
    w.u32(0);
    if (v1) w.u64(td); else w.u32(uint32_t(td));
    w.zeros(8);
    w.u16(0);                  // layer
    w.u16(0);                  // alternate group
    w.u16(video ? 0 : 0x100);  // volume
    w.u16(0);
    for (uint32_t m : t.matrix) w.u32(m);
    w.u32(video ? t.width : 0);
    w.u32(video ? t.height : 0);
    w.close(tkhd);

    if (o.elst) {
        bool ev1 = o.seg_ms > 0xFFFFFFFFu || o.empty_ms > 0xFFFFFFFFu || o.media_time > 0x7FFFFFFFu;
        if (o.media_time > uint64_t(std::numeric_limits<int64_t>::max())) tfail(t, "edit list out of range");
        size_t edts = w.open(fc("edts"));
        size_t elst = w.full(fc("elst"), ev1, 0);
        w.u32(o.empty_ms ? 2 : 1);
        auto entry = [&](uint64_t dur, int64_t time) {
            if (ev1) {
                w.u64(dur);
                w.u64(uint64_t(time));
            } else {
                w.u32(uint32_t(dur));
                w.u32(uint32_t(time));
            }
            w.u32(0x10000);   // rate 1.0
        };
        if (o.empty_ms) entry(o.empty_ms, -1);
        entry(o.seg_ms, int64_t(o.media_time));
        w.close(elst);
        w.close(edts);
    }

    size_t mdia = w.open(fc("mdia"));
    bool mv1 = o.media_dur > 0xFFFFFFFFu;
    size_t mdhd = w.full(fc("mdhd"), mv1, 0);
    if (mv1) w.zeros(16); else w.zeros(8);
    w.u32(t.timescale);
    if (mv1) w.u64(o.media_dur); else w.u32(uint32_t(o.media_dur));
    w.u16(t.language);
    w.u16(0);
    w.close(mdhd);
    size_t hdlr = w.open(fc("hdlr"));
    w.bytes(t.hdlr);
    w.close(hdlr);

    size_t minf = w.open(fc("minf"));
    if (video) {
        size_t b = w.full(fc("vmhd"), 0, 1);
        w.zeros(8);
        w.close(b);
    } else {
        size_t b = w.full(fc("smhd"), 0, 0);
        w.zeros(4);
        w.close(b);
    }
    size_t dinf = w.open(fc("dinf"));
    size_t dref = w.full(fc("dref"), 0, 0);
    w.u32(1);
    size_t url = w.full(fc("url "), 0, 1);   // self-contained
    w.close(url);
    w.close(dref);
    w.close(dinf);

    size_t stbl = w.open(fc("stbl"));
    size_t stsd = w.open(fc("stsd"));
    w.bytes(t.stsd);
    w.close(stsd);

    const auto &s = t.s;
    {   // stts
        size_t b = w.full(fc("stts"), 0, 0), cnt = w.b.size();
        w.u32(0);
        uint32_t n = 0;
        for (size_t i = 0; i < s.size();) {
            size_t j = i;
            while (j < s.size() && s[j].dur == s[i].dur) ++j;
            w.u32(uint32_t(j - i));
            w.u32(s[i].dur);
            ++n;
            i = j;
        }
        for (int k = 0; k < 4; ++k) w.b[cnt + k] = uint8_t(n >> (24 - 8 * k));
        w.close(b);
    }
    if (video && std::any_of(s.begin(), s.end(), [](const Sample &x) { return !x.sync; })) {
        size_t b = w.full(fc("stss"), 0, 0), cnt = w.b.size();
        w.u32(0);
        uint32_t n = 0;
        for (size_t i = 0; i < s.size(); ++i)
            if (s[i].sync) w.u32(uint32_t(i + 1)), ++n;
        for (int k = 0; k < 4; ++k) w.b[cnt + k] = uint8_t(n >> (24 - 8 * k));
        w.close(b);
    }
    if (std::any_of(s.begin(), s.end(), [&](const Sample &x) { return int64_t(x.cts) + o.shift != 0; })) {
        size_t b = w.full(fc("ctts"), 0, 0), cnt = w.b.size();
        w.u32(0);
        uint32_t n = 0;
        for (size_t i = 0; i < s.size();) {
            size_t j = i;
            while (j < s.size() && s[j].cts == s[i].cts) ++j;
            w.u32(uint32_t(j - i));
            w.u32(uint32_t(int64_t(s[i].cts) + o.shift));
            ++n;
            i = j;
        }
        for (int k = 0; k < 4; ++k) w.b[cnt + k] = uint8_t(n >> (24 - 8 * k));
        w.close(b);
    }
    {   // stsc: a new entry when samples-per-chunk or the sample description changes
        size_t b = w.full(fc("stsc"), 0, 0), cnt = w.b.size();
        w.u32(0);
        uint32_t n = 0, num = 0, last_count = 0, last_sdi = 0;
        for (const auto &c : chunks) {
            if (c.track != index) continue;
            ++num;
            uint16_t sdi = s[c.first].sdi;
            if (c.count != last_count || sdi != last_sdi) {
                w.u32(num);
                w.u32(c.count);
                w.u32(sdi);
                ++n;
                last_count = c.count;
                last_sdi = sdi;
            }
        }
        for (int k = 0; k < 4; ++k) w.b[cnt + k] = uint8_t(n >> (24 - 8 * k));
        w.close(b);
    }
    {
        size_t b = w.full(fc("stsz"), 0, 0);
        w.u32(0);
        w.u32(uint32_t(s.size()));
        for (const auto &x : s) w.u32(x.size);
        w.close(b);
    }
    {
        size_t b = w.full(co64 ? fc("co64") : fc("stco"), 0, 0), cnt = w.b.size();
        w.u32(0);
        uint32_t n = 0;
        for (const auto &c : chunks) {
            if (c.track != index) continue;
            if (co64) w.u64(c.off); else w.u32(uint32_t(c.off));
            ++n;
        }
        for (int k = 0; k < 4; ++k) w.b[cnt + k] = uint8_t(n >> (24 - 8 * k));
        w.close(b);
    }
    w.close(stbl);
    w.close(minf);
    w.close(mdia);
    w.close(trak);
}

std::vector<uint8_t> head(const std::vector<const Track *> &tr, const std::vector<Out> &out,
                          std::vector<Chunk> &chunks, uint64_t payload, bool co64, uint64_t &mdat_start) {
    W w;
    size_t ftyp = w.open(fc("ftyp"));
    w.u32(fc("isom"));
    w.u32(0x200);
    for (uint32_t b : {fc("isom"), fc("iso2"), fc("avc1"), fc("mp41")}) w.u32(b);
    w.close(ftyp);

    size_t moov = w.open(fc("moov"));
    uint64_t dur = 0;
    for (const auto &o : out) dur = std::max(dur, o.tkhd_dur());
    bool v1 = dur > 0xFFFFFFFFu;
    size_t mvhd = w.full(fc("mvhd"), v1, 0);
    if (v1) w.zeros(16); else w.zeros(8);
    w.u32(kMovieTimescale);
    if (v1) w.u64(dur); else w.u32(uint32_t(dur));
    w.u32(0x10000);   // rate 1.0
    w.u16(0x100);     // volume 1.0
    w.zeros(10);
    for (uint32_t m : {0x10000u, 0u, 0u, 0u, 0x10000u, 0u, 0u, 0u, 0x40000000u}) w.u32(m);
    w.zeros(24);
    w.u32(uint32_t(tr.size() + 1));   // next track ID
    w.close(mvhd);
    for (size_t k = 0; k < tr.size(); ++k) write_trak(w, *tr[k], out[k], uint32_t(k + 1), chunks, int(k), co64);
    w.close(moov);

    bool big = payload > 0xFFFFFFFFu - 8;
    mdat_start = w.b.size() + (big ? 16 : 8);
    if (big) {
        w.u32(1);
        w.u32(fc("mdat"));
        w.u64(payload + 16);
    } else {
        w.u32(uint32_t(payload + 8));
        w.u32(fc("mdat"));
    }
    return std::move(w.b);
}

void copy_range(int in, int out, uint64_t off, uint64_t len, std::vector<uint8_t> &buf, const char *what) {
    while (len) {
        size_t n = size_t(std::min<uint64_t>(len, buf.size()));
        pread_all(in, buf.data(), n, off, what);
        write_all(out, buf.data(), n);
        off += n;
        len -= n;
    }
}

}  // namespace

bool is_fragmented(const std::string &path) {
    try {
        Fd f;
        uint64_t size = open_input(f, path, "input");
        return probe(f.fd, size, "input") == 'F';
    } catch (...) {
        return false;
    }
}

bool remux(const std::string &video_path, const std::string &audio_path, const std::string &out_path,
           RemuxInfo &info, std::string &err) {
    info = RemuxInfo();
    err.clear();
    std::string tmp = out_path + ".tmp";
    bool created = false;
    try {
        Fd vf, af;
        Track v, a;
        v.what = "video";
        a.what = "audio";
        uint64_t vsize = open_input(vf, video_path, "video");
        if (probe(vf.fd, vsize, "video") != 'F') {
            err = "not fragmented";
            return false;
        }
        read_track(v, vf.fd, vsize, fc("vide"));
        std::vector<const Track *> tr{&v};
        if (!audio_path.empty()) {
            uint64_t asize = open_input(af, audio_path, "audio");
            if (probe(af.fd, asize, "audio") != 'F') fail("audio: not fragmented");
            read_track(a, af.fd, asize, fc("soun"));
            tr.push_back(&a);
        }

        std::vector<Out> out;
        uint64_t payload = 0;
        for (const Track *t : tr) {
            out.push_back(plan(*t));
            for (const auto &s : t->s) payload = add(payload, s.size);
        }
        std::vector<Chunk> chunks = make_chunks(tr, out);
        auto place = [&](uint64_t start) {
            for (auto &c : chunks) {
                c.off = start;
                const auto &s = tr[c.track]->s;
                for (uint32_t i = 0; i < c.count; ++i) start += s[c.first + i].size;
            }
        };
        // The moov's size depends only on stco vs co64: measure, place the chunks, write.
        uint64_t mdat_start = 0;
        bool co64 = false;
        head(tr, out, chunks, payload, co64, mdat_start);
        place(mdat_start);
        if (!chunks.empty() && chunks.back().off > 0xFFFFFFFFu) {
            co64 = true;
            head(tr, out, chunks, payload, co64, mdat_start);
            place(mdat_start);
        }
        std::vector<uint8_t> h = head(tr, out, chunks, payload, co64, mdat_start);

        Fd of;
        of.fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (of.fd < 0) fail("cannot create " + tmp + ": " + std::strerror(errno));
        created = true;
        write_all(of.fd, h.data(), h.size());
        std::vector<uint8_t> buf(kCopyBuffer);
        for (const auto &c : chunks) {
            const Track &t = *tr[c.track];
            int in = c.track == 0 ? vf.fd : af.fd;
            for (uint32_t i = c.first, end = c.first + c.count; i < end;) {
                uint64_t off = t.s[i].off, len = 0;
                while (i < end && t.s[i].off == off + len) len += t.s[i++].size;
                copy_range(in, of.fd, off, len, buf, t.what);
            }
        }
        int fd = of.fd;
        of.fd = -1;
        if (::close(fd) != 0) fail(std::string("write error: ") + std::strerror(errno));
        if (::rename(tmp.c_str(), out_path.c_str()) != 0)
            fail("cannot rename to " + out_path + ": " + std::strerror(errno));
        created = false;

        info.has_audio = tr.size() > 1;
        info.bytes = mdat_start + payload;
        uint64_t dur = 0;
        for (const auto &o : out) dur = std::max(dur, o.tkhd_dur());
        info.duration_s = double(dur) / kMovieTimescale;
        // What a player shows (see remux.h): coded size from the sample entry, pasp on the width.
        info.width = v.entry_w;
        info.height = v.entry_h;
        if (v.pasp_h && v.pasp_v && v.pasp_h != v.pasp_v)
            info.width = int((uint64_t(v.entry_w) * v.pasp_h + v.pasp_v / 2) / v.pasp_v);
        if (!info.width || !info.height) info.width = int(v.width >> 16), info.height = int(v.height >> 16);
        return true;
    } catch (const Error &e) {
        err = e.msg;
    } catch (const std::bad_alloc &) {
        err = "out of memory";
    } catch (...) {
        err = "unexpected error";
    }
    if (created) ::unlink(tmp.c_str());
    info = RemuxInfo();
    return false;
}

}  // namespace phx::mp4
