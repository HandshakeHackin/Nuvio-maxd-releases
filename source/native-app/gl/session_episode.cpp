#include "session.hpp"
#include "http.h"
#include "providers.h"
#include <algorithm>
#include <cmath>

namespace sx {
void Session::watch_load_segments(int season, int episode) {
    w_anime_ = {}; w_anime_requested_ = false;
    if (!prefs_.skip_segments) return;
    const bool movie = w_item_.type == "movie";
    // Anime addons number each entry from 1; IntroDB needs the IMDb numbering.
    int imdb_season = season, imdb_episode = episode;
    for (const auto& v : d_meta_.videos)
        if (v.id == w_video_id_ && v.imdb_season > 0 && v.imdb_episode > 0) { imdb_season = v.imdb_season; imdb_episode = v.imdb_episode; }
    const std::vector<std::string> ids{d_meta_.imdb_id, d_meta_.id, w_item_.id, w_video_id_};
    const auto url = episode_playback::timing_url(ids, imdb_season, imdb_episode, movie);
    const auto anime = movie || episode <= 0 ? std::nullopt : episode_playback::anime_source(ids);
    const auto arm = anime ? episode_playback::arm_ids_url(*anime) : movie ? std::string() : episode_playback::arm_imdb_url(ids);
    if (url.empty() && arm.empty()) { dlog("episode timings: no IMDb or anime episode identity"); return; }
    const int gen = w_gen_;
    const auto cancel = w_episode_cancel_;
    struct Found { std::vector<episode_playback::Segment> segments; episode_playback::AnimeIds anime; };
    bg<Found>([url, arm, by_id = anime.has_value(), episode, imdb_season, imdb_episode, cancel] {
        Found found;
        if (!url.empty()) {
            const auto response = http_get_https(url, 5, cancel.get());
            if (response.ok()) found.segments = episode_playback::parse_segments(json::parse(response.body, nullptr, false));
        }
        // An IMDb title is looked up as anime only when IntroDB has no intro for it.
        const bool needed = by_id || std::none_of(found.segments.begin(), found.segments.end(),
            [](const auto& s) { return s.kind == episode_playback::Kind::intro; });
        if (arm.empty() || !needed || (cancel && *cancel)) return found;
        const auto response = http_get_https(arm, 5, cancel.get());
        if (!response.ok()) return found;
        const auto data = json::parse(response.body, nullptr, false);
        found.anime = by_id ? episode_playback::parse_arm_ids(data, episode)
                            : episode_playback::pick_arm_season(data, imdb_season, imdb_episode);
        return found;
    }, [this, gen](auto& found) {
        if (!watching || gen != w_gen_) return;
        w_episode_.segments = std::move(found.segments);
        w_anime_ = found.anime;
        dlog("episode timings: IntroDB returned %zu segments%s", w_episode_.segments.size(),
             w_anime_.any() ? "; anime entry found" : "");
    });
}

void Session::watch_load_anime_segments(double duration) {
    w_anime_requested_ = true;
    const auto anime = w_anime_;
    const auto client_id = services_.animeskip.key;
    const int gen = w_gen_;
    const auto cancel = w_episode_cancel_;
    struct Found { std::vector<episode_playback::Segment> animeskip, aniskip; };
    bg<Found>([anime, client_id, duration, cancel] {
        Found found;
        if (!client_id.empty() && anime.anilist > 0) {
            // The client ID goes only to Anime-Skip, over HTTPS without redirects, and is never logged.
            const std::vector<std::string> headers{"X-Client-ID: " + client_id, "Accept: application/json"};
            const auto body = episode_playback::animeskip_shows_body(anime.anilist);
            const auto shows = http_provider_request(episode_playback::animeskip_url, &body, "application/json", headers, cancel.get());
            if (!shows.ok()) dlog("episode timings: Anime-Skip unavailable (HTTP %ld)", shows.status);
            else for (const auto& show : episode_playback::parse_animeskip_shows(json::parse(shows.body, nullptr, false))) {
                if (cancel && *cancel) break;
                const auto query = episode_playback::animeskip_episodes_body(show);
                const auto episodes = http_provider_request(episode_playback::animeskip_url, &query, "application/json", headers, cancel.get());
                if (!episodes.ok()) break;
                found.animeskip = episode_playback::parse_animeskip(json::parse(episodes.body, nullptr, false), anime.episode, duration);
                if (!found.animeskip.empty()) break;
            }
        }
        const auto url = episode_playback::aniskip_url(anime.mal, anime.episode, duration);
        if (!url.empty() && !(cancel && *cancel)) {
            // AniSkip answers 404 when it has nothing for this episode and length.
            const auto response = http_get_https(url, 5, cancel.get());
            if (response.ok()) found.aniskip = episode_playback::parse_aniskip(json::parse(response.body, nullptr, false), duration);
        }
        return found;
    }, [this, gen](auto& found) {
        if (!watching || gen != w_gen_) return;
        // Anime timings match this file's length; IntroDB keeps what they lack.
        w_episode_.segments = episode_playback::merge_segments({found.animeskip, found.aniskip, w_episode_.segments});
        dlog("episode timings: Anime-Skip returned %zu, AniSkip %zu; %zu segments in use",
             found.animeskip.size(), found.aniskip.size(), w_episode_.segments.size());
    });
}

void Session::watch_prefetch_next() {
    if (!w_next_ || !watching) return;
    next_preparation_cancel();
    w_next_requested_ = true;
    w_next_fetched_ = now_seconds();
    w_next_streams_.clear();
    w_next_pending_ = w_next_answered_ = 0;
    w_next_pick_ = -1;
    const int gen = w_gen_, search = ++w_next_generation_;
    const auto cancel = w_episode_cancel_;
    const auto id = w_next_->id, type = w_item_.type;
    std::vector<::Addon> sources;
    for (const auto &addon : addons_)
        for (auto source : providers::stream_sources(*addon, type, id))
            sources.push_back(std::move(source));
    for (const auto &source : sources)
    {
        ++w_next_pending_;
        const auto meta = d_meta_;
        auto work = [source, type, id, meta, cancel]
        { return providers::fetch(source, type, id, meta, cancel.get()).streams; };
        auto done = [this, gen, search](auto &streams)
        {
            if (!watching || gen != w_gen_ || search != w_next_generation_)
                return;
            ++w_next_answered_;
            for (auto& stream : streams)
                stream.debrid_resolvable = services_.can_resolve(stream);
            w_next_streams_.insert(w_next_streams_.end(), streams.begin(), streams.end());
            if (w_next_answered_ == w_next_pending_)
            {
                w_next_fetched_ = now_seconds();
                w_next_pick_ = episode_playback::select_stream(w_next_streams_, w_stream_);
                dlog("next episode: %zu streams from %d addons", w_next_streams_.size(), w_next_answered_);
            }
        };
        if (source.provider_repository)
            provider_tasks_.run<std::vector<::Stream>>(work, done);
        else
            bg<std::vector<::Stream>>(work, done);
    }
}

bool Session::watch_episode_update(double now, double position, double duration, bool advancing, bool ended) {
    const double elapsed = now - w_episode_tick_;
    w_episode_tick_ = now;
    if (!w_started_ || duration <= 0) return false;
    if (w_anime_.any() && !w_anime_requested_) watch_load_anime_segments(duration);
    if (w_next_ && position >= duration * 0.8 && !w_next_requested_) watch_prefetch_next();
    const bool complete = w_next_requested_ && w_next_answered_ >= w_next_pending_;
    const int pick = w_next_pick_;
    const bool fresh = now - w_next_fetched_ < 90;
    if (complete && fresh && pick >= 0 && !w_episode_.cancelled) next_preparation_update();
    w_episode_.set_countdown(prefs_.next_episode_seconds);
    w_episode_.update(position, duration, elapsed, advancing, w_next_.has_value(), prefs_.autoplay_next,
                      complete && fresh && pick >= 0, ended);
    if (w_episode_.next_visible && !w_next_requested_) watch_prefetch_next();
    // Signed links are refreshed if a long credit sequence outlives prefetch.
    if (w_episode_.next_visible && complete && !fresh && advancing && !w_episode_.cancelled) watch_prefetch_next();
    playing.skip_label.clear();
    if (const auto target = w_episode_.skip_target(duration)) {
        (void)target;
        const auto kind = w_episode_.segments[w_episode_.active].kind;
        playing.skip_label = kind == episode_playback::Kind::intro ? "Skip Intro" :
                             kind == episode_playback::Kind::recap ? "Skip Recap" : "Skip Credits";
    }
    playing.next_label.clear(); playing.next_status.clear();
    if (w_episode_.next_visible && !playing.episode_dismissed) {
        playing.next_label = "Next: S" + std::to_string(w_next_->season) + " E" + std::to_string(w_next_->episode);
        if (!w_next_->title.empty()) playing.next_label += "  " + w_next_->title;
        if (!complete || !fresh) playing.next_status = "Finding next episode streams...";
        else if (pick < 0) playing.next_status = "Choose a stream to keep your preferred quality";
        else if (!prefs_.autoplay_next || w_episode_.cancelled) playing.next_status = "Press Cross to play";
        else if (!advancing) playing.next_status = "Countdown paused";
        else playing.next_status = "Playing in " + std::to_string(static_cast<int>(std::ceil(std::max(0.0, w_episode_.remaining)))) + " s";
    }
    if (playing.skip_label.empty() && playing.next_label.empty()) playing.episode_focused = false;
    if (playing.skip_label.empty()) playing.episode_action = 1;
    else if (playing.next_label.empty()) playing.episode_action = 0;
    if (prefs_.autoplay_next && advancing && w_episode_.launch()) {
        watch_next_episode(); return true;
    }
    return false;
}

bool Session::watch_episode_input(const hui::InputFrame& in) {
    using hui::Action; using hui::Direction;
    const bool available = !playing.skip_label.empty() || !playing.next_label.empty();
    if (!available) return false;
    if (in.nav == Direction::down) { playing.episode_focused = true; return true; }
    if (!playing.episode_focused) return false;
    if (in.nav == Direction::up) { playing.episode_focused = false; return true; }
    if (in.nav == Direction::left || in.nav == Direction::right) {
        if (!playing.skip_label.empty() && !playing.next_label.empty()) playing.episode_action = 1 - playing.episode_action;
        return true;
    }
    if (in.is_pressed(Action::back)) {
        w_episode_.cancel(); playing.episode_dismissed = true; playing.episode_focused = false;
        next_preparation_cancel();
        playing.next_label.clear();
        say("Automatic next episode cancelled for this episode"); return true;
    }
    if (in.is_pressed(Action::confirm)) {
        if (playing.episode_action == 1 && !playing.next_label.empty()) watch_next_episode();
        else if (const auto target = w_episode_.skip_target(player_.duration())) {
            dlog("episode timings: skipping to %.2f s", *target);
            player_.seek(*target); w_episode_.skipped(); playing.skip_label.clear();
            playing.episode_focused = false;
        }
        return true;
    }
    return false;
}
} // namespace sx
