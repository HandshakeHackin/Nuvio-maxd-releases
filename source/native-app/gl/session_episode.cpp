#include "session.hpp"
#include "http.h"
#include "providers.h"
#include <algorithm>
#include <cmath>

namespace sx {
void Session::watch_load_segments(int season, int episode) {
    if (!prefs_.skip_segments) return;
    const bool movie = w_item_.type == "movie";
    const auto url = episode_playback::timing_url({d_meta_.imdb_id, d_meta_.id, w_item_.id, w_video_id_}, season, episode, movie);
    if (url.empty()) { dlog("episode timings: no IMDb episode identity"); return; }
    const int gen = w_gen_;
    const auto cancel = w_episode_cancel_;
    bg<std::vector<episode_playback::Segment>>([url, cancel] {
        const auto response = http_get_https(url, 5, cancel.get());
        if (!response.ok()) return std::vector<episode_playback::Segment>();
        return episode_playback::parse_segments(json::parse(response.body, nullptr, false));
    }, [this, gen](auto& segments) {
        if (!watching || gen != w_gen_) return;
        w_episode_.segments = std::move(segments);
        dlog("episode timings: IntroDB returned %zu segments", w_episode_.segments.size());
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
