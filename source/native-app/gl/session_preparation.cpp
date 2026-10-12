// Original-link preparation stays in the existing task pool; the UI never waits for it.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"

namespace sx {
std::pair<int, int> Session::detail_episode_identity() const {
    if (detail.series)
        for (const auto& v : d_meta_.videos)
            if (v.id == d_video_id_) return {v.season, v.episode};
    return {-1, -1};
}

void Session::detail_preparation_cancel() {
    prepared_->close_scope(d_prepare_scope_);
    d_prepare_scope_ = 0; d_prepare_busy_ = false;
    d_prepare_identity_.clear(); d_prepare_addon_.clear(); d_prepare_at_ = 0;
}

void Session::detail_preparation_labels() {
    const auto [season, episode] = detail_episode_identity();
    for (size_t i = 0; i < detail.streams.size() && i < d_streams_.size(); ++i) {
        const bool ready = prepared_->state(d_streams_[i], season, episode) == debrid::PreparedLinks::State::ready;
        if (ready != detail.streams[i].prepared) { detail.streams[i].prepared = ready; changed_ |= kDetail; }
    }
}

void Session::detail_preparation_update(const Cursor& cursor) {
    const double now = now_seconds();
    if (now >= d_prepare_labels_at_) { detail_preparation_labels(); d_prepare_labels_at_ = now + 1; }
    if (!d_open_ || watching || launch_open || !cursor.streams_open || services_.instant_limit <= 0 ||
        cursor.stream < 0 || cursor.stream >= static_cast<int>(d_streams_.size())) {
        if (d_prepare_scope_) detail_preparation_cancel();
        return;
    }
    const auto& identity = detail.streams[cursor.stream].identity;
    if (!d_prepare_scope_ || identity != d_prepare_identity_ || cursor.stream_addon != d_prepare_addon_) {
        detail_preparation_cancel();
        d_prepare_scope_ = prepared_->open_scope();
        d_prepare_identity_ = identity; d_prepare_addon_ = cursor.stream_addon;
        d_prepare_at_ = now + 0.3; // Allow navigation to settle before contacting the provider.
    }
    if (d_prepare_busy_ || now < d_prepare_at_) return;
    const auto candidates = debrid::preparation_candidates(d_streams_, services_, cursor.stream,
                                                         cursor.stream_addon, services_.instant_limit);
    const auto [season, episode] = detail_episode_identity();
    const auto manager = prepared_; const auto epoch = manager->epoch(), scope = d_prepare_scope_;
    d_prepare_at_ = now + 5;
    bool missing = false;
    for (const auto& s : candidates)
        missing |= manager->state(s, season, episode) == debrid::PreparedLinks::State::missing;
    if (!missing) return;
    d_prepare_busy_ = true;
    bg<bool>([manager, candidates, season, episode, epoch, scope] {
        for (const auto& s : candidates)
            manager->prepare(s, season, episode, scope, epoch);
        return true;
    }, [this, scope](bool&) {
        if (scope != d_prepare_scope_) return;
        d_prepare_busy_ = false; detail_preparation_labels();
    });
}

void Session::next_preparation_cancel() {
    prepared_->close_scope(w_next_prepare_scope_);
    w_next_prepare_scope_ = 0; w_next_prepare_busy_ = false; w_next_prepare_at_ = 0;
}

void Session::next_preparation_update() {
    if (!w_next_ || !watching || services_.instant_limit <= 0 || w_next_pick_ < 0 ||
        w_next_pick_ >= static_cast<int>(w_next_streams_.size()) || w_next_prepare_busy_ ||
        now_seconds() < w_next_prepare_at_) return;
    const auto stream = w_next_streams_[w_next_pick_];
    if (!services_.can_resolve(stream) || prepared_->state(stream, w_next_->season, w_next_->episode) != debrid::PreparedLinks::State::missing) return;
    if (!w_next_prepare_scope_) w_next_prepare_scope_ = prepared_->open_scope();
    const auto manager = prepared_; const auto epoch = manager->epoch(), scope = w_next_prepare_scope_;
    const int season = w_next_->season, episode = w_next_->episode;
    w_next_prepare_busy_ = true; w_next_prepare_at_ = now_seconds() + 5;
    bg<bool>([manager, stream, season, episode, scope, epoch] {
        manager->prepare(stream, season, episode, scope, epoch); return true;
    }, [this, scope](bool&) {
        if (scope == w_next_prepare_scope_) w_next_prepare_busy_ = false;
    });
}
} // namespace sx
