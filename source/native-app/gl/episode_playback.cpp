#include "episode_playback.hpp"
#include "stream_display.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sx::episode_playback {
namespace {
double seconds(const json& segment, const char* sec, const char* ms) {
    double value = jnum(segment, sec, std::numeric_limits<double>::quiet_NaN());
    if (!std::isfinite(value)) value = jnum(segment, ms, value) / 1000;
    return value;
}
bool valid(const Segment& s, double duration) {
    return std::isfinite(s.start) && std::isfinite(s.end) && s.start >= 0 && s.end > s.start &&
           duration > 0 && s.start < duration && s.end <= duration + 1;
}
int resolution(const ::Stream& st) {
    const auto r = stream_display::resolution_of(st);
    return r == "4K" ? 2160 : r == "1440p" ? 1440 : r == "1080p" ? 1080 :
           r == "720p" ? 720 : r == "SD" ? 480 : 0;
}
bool same_quality(const ::Stream& candidate, const ::Stream& previous) {
    const auto source = stream_display::view_of(previous), target = stream_display::view_of(candidate);
    if (resolution(previous) && resolution(candidate) < resolution(previous)) return false;
    // Keep explicitly advertised source, colour, codec and audio traits. File
    // sizes vary by episode and cannot be used as a quality guarantee.
    for (const auto badge : source.badges) {
        if (badge == StreamBadge::uhd || badge == StreamBadge::full_hd || badge == StreamBadge::hd) continue;
        if (std::find(target.badges.begin(), target.badges.end(), badge) == target.badges.end()) return false;
    }
    return true;
}
} // namespace

std::vector<Segment> parse_segments(const json& data) {
    std::vector<Segment> result;
    if (!data.is_object()) return result;
    for (const auto& [key, kind] : {std::pair{"recap", Kind::recap}, {"intro", Kind::intro},
                                  {"outro", Kind::credits}, {"post_credits", Kind::post_credits}}) {
        const auto& raw = jobj(data, key);
        Segment s{kind, seconds(raw, "start_sec", "start_ms"), seconds(raw, "end_sec", "end_ms")};
        if (valid(s, 24 * 3600.0)) result.push_back(s);
    }
    // Never seek past an explicitly identified scene inside the credits.
    for (auto& credit : result) if (credit.kind == Kind::credits)
        for (const auto& scene : result) if (scene.kind == Kind::post_credits &&
            scene.start > credit.start && scene.start < credit.end) credit.end = scene.start;
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    return result;
}

std::string timing_url(const std::vector<std::string>& ids, int season, int episode, bool movie) {
    if (!movie && (season <= 0 || episode <= 0)) return {};
    for (auto id : ids) {
        id = lower(trim(id.substr(0, id.find(':'))));
        if (id.size() <= 2 || id.size() > 20 || id.substr(0, 2) != "tt" ||
            id.find_first_not_of("0123456789", 2) != std::string::npos) continue;
        std::string url = "https://api.introdb.app/segments?imdb_id=" + id;
        return movie ? url + "&is_movie=true" : url + "&season=" + std::to_string(season) + "&episode=" + std::to_string(episode);
    }
    return {};
}

const Video* next_video(const std::vector<Video>& videos, const std::string& current, std::int64_t now_ms) {
    const auto it = std::find_if(videos.begin(), videos.end(), [&](const auto& v) { return v.id == current; });
    if (it == videos.end()) return nullptr;
    const Video* next = nullptr;
    for (const auto& v : videos) {
        if (v.id.empty() || v.id == current || v.season <= 0 || v.episode <= 0 ||
            v.season < it->season || (v.season == it->season && v.episode <= it->episode)) continue;
        if (!next || v.season < next->season || (v.season == next->season && v.episode < next->episode)) next = &v;
    }
    if (!next) return nullptr;
    const auto released = iso8601_to_ms(next->released);
    // Do not jump over an unaired episode into a later one.
    return released > now_ms ? nullptr : next;
}

int select_stream(const std::vector<::Stream>& streams, const ::Stream& previous) {
    int pick = -1, best = -1;
    for (std::size_t i = 0; i < streams.size(); ++i) {
        const auto& candidate = streams[i];
        if ((!candidate.direct_http() && !candidate.debrid_resolvable) || !same_quality(candidate, previous)) continue;
        const bool group = !previous.binge_group.empty() && candidate.binge_group == previous.binge_group;
        const bool addon = candidate.addon == previous.addon;
        // Unknown metadata cannot justify a different release automatically.
        if (!group && (!addon || resolution(previous) == 0)) continue;
        const int score = (group ? 100 : 0) + (addon ? 10 : 0) + (resolution(candidate) == resolution(previous) ? 1 : 0);
        if (score > best) { best = score; pick = static_cast<int>(i); }
    }
    return pick;
}

void Flow::update(double position, double duration, double elapsed, bool advancing,
                  bool next_exists, bool autoplay, bool sources_ready, bool ended) {
    active = -1; next_visible = false;
    if (!std::isfinite(position) || !std::isfinite(duration) || duration <= 0 || position < 0) { counting_ = false; return; }
    double trigger = duration;
    double latest_credits_end = 0, scene_end = 0;
    bool credits = false;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const auto& s = segments[i];
        if (!valid(s, duration)) continue;
        if (s.kind != Kind::post_credits && position >= s.start && position < s.end - 0.5 && active < 0)
            active = static_cast<int>(i);
        if (s.kind == Kind::credits) {
            credits = true;
            trigger = std::min(trigger, s.start);
            latest_credits_end = std::max(latest_credits_end, s.end);
        }
    }
    for (const auto& s : segments) if (credits && s.kind == Kind::post_credits && valid(s, duration) && s.start >= trigger)
        scene_end = std::max(scene_end, s.end);
    if (scene_end > 0) trigger = std::max(trigger, scene_end);
    else if (credits && duration - latest_credits_end > 5) trigger = duration - 1;
    if (suppressed_ >= 0 && active != suppressed_) suppressed_ = -1;
    if (active == suppressed_) active = -1;
    next_visible = next_exists && (ended || (credits && position >= trigger));
    const bool count = next_visible && autoplay && sources_ready && !cancelled;
    if (!next_visible) remaining = countdown_;
    if (count && counting_ && advancing && std::isfinite(elapsed)) remaining -= std::clamp(elapsed, 0.0, 0.25);
    counting_ = count;
}

std::optional<double> Flow::skip_target(double duration) const {
    if (active < 0 || active >= static_cast<int>(segments.size()) || !valid(segments[active], duration)) return {};
    return std::min(segments[active].end, duration);
}
} // namespace sx::episode_playback
