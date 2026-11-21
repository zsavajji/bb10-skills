// Fragmented MP4 (DASH: ftyp moov sidx (moof mdat)...) -> one progressive MP4 (ftyp moov mdat),
// stream copy, no decoding. The phone's player (mm-renderer) plays a fragmented file but has no
// duration and cannot seek in it, and it takes a single source, so v.redd.it's separate video
// and audio files are merged here into one file it can seek in.
#pragma once
#include <cstdint>
#include <string>

namespace phx::mp4 {

struct RemuxInfo {
    // The size a player shows: the coded size of the sample entry (avc1), the width scaled by the
    // entry's pixel aspect (pasp) when that is not square, rounded. Read from the sample entry
    // rather than tkhd because tkhd is the packager's own rounding (and may be absent or 0). Example:
    // v.redd.it p5rowtg41iub1 DASH_480 codes 592x480 with pasp 3986:3991, so it is 591x480 here
    // (tkhd says 591.258); ffprobe's "width" is the coded 592, its sample_aspect_ratio the pasp.
    int width = 0, height = 0;
    double duration_s = 0;
    bool has_audio = false;
    uint64_t bytes = 0;      // size of the file written
};

// True if the file is an MP4 with movie fragments (moov has mvex, or a moof follows). Never throws.
bool is_fragmented(const std::string &path);

// video_path: a fragmented MP4 with one video track. audio_path: "" or a fragmented MP4 with one
// audio track. Writes a progressive MP4 to out_path (via out_path + ".tmp" and rename; the tmp
// file is removed on failure). Never throws, never crashes on hostile input: false + err.
// err is exactly "not fragmented" when the video file is a plain MP4 (play that file as it is).
bool remux(const std::string &video_path, const std::string &audio_path,
           const std::string &out_path, RemuxInfo &info, std::string &err);

}  // namespace phx::mp4
