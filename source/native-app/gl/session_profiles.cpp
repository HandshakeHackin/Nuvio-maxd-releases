// Nuvio account profiles: metadata and PINs use the account service; all local
// history and source switches are scoped to the selected account/profile.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "nuvio_account.h"
#include <algorithm>

namespace sx {
void Session::load_profile_settings() {
    prefs_.extra_addons.clear(); prefs_.provider_enabled = json::object(); prefs_.provider_settings=json::object();
    prefs_.subtitle_langs = "eng"; prefs_.audio_langs.clear(); prefs_.auto_subtitles = true;
    json j;
    load_json(profile_dir_ + "/profile-settings.json", j);
    if (jobj(j, "extra_addons").is_array())
        for (const auto& u : j["extra_addons"]) if (u.is_string()) prefs_.extra_addons.push_back(u.get<std::string>());
    if (jobj(j, "provider_enabled").is_object()) prefs_.provider_enabled = j["provider_enabled"];
    if (jobj(j,"provider_settings").is_object()) prefs_.provider_settings=j["provider_settings"];
    prefs_.subtitle_langs = jstr(j, "subtitle_languages", "eng");
    prefs_.audio_langs = jstr(j, "audio_languages"); prefs_.auto_subtitles = jbool(j, "auto_subtitles", true);
    if (inherit_plugins_) {
        const auto slash = profile_dir_.find_last_of('/');
        json primary;
        if (load_json(profile_dir_.substr(0, slash) + "/1/profile-settings.json", primary) &&
            jobj(primary, "provider_enabled").is_object()) prefs_.provider_enabled = primary["provider_enabled"];
        if(jobj(primary,"provider_settings").is_object()) prefs_.provider_settings=primary["provider_settings"];
    }
}
void Session::save_profile_settings() {
    // Before profile selection succeeds, never persist placeholder guest state
    // over the last selected account profile.
    if (!profile_ready_ || profile_dir_.empty()) return;
    json j;
    load_json(profile_dir_ + "/profile-settings.json", j);
    j["extra_addons"] = prefs_.extra_addons;
    if (!inherit_plugins_) {j["provider_enabled"] = prefs_.provider_enabled;j["provider_settings"]=prefs_.provider_settings;}
    j["subtitle_languages"] = prefs_.subtitle_langs;
    j["audio_languages"] = prefs_.audio_langs;
    j["auto_subtitles"] = prefs_.auto_subtitles;
    if (!save_json(profile_dir_ + "/profile-settings.json", j)) say("Could not save profile preferences", 8.0, true);
}
void Session::clear_profile_views() {
    stream_result_cache_.clear();
    profile_pin_cancel();
    ++profile_generation_; ++login_gen_; ++addons_gen_; ++home_gen_; ++disc_gen_;
    close_detail(); close_search(); next_preparation_cancel();
    if (launch_cancel_) *launch_cancel_ = true;
    if (w_episode_cancel_) *w_episode_cancel_ = true;
    cancel_autoplay(); resuming_ = false; open_req_.reset(); want_streams_video_.clear();
    tracking_status_ = json::object(); ++tracking_generation_; ++library_generation_;
    library_.clear(); progress_.clear(); watched_.clear(); addons_.clear(); board_.clear();
    library_loaded_ = false; library_loading_ = false; addons_loading_ = false;
    board_loading_ = false; disc_loading_ = false; disc_end_ = false;
    disc_items_.clear(); disc_types_.clear(); discover.items.clear();
    board.rows.clear(); board_map_.clear(); meta_cache_.clear(); meta_asked_.clear();
    disc_catalogs_.clear(); disc_type_.clear(); disc_genre_.clear();
    ++services_generation_; services_importing_ = false;
    if (device_cancel_) *device_cancel_ = true;
    device_provider_.clear();
    ++w_gen_; ++w_next_generation_; w_next_.reset(); w_next_streams_.clear(); w_next_requested_ = false;
    services_ = debrid::Settings{}; prepared_->configure(services_);
    active_profile_ = json::object(); profiles_ = json::array(); source_attempted_.clear();
    inherit_addons_ = inherit_plugins_ = false;
    prefs_.extra_addons.clear(); prefs_.provider_enabled = json::object(); prefs_.provider_settings=json::object();
    changed_ |= kBoard | kDiscover | kDiscoverNew | kLibrary | kLibraryNew | kAddons | kDetail;
}
void Session::profile_apply(const json& result) {
    clear_profile_views();
    profile_dir_ = jstr(result, "path"); make_dirs(profile_dir_);
    prefs_.auth_key = jstr(result, "authKey"); active_profile_ = jobj(result, "profile");
    profiles_ = jobj(result, "profiles");
    prefs_.user_email = jstr(result, "userEmail");
    inherit_addons_ = jbool(active_profile_, "uses_primary_addons");
    inherit_plugins_ = jbool(active_profile_, "uses_primary_plugins");
    profile_ready_ = true; profiles_loading_ = false;
    services_ = debrid::Settings{};
    debrid::load(profile_dir_ + "/connected-services.json", services_);
    const auto account = nuvio_account_user_id();
    if ((!services_.torbox.sync_owner.empty() && services_.torbox.sync_owner != account) ||
        (!services_.premiumize.sync_owner.empty() && services_.premiumize.sync_owner != account)) services_.clear_synced();
    prepared_->configure(services_);
    if (signed_in()) tracking_refresh();
    load_profile_settings(); load_progress(); refresh_library(); refresh_addons(); refresh_settings();
    changed_ |= kDiscover | kDiscoverNew | kSearch | kSearchNew;
    board.status = "Loading this profile's catalogs...";
    load_addons(); if (signed_in()) load_library();
}
void Session::profile_choose(int id, bool locked) {
    if (watching || resuming_) { say("Stop playback before switching profiles", 6.0); return; }
    const auto select = [this, id](const std::string& pin) {
        if (profiles_loading_) return;
        const auto key = prefs_.auth_key;
        const bool restore = profile_ready_;
        const json outgoing{{"path", profile_dir_}, {"authKey", key}, {"profile", active_profile_},
                            {"profiles", profiles_}, {"userEmail", prefs_.user_email}};
        // Save and hide the outgoing profile before the worker changes the
        // account context. The user cannot start an old stream in that gap.
        if (restore) { save_progress(); save_profile_settings(); }
        clear_profile_views(); profile_ready_ = false; profiles_loading_ = true;
        board.status = "Opening profile...";
        say("Opening profile...");
        bg<ApiResult>([key, id, pin] { return api_profile_select(key, id, pin); }, [this, restore, outgoing](ApiResult& r) {
            profiles_loading_ = false;
            if (!r.ok) {
                if (restore) profile_apply(outgoing);
                else board.status = "Choose a profile in Settings > Account & Services > Profiles.";
                say(r.error, 10.0, true); return;
            }
            profile_apply(r.result); say("Profile: " + jstr(active_profile_, "name"));
        });
    };
    if (locked) {
        profile_pin_cancel();
        profile_pin_submit_=select;
        pin_open=true;
    }
    else select("");
}
void Session::profile_pin_cancel() {
    pin_open=false;
    pin_entry.clear();
    profile_pin_submit_={};
}
void Session::profile_pin_input(const hui::InputFrame& input) {
    if (!pin_open) return;
    const auto result=pin_entry.input(input);
    if (result==PinEntry::Result::cancel) { profile_pin_cancel(); return; }
    if (result!=PinEntry::Result::submit) return;
    auto pin=pin_entry.take();
    pin_open=false;
    auto submit=std::move(profile_pin_submit_);
    profile_pin_submit_={};
    if (submit) submit(pin);
    volatile char* bytes=pin.data();
    for (std::size_t i=0;i<pin.size();++i) bytes[i]=0;
    pin.clear();
}
void Session::profiles_menu(bool startup) {
    if (!signed_in()) { say("Sign in to Nuvio to use account profiles", 6.0); return; }
    if (watching || resuming_) { say("Stop playback before switching profiles", 6.0); return; }
    if (profiles_loading_) return;
    profiles_loading_ = true;
    const auto key = prefs_.auth_key;
    bg<ApiResult>([key] { return api_profiles_get(key); }, [this, startup](ApiResult& r) {
        profiles_loading_ = false;
        if (!r.ok) { say(r.error + ". Open Settings > Account & Services > Profiles to retry.", 12.0, true); return; }
        profiles_ = jobj(r.result, "profiles");
        prefs_.auth_key = jstr(r.result, "authKey", prefs_.auth_key);
        std::vector<std::string> choices;
        int selected = 0;
        for (const auto& p : profiles_) {
            if (jnum(p, "profile_index") == jnum(active_profile_, "profile_index", jnum(jobj(r.result, "profile"), "profile_index"))) selected = int(choices.size());
            choices.push_back(jstr(p, "name") + (jbool(p, "pin_enabled") ? " (PIN)" : ""));
        }
        const auto snapshot = profiles_;
        const int count = int(choices.size());
        choices.push_back("Create profile"); choices.push_back("Cancel");
        open_dropdown(startup ? "Who's watching?" : "Nuvio profiles", choices, selected,
            [this, snapshot, count](int index) {
                if (index >= 0 && index < count) {
                    const auto& p = snapshot[static_cast<size_t>(index)];
                    profile_choose(int(jnum(p, "profile_index")), jbool(p, "pin_enabled"));
                } else if (index == count) {
                    say("Enter a name for the new profile", 8.0);
                    ask_text([this](const std::string& name) {
                        if (trim(name).empty()) return;
                        profiles_loading_ = true;
                        const auto key = prefs_.auth_key;
                        bg<ApiResult>([key, name] { return api_profile_create(key, name); }, [this](ApiResult& created) {
                            profiles_loading_ = false;
                            if (!created.ok) { say(created.error, 10.0, true); return; }
                            profile_choose(int(jnum(created.result, "createdProfile")), false);
                        }, true);
                    });
                }
            });
        dropdown.message = "History, addons and plugins belong to each profile. Shared sources follow your Nuvio profile settings.";
    }, true);
}
void Session::load_watched() {
    if (!signed_in() || !profile_ready_) return;
    const auto key = prefs_.auth_key;
    const auto revision = watched_revision_;
    bg<ApiResult>([key] { return api_watched_get(key); }, [this, revision](ApiResult& r) {
        if (revision != watched_revision_) return;
        if (!r.ok) { say(r.error, 8.0, true); return; }
        std::set<std::string> remote;
        for (const auto& entry : jobj(r.result, "entries")) {
            auto id = jstr(entry, "content_id");
            if (id.empty()) continue;
            if (jobj(entry, "season").is_number() && jobj(entry, "episode").is_number())
                id += ":" + std::to_string(int(jnum(entry, "season"))) + ":" + std::to_string(int(jnum(entry, "episode")));
            remote.insert(id);
        }
        watched_ = std::move(remote); save_progress();
        if (d_open_) detail_set_season(d_season_idx_);
    });
}
void Session::sync_watched_mark(const std::string& video, const std::string& title, bool watched,
                                const std::string& content, int season, int episode) {
    ++watched_revision_;
    if (!signed_in() || !profile_ready_) return;
    const auto key = prefs_.auth_key;
    bg<ApiResult>([key, video, title, watched, content, season, episode] { return api_watched_update(key, video, title, watched, content, season, episode); },
        [this](ApiResult& r) {
            if (!r.ok) say(r.error, 8.0, true);
            else if (!jbool(r.result, "synced")) say("Watched history saved on this PS5; Nuvio sync will retry.", 6.0);
        });
}
void Session::sync_source(const std::string& url, bool plugin, bool remove, bool automatic) {
    if (!signed_in()) return;
    if (automatic && source_attempted_.count(url)) return;
    source_attempted_.insert(url);
    const auto key = prefs_.auth_key;
    bg<ApiResult>([key, url, plugin, remove] { return api_profile_source_update(key, url, plugin, remove); },
        [this](ApiResult& r) {
            if (!r.ok) say(r.error, 10.0, true);
            else { say("Sources synced to this Nuvio profile"); load_addons(); }
        });
}
} // namespace sx
