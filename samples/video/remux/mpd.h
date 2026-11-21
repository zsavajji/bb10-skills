// A DASH manifest (v.redd.it's DASHPlaylist.mpd) reduced to what the player needs: the URL of
// each single-file rendition. A small hand-written tag scanner, no XML library.
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace phx::mp4 {

struct Rendition {
    std::string url;        // absolute: BaseURL resolved against the MPD's own url
    int width = 0, height = 0;   // 0 for audio
    int bandwidth = 0;      // bits per second
    std::string codecs;     // "avc1.4d401f", "mp4a.40.2"
};

struct Mpd {
    std::vector<Rendition> video, audio;   // in the document's order
    double duration_s = 0;                 // from mediaPresentationDuration, 0 if absent
};

// Never throws. false + err on text that is not an MPD or has no video Representation.
// Only the first Period is read; a Representation without a BaseURL of its own or of its
// AdaptationSet (SegmentTemplate/SegmentList streams) is skipped.
bool parse_mpd(std::string_view xml, std::string_view mpd_url, Mpd &out, std::string &err);

// The video whose shorter side is the greatest <= max_short_side ("480p": 854x480, or 480x854 for
// a portrait one); if every one is larger, the smallest.
// Only codecs starting "avc1" are eligible (the phone decodes H.264 only); nullptr if none.
const Rendition *pick_video(const Mpd &, int max_short_side);

// The audio with the greatest bandwidth <= max_bps; if every one is above, the lowest.
// Only codecs starting "mp4a"; nullptr if none.
const Rendition *pick_audio(const Mpd &, int max_bps);

}  // namespace phx::mp4
