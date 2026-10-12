#include "episode_playback.hpp"
#include "stream_display.hpp"
#include <algorithm>
#include <cctype>
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
// AniSkip's own tolerance between a submission's episode length and the file.
constexpr double kLengthTolerance = 20;
bool length_matches(double length, double duration) {
    return !(length > 0) || !(duration > 0) || std::abs(length - duration) <= kLengthTolerance;
}
long positive_id(const std::string& text) {
    if (text.empty() || text.size() > 12 || text.find_first_not_of("0123456789") != std::string::npos) return 0;
    return std::stol(text);
}
std::string imdb_id(const std::vector<std::string>& ids) {
    for (auto id : ids) {
        id = lower(trim(id.substr(0, id.find(':'))));
        if (id.size() <= 2 || id.size() > 20 || id.substr(0, 2) != "tt" ||
            id.find_first_not_of("0123456789", 2) != std::string::npos) continue;
        return id;
    }
    return {};
}
bool has_kind(const std::vector<Segment>& segments, Kind kind) {
    return std::any_of(segments.begin(), segments.end(), [kind](const Segment& s) { return s.kind == kind; });
}
// Never seek past an explicitly identified scene inside the credits.
std::vector<Segment> finish(std::vector<Segment> result) {
    for (auto& credit : result) if (credit.kind == Kind::credits)
        for (const auto& scene : result) if (scene.kind == Kind::post_credits &&
            scene.start > credit.start && scene.start < credit.end) credit.end = scene.start;
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    return result;
}
bool safe_show_id(const std::string& id) {
    return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-';
    });
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
    return finish(std::move(result));
}

std::string timing_url(const std::vector<std::string>& ids, int season, int episode, bool movie) {
    if (!movie && (season <= 0 || episode <= 0)) return {};
    const auto id = imdb_id(ids);
    if (id.empty()) return {};
    std::string url = "https://api.introdb.app/segments?imdb_id=" + id;
    return movie ? url + "&is_movie=true" : url + "&season=" + std::to_string(season) + "&episode=" + std::to_string(episode);
}

std::optional<AnimeSource> anime_source(const std::vector<std::string>& ids) {
    for (const auto& raw : ids) {
        const auto colon = raw.find(':');
        if (colon == std::string::npos) continue;
        const auto prefix = lower(trim(raw.substr(0, colon)));
        const auto rest = raw.substr(colon + 1);
        const long id = positive_id(rest.substr(0, rest.find(':')));
        if (id <= 0) continue;
        if (prefix == "kitsu") return AnimeSource{"kitsu", id};
        if (prefix == "mal") return AnimeSource{"myanimelist", id};
        if (prefix == "anilist") return AnimeSource{"anilist", id};
    }
    return {};
}

std::string arm_ids_url(const AnimeSource& source) {
    if (source.id <= 0 || (source.source != "kitsu" && source.source != "myanimelist" && source.source != "anilist")) return {};
    return "https://arm.haglund.dev/api/v2/ids?source=" + source.source + "&id=" + std::to_string(source.id) +
           "&include=myanimelist,anilist";
}

std::string arm_imdb_url(const std::vector<std::string>& ids) {
    const auto id = imdb_id(ids);
    return id.empty() ? std::string() :
        "https://arm.haglund.dev/api/v2/imdb?id=" + id + "&include=myanimelist,anilist,thetvdb-season,media";
}

AnimeIds parse_arm_ids(const json& data, int episode) {
    if (!data.is_object() || episode <= 0) return {};
    return {positive_id(jstr(data, "myanimelist")), positive_id(jstr(data, "anilist")), episode};
}

AnimeIds pick_arm_season(const json& data, int season, int episode) {
    if (!data.is_array() || season <= 0) return {};
    const json* match = nullptr;
    for (const auto& entry : data) {
        const auto media = lower(jstr(entry, "media"));
        if (jnum(entry, "thetvdb-season", -1) != season || (media != "tv" && media != "ona")) continue;
        if (match) return {};
        match = &entry;
    }
    return match ? parse_arm_ids(*match, episode) : AnimeIds{};
}

std::string aniskip_url(long mal, int episode, double duration) {
    if (mal <= 0 || episode <= 0) return {};
    const long length = std::isfinite(duration) && duration > 0 && duration < 24 * 3600.0 ? std::lround(duration) : 0;
    return "https://api.aniskip.com/v2/skip-times/" + std::to_string(mal) + "/" + std::to_string(episode) +
           "?types=op&types=ed&types=recap&types=mixed-op&types=mixed-ed&episodeLength=" + std::to_string(length);
}

std::vector<Segment> parse_aniskip(const json& data, double duration) {
    std::vector<Segment> result;
    const auto& results = jobj(data, "results");
    if (!jbool(data, "found") || !results.is_array()) return result;
    // The server returns one submission per type, in the requested order, so a
    // plain opening or ending comes before its mixed variant.
    for (const auto& item : results) {
        const auto type = lower(jstr(item, "skipType"));
        const Kind kind = type == "op" || type == "mixed-op" ? Kind::intro :
                          type == "ed" || type == "mixed-ed" ? Kind::credits : Kind::recap;
        if ((kind == Kind::recap && type != "recap") || has_kind(result, kind)) continue;
        const auto& interval = jobj(item, "interval");
        const Segment s{kind, jnum(interval, "startTime", std::numeric_limits<double>::quiet_NaN()),
                        jnum(interval, "endTime", std::numeric_limits<double>::quiet_NaN())};
        if (valid(s, 24 * 3600.0) && length_matches(jnum(item, "episodeLength", 0), duration)) result.push_back(s);
    }
    return finish(std::move(result));
}

std::string animeskip_shows_body(long anilist) {
    if (anilist <= 0) return {};
    return json{{"query", "{ findShowsByExternalId(service: ANILIST, serviceId: \"" + std::to_string(anilist) +
                          "\") { id } }"}}.dump();
}

std::vector<std::string> parse_animeskip_shows(const json& data) {
    std::vector<std::string> shows;
    const auto& found = jobj(jobj(data, "data"), "findShowsByExternalId");
    if (!found.is_array()) return shows;
    for (const auto& show : found) {
        const auto id = jstr(show, "id");
        if (safe_show_id(id) && shows.size() < 3) shows.push_back(id);
    }
    return shows;
}

std::string animeskip_episodes_body(const std::string& show) {
    if (!safe_show_id(show)) return {};
    return json{{"query", "{ findEpisodesByShowId(showId: \"" + show +
                          "\") { season number baseDuration timestamps { at type { name } } } }"}}.dump();
}

std::vector<Segment> parse_animeskip(const json& data, int episode, double duration) {
    const auto& episodes = jobj(jobj(data, "data"), "findEpisodesByShowId");
    if (!episodes.is_array() || episode <= 0) return {};
    const json* match = nullptr;
    for (const auto& item : episodes) {
        if (trim(jstr(item, "number")) != std::to_string(episode)) continue;
        if (match) return {}; // the same number in several seasons is ambiguous
        match = &item;
    }
    if (!match) return {};
    const double base = jnum(*match, "baseDuration", 0);
    // Each timestamp starts a section that runs until the next one.
    std::vector<std::pair<double, std::string>> marks;
    const auto& timestamps = jobj(*match, "timestamps");
    if (timestamps.is_array()) for (const auto& t : timestamps) {
        const double at = jnum(t, "at", std::numeric_limits<double>::quiet_NaN());
        if (std::isfinite(at)) marks.emplace_back(at, lower(trim(jstr(jobj(t, "type"), "name"))));
    }
    std::stable_sort(marks.begin(), marks.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    // Timings come from one release. Other copies often lack its opening
    // branding, so a shorter file is first assumed to be missing that branding.
    double shift = 0;
    if (base > 0 && duration > 0 && duration < base && marks.size() > 1 && marks[0].first <= 0.5 && marks[0].second == "branding")
        shift = std::min(marks[1].first - marks[0].first, base - duration);
    if (!length_matches(base - shift, duration)) return {};
    const double last = base > 0 ? base : duration + shift;
    std::vector<Segment> result;
    for (std::size_t i = 0; i < marks.size(); ++i) {
        const auto& name = marks[i].second;
        const bool intro = name == "intro" || name == "new intro", credits = name == "credits";
        if (!intro && !credits && name != "recap") continue;
        const Kind kind = intro ? Kind::intro : credits ? Kind::credits : Kind::recap;
        const Segment s{kind, std::max(0.0, marks[i].first - shift), (i + 1 < marks.size() ? marks[i + 1].first : last) - shift};
        if (!has_kind(result, kind) && valid(s, 24 * 3600.0)) result.push_back(s);
    }
    return finish(std::move(result));
}

std::vector<Segment> merge_segments(const std::vector<std::vector<Segment>>& by_priority) {
    std::vector<Segment> result;
    for (const auto& list : by_priority) {
        std::vector<Segment> added;
        for (const auto& s : list)
            if (s.kind == Kind::post_credits || (!has_kind(result, s.kind) && !has_kind(added, s.kind))) added.push_back(s);
        result.insert(result.end(), added.begin(), added.end());
    }
    return finish(std::move(result));
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
