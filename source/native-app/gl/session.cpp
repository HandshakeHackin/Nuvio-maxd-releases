// The session's core: setup, the account, addons, the library, settings, the
// pop-ups and the artwork. The Board, Discover and Library are session_browse.cpp,
// a title's page is session_detail.cpp.

#include "session.hpp"
#include "tracking.h"
#include "release_notes.hpp"

#include "http.h"
#include "providers.h"
#include "display_output.h"
#include "nuvio_account.h"
#include "qrcodegen.h"
#include "netstream.h"
#include "torrent/engine.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace sx
{

namespace
{

constexpr std::size_t kTextureBudget = 640u << 20; // bytes of artwork kept on the GPU

enum SettingIndex
{
    kAccount,
    kProfiles,
    kTracking,
    kLibrarySource,
    kConnectedServices,
    kTorrents,
    kServer,
    kDisplay,
    kRefresh,
    kHdr,
    kPlaybackEngine,
    kSubLangs,
    kAutoSubs,
    kSubSize,
    kSubFont,
    kSubColour,
    kSubEffect,
    kSubBackground,
    kSeek,
    kShoulderSeek,
    kCountdown,
    kReducedMotion,
    kHighContrast,
    kDiagnostics,
    kReleaseNotes,
    kAudioMode,
    kAudioOutput,
    kAudioLangs,
    kAutoplay,
    kSkipSegments,
    kUiSounds,
    kCalmBackgrounds,
    kAddAddon,
    kReload,
    kDonation,
    kInstallSource,
    kUpdates,
    kExit,
    kConsoleLights,
    kReadAhead,
    kSettingCount
};

std::string size_label(const std::string &s)
{
    if (s == "sub-s")
        return "Small";
    if (s == "sub-l")
        return "Large";
    return "Medium";
}

// The first letter of a name (one UTF-8 character): an addon's mark when it has no logo.
std::string first_letter(const std::string &name)
{
    std::size_t len = name.empty() ? 0 : 1;
    while (len < name.size() && (static_cast<unsigned char>(name[len]) & 0xC0) == 0x80)
        ++len;
    return name.substr(0, len);
}

std::uint32_t colour_for(const std::string &name)
{
    static const std::uint32_t palette[] = {0x7b5bf5, 0x3b82f6, 0x0ea5a4, 0xe0568a, 0xe08a3c, 0x6d6ae0};
    return palette[fnv1a(name) % (sizeof(palette) / sizeof(palette[0]))];
}

} // namespace

Item item_from_meta_json(const json &j)
{
    Item it;
    it.id = jstr(j, "id", jstr(j, "imdb_id"));
    it.type = jstr(j, "type");
    it.name = jstr(j, "name");
    it.poster = jstr(j, "poster");
    it.background = jstr(j, "background");
    it.logo = jstr(j, "logo");
    it.description = jstr(j, "description");
    it.release_info = jstr(j, "releaseInfo", jstr(j, "year"));
    it.runtime = jstr(j, "runtime");
    it.imdb_rating = jstr(j, "imdbRating");
    const json &g = jobj(j, "genres");
    if (g.is_array())
        for (const auto &v : g)
            if (v.is_string())
                it.genres.push_back(v.get<std::string>());
    return it;
}

// ---------------------------------------------------------------------------
// Setup

bool Session::init(const std::string &base_dir, const std::string &data_dir, hui::gfx::Renderer *renderer)
{
    player_.subtitle_renderer().configure(base_dir+"/fonts");
    base_dir_ = base_dir;
    data_dir_ = data_dir;
    profile_dir_ = data_dir + "/guest";
    make_dirs(profile_dir_);
    renderer_ = renderer;
    make_dirs(data_dir_);
    log_open(data_dir_ + "/log.txt");
    dlog("app: Nuvio Max'd %s (native experimental)", nuvio_update::current_version);
    http_init(base_dir_ + "/ca-bundle.crt");
    g_tasks.start(6);
    provider_tasks_.start(2);
    if (!providers::initialize(base_dir_ + "/providers/runtime.js"))
        dlog("providers: runtime unavailable");
    boot_start_ = now_seconds();
    tex_.init(data_dir_ + "/art", renderer_, 6);
    // The app's storage holds the read-ahead: the built-in torrent engine's 6 GB
    // rolling cache, and up to 6 GB for a stream from a server or a direct link.
    bt::Engine::get().configure(data_dir_, 6ll << 30);
    NetStream::set_cache_dir(data_dir_);

    load_prefs();
    release_notes_version=nuvio_update::current_version;
    release_notes_open=should_show_release_notes(prefs_.seen_release,release_notes_version);
    if (!nuvio_account_init(base_dir_ + "/account-config.json", data_dir_))
        say("Could not load Nuvio account configuration", 8.0, true);
    prefs_.auth_key = nuvio_profile_key();
    debrid::load(profile_dir_ + "/connected-services.json", services_);
    const auto account_user = nuvio_account_user_id();
    if ((!services_.torbox.sync_owner.empty() && services_.torbox.sync_owner != account_user) ||
        (!services_.premiumize.sync_owner.empty() && services_.premiumize.sync_owner != account_user)) {
        services_.clear_synced(); debrid::save(profile_dir_ + "/connected-services.json", services_);
    }
    prepared_->configure(services_);
    if (!signed_in()) {
        // Legacy guest data has no account owner. Claim it once so a future
        // account sign-in cannot absorb a different guest's local history.
        json marker, old_watch, old_prefs, existing;
        load_json(data_dir_ + "/watch-sync.json", old_watch);
        if (!load_json(data_dir_ + "/legacy-profile-owner.json", marker) && jstr(old_watch, "owner").empty()) {
            bool copied = true;
            for (const auto* file : {"progress.json", "connected-services.json"}) {
                json old;
                if (!load_json(profile_dir_ + "/" + file, existing) && load_json(data_dir_ + "/" + file, old))
                    copied = save_json(profile_dir_ + "/" + file, old) && copied;
            }
            if (!load_json(profile_dir_ + "/profile-settings.json", existing) && load_json(data_dir_ + "/settings.json", old_prefs))
                copied = save_json(profile_dir_ + "/profile-settings.json", old_prefs) && copied;
            if (copied) save_json(data_dir_ + "/legacy-profile-owner.json", json{{"owner", "guest"}});
        }
        load_profile_settings(); load_progress();
    }
    clock = format_clock();
    board.status = "Loading your catalogs...";
    refresh_settings();
    refresh_library();
    refresh_addons();
    profile_ready_ = !signed_in();
    if (signed_in()) {
        board.status = "Choose your Nuvio profile to load your catalogs and history.";
        profile_prompt_pending_ = true;
    } else load_addons();
    if (!signed_in() && !prefs_.onboarding_done) {
        if(release_notes_open || prefs_.install_source.empty()) login_after_notes_=true;
        else login_start();
    }
    return true;
}

void Session::shutdown()
{
    if (device_cancel_) *device_cancel_ = true;
    prepared_->shutdown();
    updater_.shutdown();
    if (watching)
        watch_stop(false);
    player_.close();
    bt::Engine::get().shutdown();
    *d_cancel_ = true;
    *d_stream_cancel_ = true;
    *w_episode_cancel_ = true;
    provider_tasks_.stop();
    g_tasks.stop();
    tex_.shutdown();
    save_progress();
}

// ---------------------------------------------------------------------------
// Persistence

void Session::load_prefs()
{
    json j;
    if (load_json(data_dir_ + "/settings.json", j))
    {
        load_experience(j, prefs_);
        prefs_.server_url.clear();
        prefs_.builtin_torrents = jbool(j, "allow_torrents", false);
        prefs_.onboarding_done = jbool(j, "onboarding_done");
        prefs_.subtitle_langs = jstr(j, "subtitle_languages", prefs_.subtitle_langs);
        prefs_.auto_subtitles = jbool(j, "auto_subtitles", prefs_.auto_subtitles);
        prefs_.sub_size = jstr(j, "subtitle_size", prefs_.sub_size);
        prefs_.audio_langs = jstr(j, "audio_languages");
        prefs_.autoplay_next = jbool(j, "autoplay_next", prefs_.autoplay_next);
        prefs_.skip_segments = jbool(j, "skip_segments", true);
        prefs_.calm_backgrounds = jbool(j, "calm_backgrounds", prefs_.calm_backgrounds);
        prefs_.ui_sounds = jbool(j, "ui_sounds", prefs_.ui_sounds);
        prefs_.display_output = display_output_preference_name(display_output_preference(jstr(j, "display_output")));
        prefs_.display_refresh = jnum(j, "display_refresh", 60) == 120 ? 120 : 60;
        prefs_.native_hdr = jbool(j, "native_hdr", true);
        prefs_.evo_player = jstr(j, "playback_engine") == "evo";
        prefs_.vlc_player = jstr(j, "playback_engine") == "vlc";
        const auto read_ahead = jnum(j, "read_ahead_seconds", 60);
        prefs_.read_ahead_seconds = (read_ahead == 30 || read_ahead == 60 || read_ahead == 120 || read_ahead == 300)
                                       ? static_cast<int>(read_ahead) : 60;
        prefs_.user_email = jstr(j, "user_email");
    }
}

void Session::save_prefs()
{
    json j;
    save_experience(j, prefs_);
    j["onboarding_done"] = prefs_.onboarding_done;
    j["allow_torrents"] = prefs_.builtin_torrents;
    j["subtitle_languages"] = prefs_.subtitle_langs;
    j["auto_subtitles"] = prefs_.auto_subtitles;
    j["subtitle_size"] = prefs_.sub_size;
    j["audio_languages"] = prefs_.audio_langs;
    j["autoplay_next"] = prefs_.autoplay_next;
    j["skip_segments"] = prefs_.skip_segments;
    j["calm_backgrounds"] = prefs_.calm_backgrounds;
    j["ui_sounds"] = prefs_.ui_sounds;
    j["display_output"] = prefs_.display_output;
    j["display_refresh"] = prefs_.display_refresh;
    j["native_hdr"] = prefs_.native_hdr;
    j["playback_engine"] = prefs_.vlc_player ? "vlc" : prefs_.evo_player ? "evo" : "native";
    j["read_ahead_seconds"] = prefs_.read_ahead_seconds;
    save_profile_settings();
    j["user_email"] = prefs_.user_email;
    if (!save_json(data_dir_ + "/settings.json", j))
        say("Could not save settings to " + data_dir_, 5.0, true);
}

void Session::load_progress()
{
    json j;
    const auto selected_source = jstr(tracking::Store(profile_dir_).status(), "progressPreferred", "nuvio");
    progress_file_ = tracking::provider(selected_source) ? "progress-"+selected_source+".json" : "progress.json";
    if (!load_json(profile_dir_ + "/"+progress_file_, j))
        return;
    const json &items = jobj(j, "items");
    if (items.is_object())
        for (auto it = items.begin(); it != items.end(); ++it)
        {
            Progress p;
            p.type = jstr(it.value(), "type");
            p.name = jstr(it.value(), "name");
            p.poster = jstr(it.value(), "poster");
            p.video_id = jstr(it.value(), "video_id");
            p.time = jnum(it.value(), "time");
            p.duration = jnum(it.value(), "duration");
            p.updated = static_cast<std::int64_t>(jnum(it.value(), "updated"));
            p.stream_json = jstr(it.value(), "stream");
            p.stream_video_id = jstr(it.value(), "stream_video");
            p.up_next = jstr(it.value(), "up_next");
            progress_[it.key()] = p;
        }
    const json &w = jobj(j, "watched");
    if (w.is_array())
        for (const auto &v : w)
            if (v.is_string())
                watched_.insert(v.get<std::string>());
}

void Session::save_progress()
{
    json j;
    json items = json::object();
    for (const auto &kv : progress_)
        items[kv.first] = json{{"type", kv.second.type},         {"name", kv.second.name},
                               {"poster", kv.second.poster},     {"video_id", kv.second.video_id},
                               {"time", kv.second.time},         {"duration", kv.second.duration},
                               {"updated", kv.second.updated},   {"stream", kv.second.stream_json},
                               {"stream_video", kv.second.stream_video_id}, {"up_next", kv.second.up_next}};
    j["items"] = items;
    j["watched"] = json(std::vector<std::string>(watched_.begin(), watched_.end()));
    if (profile_ready_) save_json(profile_dir_ + "/"+progress_file_, j);
}

// ---------------------------------------------------------------------------
// Addons and the library

const ::Addon *Session::find_addon(const std::string &url) const
{
    for (const auto &a : addons_)
        if (a->transport_url == url)
            return a.get();
    return nullptr;
}

void Session::load_addons()
{
    struct Result
    {
        std::vector<std::shared_ptr<::Addon>> addons;
        std::vector<std::string> errors;
        bool auth_failed = false;
        bool account_failed = false;
    };
    if (!profile_ready_) return;
    const std::string key = prefs_.auth_key;
    const std::vector<std::string> extra = prefs_.extra_addons;
    const int gen = ++addons_gen_;
    addons_loading_ = true;
    bg<Result>(
        [key, extra]() {
            Result r;
            std::set<std::string> seen;
            if (!key.empty())
            {
                const ApiResult ar = api_addon_collection(key);
                if (ar.ok)
                {
                    if (jbool(ar.result, "offline"))
                        r.errors.push_back("Nuvio sync is unavailable. Using saved addons.");
                    if (!jstr(ar.result, "syncError").empty()) r.errors.push_back(jstr(ar.result, "syncError"));
                    const json &list = jobj(ar.result, "addons");
                    if (list.is_array())
                        for (const auto &e : list)
                        {
                            auto a = std::make_shared<::Addon>();
                            const std::string url = jstr(e, "transportUrl");
                            if (parse_addon(jobj(e, "manifest"), url, *a))
                            {
                                a->from_account = true;
                                if (seen.insert(url).second)
                                    r.addons.push_back(a);
                            }
                        }
                }
                else
                {
                    r.account_failed = true;
                    r.errors.push_back("Could not load your addons (" + ar.error + ")");
                    const std::string l = lower(ar.error);
                    if (l.find("session") != std::string::npos || l.find("auth") != std::string::npos)
                        r.auth_failed = true;
                }
            }
            std::vector<std::string> urls;
            if (r.addons.empty())
                urls = {kCinemetaUrl, kOpenSubtitlesUrl};
            for (const auto &u : extra)
                urls.push_back(u);
            for (const auto &u : urls)
            {
                if (seen.count(u))
                    continue;
                auto a = std::make_shared<::Addon>();
                std::string err;
                if (fetch_addon(u, *a, err))
                {
                    seen.insert(u);
                    r.addons.push_back(a);
                }
                else
                {
                    r.errors.push_back("Could not reach an addon (" + err + ")");
                }
            }
            return r;
        },
        [this, gen](Result &r) {
            if (gen != addons_gen_)
                return;
            addons_loading_ = false;
            // Keep the working account addons across a transient sync failure.
            if (r.account_failed && !addons_.empty() && signed_in())
            {
                say(r.errors.front(), 6.0, true);
                return;
            }
            addons_.clear();
            const auto extra_before = prefs_.extra_addons;
            for (const auto &a : r.addons)
            {
                // Once a local addition is represented in the cloud snapshot,
                // stop treating it as a permanent local override of cloud deletes.
                if (a->from_account) {
                    auto& extra = prefs_.extra_addons;
                    extra.erase(std::remove(extra.begin(), extra.end(), a->transport_url), extra.end());
                }
                auto installed = std::make_unique<::Addon>(*a);
                for (auto &p : installed->providers)
                    p.enabled = jbool(jobj(prefs_.provider_enabled, a->transport_url.c_str()), p.id.c_str());
                addons_.push_back(std::move(installed));
            }
            if (extra_before != prefs_.extra_addons) save_prefs();
            if (signed_in()) for (const auto& a : addons_)
                if (!a->from_account &&
                    std::find(prefs_.extra_addons.begin(), prefs_.extra_addons.end(), a->transport_url) != prefs_.extra_addons.end() &&
                    !(a->provider_repository ? inherit_plugins_ : inherit_addons_))
                    sync_source(a->transport_url, a->provider_repository, false, true);
            if (!r.errors.empty())
                say(r.errors.front(), 6.0, true);
            if (r.auth_failed)
                say("Your Nuvio session expired. Sign in again in Settings.", 8.0, true);
            dlog("addons: %zu loaded", addons_.size());
            for (const auto &a : addons_)
                dlog("addons: %s, %zu catalogs", a->name.c_str(), a->catalogs.size());
            on_addons_loaded();
        });
}

void Session::on_addons_loaded()
{
    build_board();
    // Discover starts again from the new catalogs the next time it opens.
    disc_items_.clear();
    discover.items.clear();
    disc_types_.clear();
    discover.status.clear();
    changed_ |= kDiscover | kDiscoverNew;
    refresh_addons();
    refresh_settings();
    if (page_ == Page::discover)
        enter_discover();
}

void Session::load_library()
{
    const std::string key = prefs_.auth_key;
    if (!profile_ready_ || key.empty() || library_loading_)
        return;
    library_loading_ = true;
    library_refresh_at_ = now_seconds() + 60;
    const int account_gen = login_gen_, library_gen = ++library_generation_;
    load_watched();
    bg<ApiResult>([key]() { return api_library_get(key); },
                  [this, key, account_gen, library_gen](ApiResult &r) {
                      if (library_gen != library_generation_) return;
                      library_loading_ = false;
                      if (prefs_.auth_key != key || login_gen_ != account_gen || !signed_in()) return;
                      if (!r.ok)
                      {
                          say("Could not load your library: " + r.error, 5.0, true);
                          return;
                      }
                      if (!r.error.empty()) say(r.error, 12.0);
                      tracking_refresh();
                      const auto active = library_.find(w_item_.id);
                      json playing_item = watching && active != library_.end() ? active->second : json();
                      library_.clear();
                      if (r.result.is_array())
                          for (const auto &it : r.result)
                          {
                              const std::string id = jstr(it, "_id");
                              if (!id.empty())
                                  library_[id] = it;
                          }
                      if (playing_item.is_object()) library_[w_item_.id] = playing_item;
                      for (auto& [id,p] : progress_) {
                          if (watching && id == w_item_.id) continue;
                          if (!library_.count(id) || !jbool(jobj(library_[id],"state"),"nuvioSyncManaged")) { p.time = 0; p.up_next.clear(); }
                      }
                      for (const auto& [id, item] : library_)
                      {
                          if (watching && id == w_item_.id) continue;
                          const auto& state = jobj(item, "state");
                          if (!jbool(state, "nuvioSyncManaged")) continue;
                          Progress& p = progress_[id];
                          const auto video = jstr(state, "video_id");
                          if (video != p.video_id) p.up_next.clear();
                          if (jnum(state, "timeOffset") > 0) p.up_next.clear();
                          if (jnum(state, "timeOffset") <= 0 && !jbool(state, "nuvioCompleted")) p.up_next.clear();
                          p.type = jstr(item, "type"); p.name = jstr(item, "name"); p.poster = jstr(item, "poster");
                          p.video_id = video; p.time = jnum(state, "timeOffset") / 1000;
                          p.duration = jnum(state, "duration") / 1000;
                          p.updated = int64_t(jnum(state, "lastWatchedMs"));
                      }
                      save_progress();
                      library_loaded_ = true;
                      dlog("library: %zu items", library_.size());
                      refresh_board();
                      refresh_library();
                  });
}

void Session::addons_reload()
{
    say("Reloading addons...");
    load_addons();
    if (signed_in())
        load_library();
}

void Session::addon_activate(int index, int provider_cursor)
{
    if (index < 0 || index >= static_cast<int>(addons_.size()))
        return;
    const std::string url = addons_[static_cast<std::size_t>(index)]->transport_url;
    auto &selected = *addons_[static_cast<std::size_t>(index)];
    if (selected.provider_repository ? inherit_plugins_ : inherit_addons_) {
        say("This profile uses the primary profile's sources. Manage them in that profile.", 7.0); return;
    }
    if (selected.provider_repository)
    {
        std::vector<std::string> choices;
        for (const auto &p : selected.providers)
            choices.push_back(std::string(p.enabled ? "On: " : "Off: ") + p.name);
        const int count = static_cast<int>(choices.size());
        choices.push_back("Remove repository");
        choices.push_back("Cancel");
        open_dropdown(selected.name, choices, -1,
                      [this, url, count](int pick)
                      {
                          ::Addon *a = nullptr;
                          for (auto &candidate : addons_)
                              if (candidate->transport_url == url)
                                  a = candidate.get();
                          if (!a || pick < 0 || pick > count)
                              return;
                          if (pick == count)
                          {
                              auto &ex = prefs_.extra_addons;
                              ex.erase(std::remove(ex.begin(), ex.end(), url), ex.end());
                              prefs_.provider_enabled.erase(url);
                              save_prefs();
                              sync_source(url, true, true);
                              load_addons();
                              return;
                          }
                          auto &p = a->providers[static_cast<size_t>(pick)];
                          p.enabled = !p.enabled;
                          prefs_.provider_enabled[url][p.id] = p.enabled;
                          save_prefs();
                          refresh_addons();
                          say(p.name + (p.enabled ? " enabled" : " disabled"));
                          // Keep the list open at the toggled provider so configuring a
                          // long repository doesn't restart navigation at the first row.
                          for (int i = 0; i < static_cast<int>(addons_.size()); ++i)
                              if (addons_[i]->transport_url == url) {
                                  addon_activate(i, pick);
                                  break;
                              }
                      });
        dropdown.selected = std::clamp(provider_cursor, 0, count + 1);
        dropdown.danger = count;
        return;
    }
    const auto it = std::find(prefs_.extra_addons.begin(), prefs_.extra_addons.end(), url);
    if (it == prefs_.extra_addons.end() && !selected.from_account)
    {
        say("Install and remove addons in the Nuvio app on your phone or computer; they sync here. "
            "Press Square to reload them now.",
            7.0);
        return;
    }
    open_dropdown(addons_[static_cast<std::size_t>(index)]->name, {"Remove addon", "Keep"}, 1, [this, url](int i) {
        if (i != 0)
            return;
        auto &ex = prefs_.extra_addons;
        ex.erase(std::remove(ex.begin(), ex.end(), url), ex.end());
        save_prefs();
        sync_source(url, false, true);
        load_addons();
    });
}

// ---------------------------------------------------------------------------
// Addons and settings screens

void Session::refresh_addons()
{
    addons.items.clear();
    addon_logos_.clear();
    for (const auto &a : addons_)
    {
        sx::Addon row;
        row.name = a->name;
        row.version = a->version.empty() ? "" : "v" + a->version;
        row.description = a->description;
        if (a->provider_repository)
        {
            size_t enabled = 0;
            for (const auto &p : a->providers)
                enabled += p.enabled;
            row.description = "Plugin repository · " + std::to_string(enabled) + " of " +
                              std::to_string(a->providers.size()) + " providers enabled. Select to manage.";
        }
        std::vector<std::string> types;
        for (const auto &t : a->types)
            types.push_back(t);
        row.types = join(types, " ");
        row.local = !a->from_account;
        row.initial = first_letter(a->name);
        row.colour = colour_for(a->name);
        addons.items.push_back(row);
        addon_logos_.push_back(jstr(a->manifest, "logo"));
    }
    changed_ |= kAddons;
    art_dirty_ = true;
}

void Session::set_display_configuration(unsigned width, unsigned height, bool fallback)
{
    configured_w_ = width; configured_h_ = height;
    dlog("display: PS5 configured %ux%u at startup; render %dx%d SDR 8-bit; accepted refresh %d Hz; fallback %d",
         width, height, output_w_, output_h_, output_hz_, fallback);
    if (fallback)
        say("Display fallback active. Check Display output in Settings; reopen Nuvio Max'd after changing it.", 12.0, true);
}

void Session::refresh_settings()
{
    settings.subtitle_style = prefs_.subtitle_style;
    settings.subtitle_size = prefs_.sub_size == "sub-s" ? 0 : prefs_.sub_size == "sub-l" ? 2 : 1;
    settings.account = prefs_.user_email;
    settings.rows.assign(kSettingCount, SettingRow());
    const auto set = [&](int i, SettingRow::Kind kind, const char *label, const std::string &value, const std::string &help,
                         bool on = false) {
        SettingRow &r = settings.rows[static_cast<std::size_t>(i)];
        r.kind = kind;
        r.label = label;
        r.value = value;
        r.help = help;
        r.on = on;
    };
    using Kind = SettingRow::Kind;
    set(kAccount, Kind::value, "Nuvio account", signed_in() ? prefs_.user_email : "Not signed in",
        signed_in() ? (confirm_sign_out_ ? "Press Cross again to sign out." : "Press Cross to sign out.")
                    : "Sign in with a QR code to choose a profile and load its addons and saved library. Press Cross.");
    set(kProfiles, Kind::value, "Profiles", signed_in() ? jstr(active_profile_, "name", "Choose profile") : "Sign in first",
        "Import or switch Nuvio account profiles, or create a profile. Each has separate history and sources.");
    set(kTorrents, Kind::toggle, "Peer-to-peer torrents", "",
        "Off uses direct and debrid links from your configured addons. Turn on to also download raw torrents.",
        prefs_.builtin_torrents);
    const int connections = int(!services_.torbox.key.empty()) + int(!services_.premiumize.key.empty());
    set(kConnectedServices, Kind::value, "Connected Services", connections ? std::to_string(connections) + " connected" : "TorBox / Premiumize",
        "Connect TorBox or Premiumize, import connections from Nuvio Sync, and prepare cached links for faster playback. The selected file keeps its original quality.");
    set(kTracking, Kind::value, "Tracking accounts", "Trakt / SIMKL / MDBList",
        "Connect trackers for this profile. Connecting does not change your Library Source or Watch Progress choice.");
    const auto library_source = jstr(tracking_status_, "library", "nuvio");
    const auto progress_source = jstr(tracking_status_, "progress", "nuvio");
    set(kLibrarySource, Kind::value, "Library Source", library_source == "nuvio" ? "Nuvio Library" : tracking::name(library_source),
        "Choose which source Nuvio reads for your Library. Connected trackers become available here. Nuvio Library is the default.");
    set(kServer, Kind::value, "Watch Progress", progress_source == "nuvio" ? "Nuvio Sync" : tracking::name(progress_source),
        "Choose which progress source powers resume and Continue Watching. Nuvio Sync is the default; guest progress stays on this PS5.");
    if (jstr(tracking_status_, "libraryPreferred", "nuvio") != library_source)
        settings.rows[kLibrarySource].help += " Tracker unavailable. Using Nuvio Library until it reconnects.";
    if (jstr(tracking_status_, "progressPreferred", "nuvio") != progress_source)
        settings.rows[kServer].help += " Tracker unavailable. Using Nuvio Sync until it reconnects.";
    const char* display_labels[] = {"Follow PS5", "1080p", "1440p", "4K"};
    set(kDisplay, Kind::value, "Display output", display_labels[display_output_preference(prefs_.display_output) + 1],
        "Follow the PS5's output resolution, or choose an override. Close and reopen Nuvio Max'd to apply a change.");
    set(kHdr, Kind::value, "HDR playback", prefs_.native_hdr ? "Native HDR10 (experimental)" : "Convert HDR to SDR",
        "Native HDR10 keeps PQ colour and highlights on a compatible TV. SDR conversion changes the colour range. Applies to the next stream.");
    set(kPlaybackEngine, Kind::value, "Playback engine", prefs_.vlc_player ? "VLC 3.0.24 (experimental)" : prefs_.evo_player ? "EVO Player 0.12.0 (experimental)" : "Nuvio native",
        "Choose the video engine. Applies to the next stream; subtitles and Watch Progress stay available.");
    set(kReadAhead, Kind::value, "Read-ahead (Native/EVO)", std::to_string(prefs_.read_ahead_seconds) + " s",
        "How far to buffer ahead. Memory limits may shorten the target. Applies to the next Native/EVO stream; VLC "
        "manages its own buffer.");
    set(kRefresh, Kind::value, "Display refresh", prefs_.display_refresh == 120 ? "Prefer 120 Hz" : "60 Hz",
        "120 Hz can make 24 fps films feel steadier on a compatible TV. Keeps the selected resolution; uses 60 Hz if unavailable. Close and reopen to apply.");
    set(kAudioMode, Kind::value, "Audio output", prefs_.vlc_player?"PCM (VLC)":prefs_.audio_output_mode=="pcm"?"PCM":prefs_.audio_output_mode=="auto"?"Auto (experimental)":"Bitstream (experimental)",
        prefs_.vlc_player ? "VLC currently uses decoded PCM. Auto and Bitstream apply to Native/EVO playback." : "Auto uses PCM with Surround off; with Surround on it checks HDMI capabilities. Bitstream forces a compressed-format attempt. Applies to the next video.");
    set(kAudioOutput, Kind::toggle, "Surround audio", "", "Use 7.1 PCM for compatible HDMI surround systems. Keep off for TV speakers or optical stereo. Applies to the next video.", prefs_.surround_audio);
    set(kSubLangs, Kind::value, "Subtitle languages", prefs_.subtitle_langs.empty() ? "None" : prefs_.subtitle_langs,
        "Preferred order, for example \"gre, eng\".");
    set(kAutoSubs, Kind::toggle, "Subtitles on automatically", "", "Prefer matching subtitles inside the video file. Use addon subtitles when the file has no matching track.",
        prefs_.auto_subtitles);
    set(kSubSize, Kind::value, "Subtitle size", size_label(prefs_.sub_size), "How large subtitles are drawn over the video.");
    static const char* fonts[]={"Regular", "Bold", "Display"};
    static const char* colours[]={"White", "Yellow", "Cyan"};
    static const char* effects[]={"Plain", "Outline", "Shadow"};
    set(kSubFont, Kind::value, "Subtitle font", fonts[prefs_.subtitle_style.font], "Choose the subtitle face. Unicode fallbacks remain available.");
    set(kSubColour, Kind::value, "Subtitle colour", colours[prefs_.subtitle_style.colour], "Choose a colour that is comfortable to read.");
    set(kSubEffect, Kind::value, "Subtitle effect", effects[prefs_.subtitle_style.effect], "Add an outline or shadow to separate text from the picture.");
    set(kSubBackground, Kind::value, "Subtitle background", std::to_string(prefs_.subtitle_style.background)+"%", "Choose the opacity behind subtitle text.");
    for(int i : {kSubSize,kSubFont,kSubColour,kSubEffect,kSubBackground}) settings.rows[i].subtitle_preview=true;
    set(kSeek, Kind::value, "D-pad seek interval", std::to_string(prefs_.seek_seconds)+" s", "How far Left and Right skip during playback.");
    set(kShoulderSeek, Kind::value, "L1 / R1 seek interval", std::to_string(prefs_.shoulder_seek_seconds)+" s", "How far the shoulder buttons skip during playback.");
    set(kCountdown, Kind::value, "Next-episode countdown", std::to_string(prefs_.next_episode_seconds)+" s", "Countdown during credits or when an episode ends, once a matching stream is ready. Pause or buffering holds the countdown; Circle cancels it.");
    set(kReducedMotion, Kind::toggle, "Reduced motion", "", "Make focus, scrolling and menu transitions immediate; keep decorative backgrounds still.", prefs_.reduced_motion);
    set(kHighContrast, Kind::toggle, "Higher contrast", "", "Brighter interface text and stronger panel and focus boundaries. The video picture is unchanged.", prefs_.high_contrast);
    set(kReleaseNotes, Kind::action, "What’s new", "Nuvio Max’d " + std::string(nuvio_update::current_version), "Read the changes in this version again.");
    set(kDiagnostics, Kind::action, "Playback diagnostics", "", "Show where this app saves playback, frame timing and crash diagnostics for troubleshooting.");
    set(kAudioLangs, Kind::value, "Audio languages", prefs_.audio_langs.empty() ? "Default" : prefs_.audio_langs,
        "Preferred audio track language, for example \"eng\". Leave empty for the default.");
    set(kAutoplay, Kind::toggle, "Play next episode automatically", "", "Show a cancellable countdown during credits or at the end of an episode. Match the selected stream quality.",
        prefs_.autoplay_next);
    set(kSkipSegments, Kind::toggle, "Skip intro and recap buttons", "",
        "Use IntroDB episode timings for intros, recaps and credits. Availability varies by show and release.", prefs_.skip_segments);
    set(kCalmBackgrounds, Kind::toggle, "Calmer backgrounds", "", "Keep decorative menu backgrounds still. Video playback stays full quality.", prefs_.calm_backgrounds);
    set(kUiSounds, Kind::toggle, "Interface sounds", "", "A soft sound when you move around, go back or switch page.", prefs_.ui_sounds);
    set(kAddAddon, Kind::value, "Add addon or plugin URL", "",
        prefs_.extra_addons.empty()
            ? "A Stremio addon or Nuvio provider manifest URL. Manage providers on the Addons page."
            : std::to_string(prefs_.extra_addons.size()) + " added here.");
    set(kReload, Kind::action, "Reload addons and catalogs", "", "Fetch every addon and catalog again.");
    const auto update = updater_.status();
    set(kDonation, Kind::action, "Support on Ko-fi", "", "Optional support for development. Scan the QR code with your phone.");
    set(kInstallSource, Kind::value, "Install source", install_source_label(prefs_.install_source),
        "Where you installed Nuvio Max'd. Update reminders use this choice; change it if you switch install methods.");
    set(kUpdates, Kind::value, "App updates", update.phase == AppUpdate::Phase::idle ? "Nuvio Max'd " + std::string(nuvio_update::current_version) + " (experimental)" : update.message,
        update_destination(prefs_.install_source));
    set(kExit, Kind::action, "Exit Nuvio Max'd", "", "Close the app and go back to the home screen.");
    set(kConsoleLights, Kind::toggle, "Console light sync", "", "Fade the console lights with the controller while Nuvio is open. Console colours are approximate. Unsupported systems leave their lights unchanged. Turning off restores the previous state.", prefs_.console_light_sync);
    const char* light_status = console_light_status_==ConsoleLights::Status::active ? "Sync active." :
        console_light_status_==ConsoleLights::Status::unavailable ? "Console light access is unavailable on this system." :
        console_light_status_==ConsoleLights::Status::system_owned ? "System lighting took priority; toggle off and on to retry." : "Waiting for Nuvio to be in front.";
    if(prefs_.console_light_sync) settings.rows[kConsoleLights].help += std::string(" ")+light_status;
    const auto group = [&](const char* name,std::initializer_list<int> ids) {
        for(int id:ids) settings.rows[id].category=name;
    };
    group("Account & Services", {kAccount,kProfiles,kTracking,kLibrarySource,kConnectedServices,kServer});
    group("Video", {kDisplay,kRefresh,kHdr,kPlaybackEngine});
    group("Audio", {kAudioMode,kAudioOutput,kAudioLangs});
    group("Subtitles", {kSubLangs,kAutoSubs,kSubSize,kSubFont,kSubColour,kSubEffect,kSubBackground});
    group("Playback", {kSeek, kShoulderSeek, kCountdown, kAutoplay, kSkipSegments, kReadAhead});
    group("Appearance & Lighting", {kReducedMotion,kHighContrast,kUiSounds,kCalmBackgrounds,kConsoleLights});
    group("Addons", {kTorrents,kAddAddon,kReload});
    group("App & Updates", {kReleaseNotes,kDiagnostics,kDonation,kInstallSource,kUpdates,kExit});
    changed_ |= kSettings;
}

void Session::settings_activate(int row)
{
    if (signed_in() && !profile_ready_ &&
        (row == kTracking || row == kLibrarySource || row == kConnectedServices || row == kServer || row == kAddAddon || row == kReload ||
         row == kSubLangs || row == kAudioLangs || row == kAutoSubs)) {
        say("Choose a profile in Account & Services > Profiles first.", 7.0); return;
    }

    if (row != kAccount && confirm_sign_out_)
        confirm_sign_out_ = false;
    switch (row)
    {
    case kConsoleLights:
        prefs_.console_light_sync=!prefs_.console_light_sync; save_prefs(); cue(hui::audio::Cue::toggle); break;
    case kConnectedServices:
        services_menu();
        break;
    case kProfiles: profiles_menu(); break;
    case kTracking: tracking_menu(); break;
    case kLibrarySource: tracking_source_menu(false); break;
    case kAccount:
        if (!signed_in())
            login_start();
        else if (!confirm_sign_out_)
            confirm_sign_out_ = true;
        else
        {
            confirm_sign_out_ = false;
            sign_out();
        }
        break;
    case kTorrents:
        prefs_.builtin_torrents = !prefs_.builtin_torrents;
        save_prefs();
        cue(hui::audio::Cue::toggle);
        detail_refresh_streams();
        break;
    case kServer:
        tracking_source_menu(true);
        break;
    case kDisplay:
        open_dropdown("Display output", {"Follow PS5", "1080p", "1440p", "4K"},
                      display_output_preference(prefs_.display_output) + 1, [this](int i) {
            if (i < 0 || i > 3) return;
            prefs_.display_output = display_output_preference_name(i - 1);
            save_prefs();
            cue(hui::audio::Cue::saved);
            say("Display choice saved. Close and reopen Nuvio Max'd to apply it.", 8.0);
            refresh_settings();
        });
        break;
    case kRefresh:
        open_dropdown("Display refresh", {"60 Hz", "Prefer 120 Hz"},
                      prefs_.display_refresh == 120 ? 1 : 0, [this](int i) {
            if (i != 0 && i != 1) return;
            prefs_.display_refresh = i == 1 ? 120 : 60;
            save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
            say("Refresh choice saved. Close and reopen Nuvio Max'd to apply it.",8.0);
        });
        break;
    case kSubLangs:
        ask_text([this](const std::string &v) {
            if (trim(v).empty())
                return;
            prefs_.subtitle_langs = trim(v);
            save_prefs();
            cue(hui::audio::Cue::saved);
            refresh_settings();
        });
        break;
    case kHdr:
        open_dropdown("HDR playback", {"Native HDR10 (experimental)", "Convert HDR to SDR"},
                      prefs_.native_hdr ? 0 : 1, [this](int i) {
            if (i != 0 && i != 1) return;
            prefs_.native_hdr = i == 0;
            save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
        });
        break;
    case kPlaybackEngine: {
        std::vector<std::string> engines={"Nuvio native", "EVO Player 0.12.0 (experimental)"};
        if(VlcBackend::available())engines.push_back("VLC 3.0.24 (experimental)");
        open_dropdown("Playback engine", engines,
                      prefs_.vlc_player && VlcBackend::available() ? 2 : prefs_.evo_player ? 1 : 0, [this](int i) {
            if(i<0 || i>2 || (i==2 && !VlcBackend::available()))return;
            prefs_.evo_player=i==1;prefs_.vlc_player=i==2;
            save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
        });
        break;
    }
    case kReadAhead:
    {
        const std::vector<int> values = {30, 60, 120, 300};
        const int selected =
            static_cast<int>(std::find(values.begin(), values.end(), prefs_.read_ahead_seconds) - values.begin());
        open_dropdown("Read-ahead", {"30 seconds", "1 minute", "2 minutes", "5 minutes"}, selected,
                      [this, values](int i)
                      {
                          if (i < 0 || i >= static_cast<int>(values.size()))
                              return;
                          prefs_.read_ahead_seconds = values[i];
                          save_prefs();
                          refresh_settings();
                          cue(hui::audio::Cue::saved);
                      });
        break;
    }
    case kSubFont: {
        static constexpr int values[] = {0,1,2};
        int selected=0; for(int i=0;i<3;++i) if(prefs_.subtitle_style.font==values[i]) selected=i;
        open_dropdown("Subtitle font", {"Regular","Bold","Display"}, selected, [this](int i) {
            if(i<0 || i>=3) return;
            prefs_.subtitle_style.font=values[i]; save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
        }); break;
    }
    case kSubColour: {
        static constexpr int values[] = {0,1,2};
        int selected=0; for(int i=0;i<3;++i) if(prefs_.subtitle_style.colour==values[i]) selected=i;
        open_dropdown("Subtitle colour", {"White","Yellow","Cyan"}, selected, [this](int i) {
            if(i<0 || i>=3) return;
            prefs_.subtitle_style.colour=values[i]; save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
        }); break;
    }
    case kSubEffect: {
        static constexpr int values[] = {0,1,2};
        int selected=0; for(int i=0;i<3;++i) if(prefs_.subtitle_style.effect==values[i]) selected=i;
        open_dropdown("Subtitle effect", {"Plain","Outline","Shadow"}, selected, [this](int i) {
            if(i<0 || i>=3) return;
            prefs_.subtitle_style.effect=values[i]; save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
        }); break;
    }
    case kSubBackground: {
        static constexpr int values[] = {0,25,50,75,100};
        int selected=0; for(int i=0;i<5;++i) if(prefs_.subtitle_style.background==values[i]) selected=i;
        open_dropdown("Subtitle background", {"Off","25%","50%","75%","100%"}, selected, [this](int i) {
            if(i<0 || i>=5) return;
            prefs_.subtitle_style.background=values[i]; save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
        }); break;
    }
    case kSeek: {
        static constexpr int values[] = {5,10,15,30,60};
        int selected=0; for(int i=0;i<5;++i) if(prefs_.seek_seconds==values[i]) selected=i;
        open_dropdown("D-pad seek interval", {"5 s","10 s","15 s","30 s","60 s"}, selected, [this](int i) {
            if(i<0 || i>=5) return;
            prefs_.seek_seconds=values[i]; save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
        }); break;
    }
    case kShoulderSeek: {
        static constexpr int values[] = {5,10,30,60,120,300};
        int selected=0; for(int i=0;i<6;++i) if(prefs_.shoulder_seek_seconds==values[i]) selected=i;
        open_dropdown("L1 / R1 seek interval", {"5 s","10 s","30 s","60 s","120 s","300 s"}, selected, [this](int i) {
            if(i<0 || i>=6) return;
            prefs_.shoulder_seek_seconds=values[i]; save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
        }); break;
    }
    case kCountdown: {
        static constexpr int values[] = {5,10,15,30,60,120};
        int selected=0; for(int i=0;i<6;++i) if(prefs_.next_episode_seconds==values[i]) selected=i;
        open_dropdown("Next-episode countdown", {"5 s","10 s","15 s","30 s","60 s","120 s"}, selected, [this](int i) {
            if(i<0 || i>=6) return;
            prefs_.next_episode_seconds=values[i]; save_prefs(); refresh_settings(); cue(hui::audio::Cue::saved);
        }); break;
    }
    case kReducedMotion: prefs_.reduced_motion=!prefs_.reduced_motion; save_prefs(); cue(hui::audio::Cue::toggle); break;
    case kHighContrast: prefs_.high_contrast=!prefs_.high_contrast; save_prefs(); cue(hui::audio::Cue::toggle); break;
    case kAudioMode: {
        const int active=prefs_.audio_output_mode=="pcm"?0:prefs_.audio_output_mode=="auto"?1:2;
        open_dropdown("Audio output",{"PCM","Auto (experimental)","Bitstream (experimental)"},active,[this](int i){
            prefs_.audio_output_mode=i==0?"pcm":i==1?"auto":"bitstream";save_prefs();refresh_settings();});
        dropdown.message="Auto uses only formats advertised by the HDMI receiver. Bitstream tries a compressed port even if capability detection is unavailable. Refused ports fall back to PCM. TrueHD and DTS-HD keep decoded lossless PCM.";break;
    }
    case kDonation: {
        sign_in=SignInState{};sign_in.link_only=true;sign_in.title="Support on Ko-fi";
        sign_in.link=donation_link;sign_in.cancel_label="Close";
        std::uint8_t temp[qrcodegen_BUFFER_LEN_MAX],qr[qrcodegen_BUFFER_LEN_MAX];
        if(qrcodegen_encodeText(donation_url,temp,qr,qrcodegen_Ecc_MEDIUM,qrcodegen_VERSION_MIN,20,qrcodegen_Mask_AUTO,true)) {
            sign_in.qr_size=qrcodegen_getSize(qr);sign_in.qr_modules.resize(sign_in.qr_size*sign_in.qr_size);
            for(int y=0;y<sign_in.qr_size;++y)for(int x=0;x<sign_in.qr_size;++x)
                sign_in.qr_modules[y*sign_in.qr_size+x]=qrcodegen_getModule(qr,x,y);
        }
        sign_in_open=true;break;
    }
    case kInstallSource: install_source_menu(); break;
    case kReleaseNotes: release_notes_open=true; release_notes_version=nuvio_update::current_version; break;
    case kDiagnostics: say("Diagnostics: " + data_dir_ + "/runtime.json, boot-current.txt and crash-current.txt. Previous run files are kept too.", 15.0); break;
    case kAudioLangs:
        ask_text([this](const std::string &v) {
            if (trim(v).empty())
                return;
            prefs_.audio_langs = trim(v) == "-" ? "" : trim(v);
            save_prefs();
            cue(hui::audio::Cue::saved);
            refresh_settings();
        });
        break;
    case kAddAddon:
        ask_text([this](const std::string &v) {
            const std::string url = normalize_addon_url(v);
            if (trim(v).empty() || trim(v) == "https://")
                return;
            say("Loading addon or plugin repository...");
            bg<std::string>(
                [url]() {
                    ::Addon a;
                    std::string err;
                    if (!fetch_addon(url, a, err))
                        return "Could not add the addon or plugin: " + err;
                    return a.provider_repository ? std::string("plugin") : std::string("addon");
                },
                [this, url](std::string &err) {
                    if (err != "plugin" && err != "addon")
                    {
                        say(err, 6.0, true);
                        return;
                    }
                    const bool plugin = err == "plugin";
                    if (plugin ? inherit_plugins_ : inherit_addons_) {
                        say("This profile inherits those sources. Switch to the primary profile to edit them.", 8.0); return;
                    }
                    if (std::find(prefs_.extra_addons.begin(), prefs_.extra_addons.end(), url) == prefs_.extra_addons.end())
                        prefs_.extra_addons.push_back(url);
                    save_prefs();
                    sync_source(url, plugin, false);
                    say("Added. Open Addons to manage it and enable individual providers.", 7.0);
                    load_addons();
                    refresh_settings();
                });
        });
        break;
    case kAudioOutput:
        prefs_.surround_audio = !prefs_.surround_audio; save_prefs(); cue(hui::audio::Cue::toggle); break;
    case kAutoSubs:
        prefs_.auto_subtitles = !prefs_.auto_subtitles;
        save_prefs();
        cue(hui::audio::Cue::toggle);
        break;
    case kSubSize:
    {
        static const char *sizes[] = {"sub-s", "sub-m", "sub-l"};
        const int cur = prefs_.sub_size == "sub-s" ? 0 : prefs_.sub_size == "sub-l" ? 2 : 1;
        open_dropdown("Subtitle size", {"Small", "Medium", "Large"}, cur, [this](int i) {
            prefs_.sub_size = sizes[i];
            save_prefs();
            cue(hui::audio::Cue::saved);
            refresh_settings();
        });
        break;
    }
    case kAutoplay:
        prefs_.autoplay_next = !prefs_.autoplay_next;
        save_prefs();
        cue(hui::audio::Cue::toggle);
        break;
    case kSkipSegments:
        prefs_.skip_segments = !prefs_.skip_segments;
        save_prefs();
        cue(hui::audio::Cue::toggle);
        break;
    case kUiSounds:
        prefs_.ui_sounds = !prefs_.ui_sounds;
        save_prefs();
        cue(hui::audio::Cue::toggle);
        break;
    case kCalmBackgrounds:
        prefs_.calm_backgrounds = !prefs_.calm_backgrounds;
        save_prefs();
        cue(hui::audio::Cue::toggle);
        break;
    case kReload:
        addons_reload();
        break;
    case kUpdates:
        update_menu();
        break;
    case kExit:
        exit_ = true;
        break;
    default:
        break;
    }
    refresh_settings();
}

// ---------------------------------------------------------------------------
// Pop-ups

void Session::say(const std::string &message, double seconds, bool error)
{
    toast.message = message;
    toast.error = error;
    toast_open = true;
    cue(hui::audio::Cue::notify);
    toast_until_ = now_seconds() + seconds;
}

void Session::open_dropdown(const std::string &title, const std::vector<std::string> &options, int active,
                            std::function<void(int)> chosen)
{
    update_dialog_ = false;
    install_source_dialog_ = false;
    dropdown.title = title;
    dropdown.message.clear();
    dropdown.danger = -1;
    dropdown.dialog = false;
    dropdown.poster = 0;
    dropdown.progress = -1.0f;
    dropdown.options = options;
    dropdown.active = active;
    dropdown.selected = std::max(0, std::min(active, static_cast<int>(options.size()) - 1));
    dropdown_chosen_ = std::move(chosen);
    dropdown_open = true;
}

void Session::dropdown_confirm()
{
    update_dialog_ = false;
    dropdown_open = false;
    const auto fn = dropdown_chosen_;
    dropdown_chosen_ = nullptr;
    if (fn && dropdown.selected >= 0 && dropdown.selected < static_cast<int>(dropdown.options.size()))
        fn(dropdown.selected);
}

void Session::release_notes_dismiss() {
    release_notes_open=false; prefs_.seen_release=release_notes_version; save_prefs();
    if(login_after_notes_ && !prefs_.install_source.empty()) { login_after_notes_=false; login_start(); }
}

void Session::dropdown_cancel()
{
    if(install_source_dialog_ && install_source_first_) {
        dropdown_open=false; dropdown_chosen_=nullptr;
        install_source_choose(2);
        return;
    }
    install_source_dialog_=false;
    if (update_dialog_) updater_.cancel();
    update_dialog_ = false;
    dropdown_open = false;
    dropdown_chosen_ = nullptr;
}

// ---------------------------------------------------------------------------
// Signing in with a code approved on another device

void Session::login_start()
{
    if (device_cancel_) *device_cancel_ = true;
    device_provider_.clear();
    const int gen = ++login_gen_;
    sign_in_open = true;
    sign_in = SignInState();
    sign_in.link = "nuvio.tv/link";
    sign_in.status = "Loading...";
    login_code_raw_.clear();
    login_qr_url_.clear();
    login_polling_ = false;
    bg<LinkCode>([]() { return link_create(); },
                 [this, gen](LinkCode &lc) {
                     if (gen != login_gen_ || !sign_in_open)
                         return;
                     if (!lc.ok)
                     {
                         sign_in.status = "Could not get a code: " + lc.error;
                         return;
                     }
                     login_code_raw_ = lc.code;
                     sign_in.code = lc.display_code;
                     std::string shown = lc.verification_uri;
                     if (starts_with(shown, "https://"))
                         shown = shown.substr(8);
                     sign_in.link = shown;
                     std::uint8_t temp[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
                     if (qrcodegen_encodeText(lc.link.c_str(), temp, qr, qrcodegen_Ecc_MEDIUM,
                         qrcodegen_VERSION_MIN, 20, qrcodegen_Mask_AUTO, true))
                     {
                         sign_in.qr_size = qrcodegen_getSize(qr);
                         sign_in.qr_modules.resize(sign_in.qr_size * sign_in.qr_size);
                         for (int y = 0; y < sign_in.qr_size; ++y)
                             for (int x = 0; x < sign_in.qr_size; ++x)
                                 sign_in.qr_modules[y * sign_in.qr_size + x] = qrcodegen_getModule(qr, x, y);
                     }
                     else
                         say("Use the displayed address and code to sign in", 6.0);
                     sign_in.status = "Waiting for approval on your phone...";
                     login_interval_ = lc.poll_interval;
                     login_next_poll_ = now_seconds() + login_interval_;
                     login_expires_ = now_seconds() + lc.expires_in;
                 });
}

void Session::login_poll()
{
    if (login_code_raw_.empty() || login_polling_ || now_seconds() < login_next_poll_)
        return;
    if (now_seconds() > login_expires_)
    {
        sign_in.status = "Sign-in timed out. Get a fresh code from Settings.";
        login_code_raw_.clear();
        return;
    }
    login_polling_ = true;
    const int gen = login_gen_;
    const std::string code = login_code_raw_;
    bg<ApiResult>([code]() { return link_read(code); },
                  [this, gen](ApiResult &r) {
                      if (gen != login_gen_ || !sign_in_open)
                          return;
                      login_polling_ = false;
                      login_next_poll_ = now_seconds() + login_interval_;
                      if (r.ok)
                          login_done(jstr(r.result, "authKey"));
                      else if (!r.error.empty())
                          sign_in.status = r.error;
                      if (jbool(r.result, "terminal"))
                      {
                          login_code_raw_.clear();
                          nuvio_pairing_cancel();
                      }
                  });
}

void Session::sign_in_cancel()
{
    if(sign_in.link_only) {sign_in_open=false;return;}
    ++login_gen_;
    nuvio_pairing_cancel();
    sign_in_open = false;
    login_code_raw_.clear();
    if (device_cancel_) *device_cancel_ = true;
    if (!device_provider_.empty()) {
        if (starts_with(device_provider_,"tracking:")) {
            const auto key=prefs_.auth_key, p=device_provider_.substr(9);
            bg<ApiResult>([key,p]{return api_tracking(key,"device_cancel",{{"provider",p}});},[](auto&){});
        }
        device_provider_.clear(); device_code_ = {}; return;
    }
    prefs_.onboarding_done = true;
    save_prefs();
}

void Session::login_done(const std::string &auth_key)
{
    sign_in_cancel();
    save_progress(); save_profile_settings();
    clear_profile_views();
    profile_ready_ = false;
    prefs_.auth_key = auth_key;
    prefs_.user_email.clear();
    save_prefs();
    say("Signed in");
    bg<ApiResult>([auth_key]() { return api_get_user(auth_key); },
                  [this](ApiResult &r) {
                      if (!r.ok)
                          return;
                      prefs_.user_email = jstr(r.result, "email");
                      save_prefs();
                      refresh_settings();
                      say("Signed in as " + prefs_.user_email);
                  });
    profile_ready_ = false;
    profiles_loading_ = false;
    profile_prompt_pending_ = true;
    board.status = "Choose a Nuvio profile to load your catalogs and history.";
    refresh_library(); refresh_addons(); refresh_settings();
}

void Session::sign_out()
{
    if (watching || resuming_) { say("Stop playback before signing out", 6.0); return; }
    const std::string key = prefs_.auth_key;
    bg<ApiResult>([key]() { return api_logout(key); }, [this](ApiResult& r) {
        if (!r.ok) { say(r.error, 8.0, true); return; }
        save_progress(); save_profile_settings();
        prefs_.auth_key.clear(); prefs_.user_email.clear();
        profile_apply(json{{"path", data_dir_ + "/guest"}, {"authKey", ""}, {"profile", json{{"name", "Guest"}}}});
        ++login_gen_;
        save_prefs();
        say("Signed out");
    });

}

// ---------------------------------------------------------------------------
// Frame

void Session::update(float dt, const Cursor &cursor)
{
    (void)dt;
    g_tasks.drain();
    provider_tasks_.drain();
    update_status();
    if (signed_in() && !nuvio_account_saved())
    {
        if (watching) watch_stop(false);
        save_progress(); save_profile_settings();
        prefs_.auth_key.clear(); prefs_.user_email.clear();
        profile_apply(json{{"path", data_dir_ + "/guest"}, {"authKey", ""}, {"profile", json{{"name", "Guest"}}}});
        save_prefs(); refresh_settings();
        say("Your Nuvio session expired. Sign in again in Settings.", 8.0, true);
    }
    if (profile_prompt_pending_ && !release_notes_open && !dropdown_open && !sign_in_open && !install_source_dialog_) {
        profile_prompt_pending_ = false; profiles_menu(true);
    }
    const double now = now_seconds();
    if (boot_done_ && signed_in() && !watching && !library_loading_ && now >= library_refresh_at_)
        load_library();
    // While the first screen is being got ready, more pictures go up to the GPU each frame.
    const bool arrived = tex_.pump(boot_done_ ? 3 : 12);

    if (now - clock_ > 1.0)
    {
        clock_ = now;
        clock = format_clock();
    }
    if (!boot_done_ && boot_check(now - boot_start_))
    {
        boot_done_ = true;
        dlog("boot: ready after %.1f s", now - boot_start_);
    }
    if (toast_open && now > toast_until_)
        toast_open = false;
    // Played from Continue Watching on the Board: the title's page was open behind it only to play.
    // When the video is over (and no next episode is lined up), it closes.
    if (resumed_from_board_ && !watching && !resuming_ && !autoplay_pending_)
    {
        resumed_from_board_ = false;
        close_detail();
    }
    if (sign_in_open && !sign_in.link_only) {
        if (device_provider_.empty()) login_poll();
        else if (starts_with(device_provider_, "tracking:")) tracker_login_poll();
        else service_login_poll();
    }
    poll_typing();
    if (watching)
    {
        watch_update();
        upload_video();
    }

    detail_preparation_update(cursor);

    // More of Discover when the cursor nears the end.
    constexpr int kDiscoverCols = 4;
    if (!disc_end_ && !disc_loading_ && !disc_items_.empty() &&
        cursor.discover + 2 * kDiscoverCols >= static_cast<int>(disc_items_.size()))
        discover_load(true);

    const bool moved = cursor.board_row != last_cursor_.board_row || cursor.board_col != last_cursor_.board_col ||
                       cursor.discover != last_cursor_.discover || cursor.library != last_cursor_.library ||
                       cursor.episode != last_cursor_.episode || cursor.search_row != last_cursor_.search_row ||
                       cursor.search_col != last_cursor_.search_col;
    last_cursor_ = cursor;
    if (arrived || moved || art_dirty_ || now - last_art_ > 1.0)
    {
        art_dirty_ = false;
        last_art_ = now;
        refresh_art(cursor);
    }
    if (tex_.trim(kTextureBudget))
        refresh_art(cursor);
}

// The app is ready to be shown when the Board has its rows and the first screen's pictures are on the
// GPU: the cards in the first two rows and the picture behind the first title. It does not wait for
// ever for what does not come: a catalog that never answers, a picture that cannot be fetched.
bool Session::boot_check(double t) const
{
    if (t > 10.0)
        return true;
    if (t < 1.8)
        return false; // the mark has its moment
    const bool answered = !addons_loading_ && !board_loading_;
    const std::size_t rows = board.rows.size();
    if (rows == 0)
        return answered; // nothing to wait for: the page tells what is wrong
    if (!answered && rows < 3)
        return false;
    if (t > 7.0)
        return true; // pictures that have not come by now come when they come
    for (std::size_t r = 0; r < rows && r < 2; ++r)
    {
        const std::vector<Item> &items = board_[static_cast<std::size_t>(board_map_[r])].items;
        for (std::size_t c = 0; c < items.size() && c < board.rows[r].items.size() && c < 8; ++c)
            if (!items[c].poster.empty() && board.rows[r].items[c].poster == 0)
                return false;
    }
    const Item &first = board_[static_cast<std::size_t>(board_map_[0])].items.front();
    if (!first.background.empty())
        return board.rows[0].items.front().backdrop != 0;
    // No picture yet: the addons are being asked for it.
    return !(meta_asked_.count(first.type + ":" + first.id) && !meta_cache_.count(first.type + ":" + first.id));
}

// ---------------------------------------------------------------------------
// Artwork: the textures for what is near the cursor; the rest only if they are
// already on the GPU.

void Session::refresh_art(const Cursor &cursor)
{
    const auto fetch = [&](const std::string &url, ArtKind kind, bool near) {
        return near ? tex_.get(url, kind) : tex_.peek(url, kind);
    };

    // Board: cards around the cursor, and the hero's picture for the focused title and its neighbours.
    for (std::size_t r = 0; r < board.rows.size() && r < board_map_.size(); ++r)
    {
        const std::vector<Item> &items = board_[static_cast<std::size_t>(board_map_[r])].items;
        const bool row_near = std::abs(static_cast<int>(r) - cursor.board_row) <= 2;
        for (std::size_t c = 0; c < board.rows[r].items.size() && c < items.size(); ++c)
        {
            Title &t = board.rows[r].items[c];
            const int col = static_cast<int>(c);
            t.poster = fetch(items[c].poster, ArtKind::poster, row_near && col < cursor.board_col + 12);
            const bool hero = static_cast<int>(r) == cursor.board_row && std::abs(col - cursor.board_col) <= 1;
            t.backdrop = fetch(items[c].background, ArtKind::backdrop, hero);
            // The library lists a title without its backdrop or story: ask the addons.
            if ((hero && (items[c].background.empty() || items[c].description.empty())) ||
                (row_near && col < cursor.board_col + 12 && (items[c].name.empty() || items[c].name == "Untitled")))
                want_meta(items[c]);
        }
    }

    // Search results: the same, without a hero.
    for (std::size_t r = 0; r < search.rows.size() && r < search_map_.size(); ++r)
    {
        const std::vector<Item> &items = search_[static_cast<std::size_t>(search_map_[r])].items;
        const bool row_near = std::abs(static_cast<int>(r) - cursor.search_row) <= 2;
        for (std::size_t c = 0; c < search.rows[r].items.size() && c < items.size(); ++c)
            search.rows[r].items[c].poster =
                fetch(items[c].poster, ArtKind::poster, row_near && static_cast<int>(c) < cursor.search_col + 12);
    }

    // Starting a stream: the title's picture behind the ring and in it.
    if (launch_open)
    {
        launch.backdrop = fetch(w_item_.background, ArtKind::backdrop, true);
        launch.poster = fetch(w_item_.poster, ArtKind::poster, true);
    }

    // Discover: posters in a window, and the preview picture of the focused title.
    for (std::size_t i = 0; i < discover.items.size() && i < disc_items_.size(); ++i)
    {
        Title &t = discover.items[i];
        const int d = static_cast<int>(i) - cursor.discover;
        t.poster = fetch(disc_items_[i].poster, ArtKind::poster, d > -12 && d < 24);
        t.backdrop = fetch(disc_items_[i].background, ArtKind::backdrop, d == 0);
    }

    // Library
    for (std::size_t i = 0; i < library.items.size() && i < lib_items_.size(); ++i)
    {
        const int d = static_cast<int>(i) - cursor.library;
        library.items[i].poster = fetch(lib_items_[i].poster, ArtKind::poster, d > -14 && d < 28);
    }

    // Addons
    for (std::size_t i = 0; i < addons.items.size() && i < addon_logos_.size(); ++i)
        addons.items[i].logo = fetch(addon_logos_[i], ArtKind::icon, true);

    // A title's page
    if (d_open_)
    {
        const std::string &background = d_meta_loaded_ && !d_meta_.background.empty() ? d_meta_.background : d_item_.background;
        detail.title.backdrop = fetch(background, ArtKind::backdrop, true);
        const std::string &poster = d_meta_loaded_ && !d_meta_.poster.empty() ? d_meta_.poster : d_item_.poster;
        detail.title.poster = fetch(poster, ArtKind::poster, !detail.series);
        for (std::size_t i = 0; i < detail.episodes.size() && i < d_season_videos_.size(); ++i)
            detail.episodes[i].thumb = fetch(d_season_videos_[i]->thumbnail.empty() ? background : d_season_videos_[i]->thumbnail, ArtKind::thumb,
                                             std::abs(static_cast<int>(i) - cursor.episode) < 10);
    }
}

} // namespace sx
