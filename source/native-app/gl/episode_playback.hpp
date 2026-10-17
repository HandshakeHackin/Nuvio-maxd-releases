// Episode timings are metadata, never inferred from the picture or audio.
#pragma once
#include "stremio.h"
#include <optional>

namespace sx::episode_playback {
enum class Kind { recap, intro, credits, post_credits };
struct Segment { Kind kind; double start, end; };
std::vector<Segment> parse_segments(const json& data);
std::string timing_url(const std::vector<std::string>& ids, int season, int episode, bool movie = false);

// Anime timings are recorded against a file length and checked against the
// playing file, so for anime they take priority over IntroDB, which fills gaps.
// ARM (arm.haglund.dev) maps anime IDs without a key; AniSkip needs a MyAnimeList
// ID, Anime-Skip an AniList ID and the user's own client ID. Every request carries
// only public catalogue IDs.
struct AnimeSource { std::string source; long id = 0; }; // ARM source name and numeric ID
struct AnimeIds {
    long mal = 0, anilist = 0;
    int episode = 0; // numbered within the anime entry
    bool any() const { return (mal > 0 || anilist > 0) && episode > 0; }
};
std::optional<AnimeSource> anime_source(const std::vector<std::string>& ids);
std::string arm_ids_url(const AnimeSource& source);
std::string arm_imdb_url(const std::vector<std::string>& ids);
AnimeIds parse_arm_ids(const json& data, int episode);
// Uses a TVDB season only when exactly one TV entry covers it; split seasons
// cannot be numbered reliably without episode offsets.
AnimeIds pick_arm_season(const json& data, int season, int episode);
std::string aniskip_url(long mal, int episode, double duration);
std::vector<Segment> parse_aniskip(const json& data, double duration);
inline constexpr const char* animeskip_url = "https://api.anime-skip.com/graphql";
std::string animeskip_shows_body(long anilist);
std::vector<std::string> parse_animeskip_shows(const json& data);
std::string animeskip_episodes_body(const std::string& show);
std::vector<Segment> parse_animeskip(const json& data, int episode, double duration);
// One segment per kind; earlier lists win. Post-credit scenes are always kept.
std::vector<Segment> merge_segments(const std::vector<std::vector<Segment>>& by_priority);
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
