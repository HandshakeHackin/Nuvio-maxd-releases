// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "i18n.hpp"
#include <utility>

namespace sx {
void Session::live_refresh_reset() {
    live_refresh_timer_.reset();
    live_refresh_requested_=live_refresh_inflight_=live_refresh_failed_=false;
}
void Session::live_refresh_load() {
    const int hours=live_refresh::hours(live_state_);
    live_state_[live_refresh::preference_key]=hours;
    live_refresh_timer_.configure(hours,now_seconds());
}
bool Session::live_refresh_blocked() const {
    return !profile_ready_ || backup_busy_ || watching || resuming_ || multiview_active ||
           launch_open || sign_in_open || dropdown_open || release_notes_open || typing();
}
bool Session::live_refresh_begin() {
    live_refresh_inflight_=std::exchange(live_refresh_requested_,false);
    return live_refresh_inflight_;
}
void Session::live_refresh_complete(live_refresh::Outcome outcome) {
    live_refresh_inflight_=false;
    live_refresh_timer_.finished(now_seconds(),outcome);
    const bool failed=outcome==live_refresh::Outcome::failure;
    if (outcome!=live_refresh::Outcome::deferred && failed!=live_refresh_failed_) {
        live_refresh_failed_=failed;
        refresh_settings();
    }
}
void Session::live_refresh_tick() {
    // A video or menu may open after the request started. Retire that automatic
    // download without changing its channels or guide; manual requests stay manual.
    if (live_refresh_blocked()) {
        if (live_refresh_inflight_ && live_cancel_) *live_cancel_=true;
        return;
    }
    if (live_loading_ || !live_refresh_timer_.due(now_seconds()) || live_source_.empty()) return;
    bool present=false;
    const auto sources=free_tv::sources(live_state_);
    if (live_source_==unified_tv::source_id) present=!sources.empty();
    else for (const auto& source:sources) if (jstr(source,"id")==live_source_) present=true;
    if (!present) return;
    live_refresh_requested_=true;
    live_reload();
}
void Session::live_refresh_menu() {
    if (!profile_ready_) { say(i18n::tr("Choose a profile first."),6); return; }
    const auto profile=profile_generation_;
    std::vector<std::string> labels;
    for (const auto& choice:live_refresh::choices) labels.emplace_back(i18n::tr(choice.label));
    open_dropdown(i18n::tr("Channel list auto-refresh"),labels,int(live_refresh::index(live_refresh::hours(live_state_))),
      [this,profile](int pick) {
        if (profile!=profile_generation_ || !profile_ready_ || backup_busy_ || pick<0 || pick>=int(live_refresh::choices.size())) return;
        const auto previous=live_state_;
        const int hours=live_refresh::choices[std::size_t(pick)].hours;
        live_state_[live_refresh::preference_key]=hours;
        if (!live_save()) { live_state_=previous; return; }
        if (live_refresh_inflight_) {
            if (live_cancel_) *live_cancel_=true;
            ++live_generation_;
            live_refresh_inflight_=false;
            live_loading_=false;
            live_build();
        }
        live_refresh_timer_.configure(hours,now_seconds());
        refresh_settings();
    });
    dropdown.message=i18n::tr("Refresh loaded free and personal channel lists while Max'd is open. Waits during playback and menus. Failed downloads keep the last working list. Guide updates are separate.");
    dropdown.wide=true;
}
} // namespace sx
