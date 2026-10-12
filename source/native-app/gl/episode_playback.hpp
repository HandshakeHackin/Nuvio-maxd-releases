// Episode timings are metadata, never inferred from the picture or audio.
#pragma once
#include "stremio.h"
#include <optional>

namespace sx::episode_playback {
enum class Kind { recap, intro, credits, post_credits };
struct Segment { Kind kind; double start, end; };
std::vector<Segment> parse_segments(const json& data);
std::string timing_url(const std::vector<std::string>& ids, int season, int episode, bool movie = false);
const Video* next_video(const std::vector<Video>& videos, const std::string& current, std::int64_t now_ms);
int select_stream(const std::vector<::Stream>& streams, const ::Stream& previous);

// Reset per episode. Seeking back can show skip buttons again; cancelling next
// episode disables automatic transitions for the rest of this episode, including EOF.
class Flow {
public:
    std::vector<Segment> segments;
    int active = -1;
    bool next_visible = false, cancelled = false;
    double remaining = 10;
    void set_countdown(int seconds) {
        const double value = seconds < 5 ? 5 : seconds > 120 ? 120 : seconds;
        if (value != countdown_) { countdown_ = value; remaining = value; counting_ = false; }
    }
    void update(double position, double duration, double elapsed, bool advancing,
                bool next_exists, bool autoplay, bool sources_ready, bool ended = false);
    std::optional<double> skip_target(double duration) const;
    void skipped() { suppressed_ = active; active = -1; }
    void cancel() { cancelled = true; remaining = countdown_; }
    bool launch() const { return counting_ && next_visible && !cancelled && remaining <= 0; }
private:
    double countdown_ = 10;
    int suppressed_ = -1;
    bool counting_ = false;
};
} // namespace sx::episode_playback
