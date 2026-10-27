// The app's state and everything that goes out to the network for it: the
// account, the addons, the catalogs, the library, a title's details and
// streams. It turns all of that into the plain content the screens draw
// (ui/content.hpp) and changes it in place as answers arrive; the shell reads
// what changed and tells the screens.
//
// One thread owns it (the one that draws). Blocking work runs on the task
// pool and its answers are delivered back to that thread in update().

#pragma once

#include "art.hpp"
#include "app_update.hpp"
#include "episode_playback.hpp"
#include "content.hpp"
#include "ime.hpp"
#include "model.hpp"
#include "connected_services.h"
#include "connected_preparer.h"
#include "overlays.hpp"
#include "player.h"
#include "presentation_timing.hpp"
#include "console_lights.hpp"
#include "video_surface.hpp"
#include "subtitles.h"
#include "tasks.h"
#include "stream_result_cache.hpp"
#include "live_tv.h"
#include "iptv_library.h"
#include "audio/cues.hpp"
#include "core/input.hpp"

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace sx
{

// How a stream from an addon is listed: resolution badge, size, seeds, tags.
Stream stream_view(const ::Stream &stream);

class Session
{
  public:
    // Where the cursor is on each screen, so the artwork around it is fetched first.
    struct Cursor
    {
        int board_row = 0, board_col = 0;
        int discover = 0;
        int library = 0;
        int iptv = 0;
        int episode = 0;
        int search_row = 0, search_col = 0;
        int stream = -1;
        std::string stream_addon;
        bool streams_open = false;
    };

    // What changed since take_changed() last said. kDiscoverNew / kLibraryNew mean
    // the list is a different one (a filter changed): the screen starts at the top.
    enum Changed : unsigned
    {
        kBoard = 1,
        kDiscover = 2,
        kLibrary = 4,
        kAddons = 8,
        kSettings = 16,
        kLive = 2048,
        kIptv = 4096,
        kIptvNew = 8192,
        kDetail = 32,
        kDiscoverNew = 64,
        kLibraryNew = 128,
        kSearch = 256,
        kSearchNew = 512,
    };

    bool init(const std::string &base_dir, const std::string &data_dir, hui::gfx::Renderer *renderer);
    void shutdown();
    void complete_update_exit() { updater_.finish_exit(); }
    void update(float dt, const Cursor &cursor);
    void set_display_output(int width, int height, int refresh_hz) {
        output_w_ = width; output_h_ = height; output_hz_ = refresh_hz;
    }
    void set_display_configuration(unsigned width, unsigned height, bool fallback);
    void fail_video_output(const std::string& error);
    void poll_presentation_timing();
    void arm_presentation_timing();
    void mark_video_submitted() { if(video_texture) video_surface_.submitted();
        if(multiview_active)for(size_t i=0;i<4;++i)if(multiview_tiles[i].texture)multi_surfaces_[i].submitted(); }

    unsigned take_changed()
    {
        const unsigned c = changed_;
        changed_ = 0;
        return c;
    }
    // A page the app wants shown (after "See all" on the Board).
    bool take_goto(Page *page)
    {
        if (!goto_)
            return false;
        *page = *goto_;
        goto_.reset();
        return true;
    }
    // The sounds the screens should make, in the order things happened (the shell plays them).
    void cue(hui::audio::Cue c)
    {
        if (cues_.size() < 16)
            cues_.push_back(c);
    }
    std::vector<hui::audio::Cue> take_cues()
    {
        std::vector<hui::audio::Cue> v;
        v.swap(cues_);
        return v;
    }
    void set_console_light_status(ConsoleLights::Status status) {
        if(console_light_status_!=status) { console_light_status_=status; refresh_settings(); }
    }
    bool console_light_sync() const { return prefs_.console_light_sync; }
    bool reduced_motion() const { return prefs_.reduced_motion; }
    bool high_contrast() const { return prefs_.high_contrast; }
    bool calm_backgrounds() const { return prefs_.calm_backgrounds; }
    bool ui_sounds() const
    {
        return prefs_.ui_sounds;
    }
    bool wants_exit() const
    {
        return exit_;
    }

    bool release_notes_open = false;
    std::string release_notes_version;
    void release_notes_dismiss();

    // ---- what the screens draw ----
    BoardContent board;
    DiscoverContent discover;
    LibraryContent library;
    AddonsContent addons;
    AddonsContent live_content;
    LibraryContent iptv_content;
    bool iptv_visible = false;
    void iptv_menu();
    void iptv_search();
    void iptv_filter(int);
    void iptv_activate(int);
    bool iptv_back();
    struct MultiTileView { std::uint32_t texture=0; int w=0,h=0; double sar=1; std::string name,status; };
    bool multiview_active=false;
    int multiview_focus=0,multiview_audio=0;
    std::array<MultiTileView,4> multiview_tiles;
    void live_menu();
    void live_reload();
    void live_groups();
    void live_search();
    void live_cycle();
    void live_activate(int index, bool favorite = false);
    SettingsContent settings;
    DetailContent detail;
    BoardContent search; // the results of a search, rows like the Board's
    std::string search_query;
    bool search_open = false; // the results are on show
    std::string clock; // "21:45"

    // ---- playing ----
    // The launch screen while a stream starts, then the player over the video.
    bool launch_open = false;
    LaunchState launch;
    bool watching = false; // the player is up (also during the launch screen)
    PlayerState playing;
    std::uint32_t video_texture = 0; // the current picture, 0 until the first one
    bool video_hdr = false; // texture contains BT.2020 PQ, composed separately from the SDR UI
    int video_w = 0, video_h = 0;
    double video_sar = 1.0;
    // Render-surface size is not proof of the TV's HDMI signal resolution.
    int output_w_ = 0, output_h_ = 0, output_hz_ = 0;
    unsigned configured_w_ = 0, configured_h_ = 0;
    bool typing() const
    {
        return ime_.active() || pin_open;
    }

    // ---- pop-ups ----
    bool pin_open = false;
    PinEntry pin_entry;
    void profile_pin_input(const hui::InputFrame&);
    void profile_pin_cancel();
    bool dropdown_open = false;
    DropdownState dropdown;
    bool sign_in_open = false;
    SignInState sign_in;
    bool toast_open = false;
    ToastState toast;

    // ---- what the screens ask for ----
    void page_opened(Page page);
    // streams_video: for a series, the episode whose streams are shown at once (Continue Watching).
    void open_title(const Title &title, const std::string &streams_video = std::string());
    void close_detail();
    void detail_pick_episode(int index);
    void detail_change_season(int delta);
    void detail_toggle_watched(int index);
    void detail_play(int index, bool with_server);
    void detail_reload_streams();
    void detail_trailers();
    void cancel_autoplay()
    {
        autoplay_pending_ = false;
    }
    void filter_activate(Page page, int filter);
    void settings_activate(int row);
    void addons_reload();
    // Cross on an addon: one added by URL can be removed; the account's are managed in Nuvio.
    void addon_activate(int index, int provider_cursor = 0);
    void provider_options(const std::string &url);
    void provider_options_load(const std::string &url, const std::string &id);
    void provider_options_show(const std::string &url, const std::string &id, const json &layout);
    // Square on the Board: "See all" (a catalog opens in Discover, Continue Watching in the Library).
    void board_square(int row, int col);
    // Whether a row of the Board is Continue Watching.
    bool board_is_continue(int row) const;
    // Cross on a card of Continue Watching: what was playing plays again (after asking), or the
    // streams of the next episode are shown.
    void continue_press(int row, int col);
    // Continue Watching asks for a title's page to be opened (the shell does it).
    bool take_open(Title *title, std::string *streams_video)
    {
        if (!open_req_)
            return false;
        *title = open_req_->first;
        *streams_video = open_req_->second;
        open_req_.reset();
        return true;
    }
    // Options on a card of Continue Watching: a menu, then a question, before it is removed.
    // False when the card has no options (the caller then treats the button as search).
    bool board_options(int row, int col);
    // True once the first screen has its catalogs and the posters in view (or has waited long enough).
    bool boot_ready() const
    {
        return boot_done_;
    }
    void dropdown_confirm();
    void dropdown_cancel();
    void sign_in_cancel();
    // The buttons while a stream starts or plays.
    void watch_input(const hui::InputFrame &input);
    void media_transport(int action, int seek_direction);
    // Asks for a search with the console's keyboard.
    void begin_search();
    void start_search(const std::string &query); // searches for a text already typed
    void close_search();
    void say(const std::string &message, double seconds = 4.0, bool error = false);

  private:
    Tasks provider_tasks_;
    template <typename T> void bg(std::function<T()> work, std::function<void(T &)> done, bool urgent = false);

    // persistence and account
    void profiles_menu(bool startup = false);
    void profile_choose(int id, bool locked);
    void clear_profile_views();
    void profile_apply(const json& result);
    void load_profile_settings();
    void save_profile_settings();
    void sync_source(const std::string& url, bool plugin, bool remove, bool automatic = false);
    void load_prefs();
    void save_prefs();
    void load_progress();
    void save_progress();
    bool signed_in() const
    {
        return !prefs_.auth_key.empty();
    }
    void load_addons();
    void on_addons_loaded();
    void load_library();
    void load_watched();
    void sync_watched_mark(const std::string& video, const std::string& title, bool watched,
                           const std::string& content = "", int season = -1, int episode = -1);
    const ::Addon *find_addon(const std::string &url) const;
    std::vector<Item> continue_watching() const;

    // board
    void build_board();
    void refresh_board();
    void continue_remove(const Item &item);
    void resume_play(const Title &title, const Item &item);
    void start_remembered_stream();
    bool saved_stream(const std::string &item_id, const std::string &video_id, ::Stream *out) const;
    std::string next_episode_id() const;
    bool boot_check(double seconds) const;
    // discover
    void enter_discover();
    void discover_build_chips();
    void discover_load(bool more);
    // library
    void refresh_library();
    // addons, settings
    void refresh_addons();
    void refresh_settings();
    void live_reset();
    void live_play(const live_tv::Channel&);
    void multi_choose(std::vector<live_tv::Channel> chosen={}, int replace=-1);
    void multi_start(const std::vector<live_tv::Channel>&);
    void multi_open(int tile, bool audio);
    void multi_stop();
    void multi_update();
    void multi_input(const hui::InputFrame&);
    std::array<std::unique_ptr<Player>,4> multi_players_;
    std::array<VideoSurface,4> multi_surfaces_;
    std::array<live_tv::Channel,4> multi_channels_;

    void iptv_reset();
    void iptv_load();
    void iptv_sources_changed();
    void iptv_probe();
    void iptv_probe_source(size_t index, unsigned generation, unsigned profile, const std::shared_ptr<std::atomic<bool>>& cancel);
    std::vector<iptv::Account> iptv_probe_accounts_;
    void iptv_save();
    void iptv_build(bool reset=false);
    void iptv_reload(bool refresh=false);
    void iptv_progress(bool finished=false);
    double iptv_resume(const std::string&) const;
    void iptv_play(const iptv::Entry&);
    std::vector<iptv::Account> iptv_accounts_;
    json iptv_state_ = json::object();
    iptv::Catalog iptv_catalog_;
    std::vector<size_t> iptv_indices_;
    std::string iptv_source_,iptv_category_,iptv_query_,iptv_series_,iptv_series_name_,iptv_series_poster_,iptv_status_;
    iptv::Kind iptv_kind_ = iptv::Kind::movies;
    size_t iptv_page_=0;
    int iptv_season_=-1;
    bool iptv_loading_=false,iptv_probing_=false;
    unsigned iptv_generation_=0,iptv_probe_generation_=0;
    std::shared_ptr<std::atomic<bool>> iptv_cancel_;
    std::shared_ptr<std::atomic<bool>> iptv_probe_cancel_;
    void live_load();
    void live_save();
    void live_build();
    void live_add(json source, int step);
    json live_state_ = json::object();
    std::vector<live_tv::Channel> live_channels_;
    std::vector<size_t> live_indices_;
    std::shared_ptr<std::atomic<bool>> live_cancel_;
    std::string live_source_, live_group_, live_query_, live_filter_;
    size_t live_page_ = 0;
    unsigned live_generation_ = 0;
    int live_view_ = 0;
    bool live_loading_ = false;
    void open_dropdown(const std::string &title, const std::vector<std::string> &options, int active,
                       std::function<void(int)> chosen);
    void login_start();
    void login_poll();
    void login_done(const std::string &auth_key);
    void sign_out();
    void tracking_menu();
    void tracker_menu(const std::string& provider);
    void tracking_source_menu(bool progress);
    void tracking_refresh();
    void tracking_action(const std::string& action, const json& args, bool reload = false);
    void tracker_login_start(const std::string& provider);
    void tracker_login_poll();
    void services_menu();
    void animeskip_menu();
    void service_menu(const std::string& provider);
    bool services_save(const debrid::Settings& next);
    void services_import();
    void service_login_start();
    void service_login_poll();
    void detail_preparation_cancel();
    void detail_preparation_update(const Cursor& cursor);
    void detail_preparation_labels();
    std::pair<int, int> detail_episode_identity() const;
    void next_preparation_cancel();
    void next_preparation_update();
    // detail
    void detail_apply_meta();
    void detail_set_season(int index);
    void detail_load_streams(const std::string &video_id, const std::string &heading,
                             const std::vector<::Stream>* prefetched = nullptr);
    void detail_refresh_streams();
    void detail_fill_title();
    bool watched(const std::string &video_id) const;
    // artwork
    void refresh_art(const Cursor &cursor);

    // search and typing
    void ask_text(const std::function<void(const std::string &)> &done, const std::string& title = "");
    void refresh_search();
    void poll_typing();

    // playing (session_watch.cpp)
    struct WatchSub
    {
        std::string label, lang;
        int embedded = -1;      // stream index in the file, or
        std::string url;        // an addon's subtitle file
        std::vector<Cue> cues;  // once loaded
        int rich_id=-1;
        std::string rich_text; // retain external ASS/text through quick-input reopen on seek
        bool is_default=false, forced=false, hearing_impaired=false;
        bool loading = false, failed = false;
    };
    void play_stream(const ::Stream &stream, bool transcode, bool trailer=false);
    void watch_start(const std::string &url, const std::vector<std::string> &headers, double start,
                     const std::string &title, const std::string &subtitle, bool direct);
    void watch_update();
    void watch_stop(bool ended);
    void watch_save_progress(bool final, bool force_finished = false);
    void watch_show_info();
    void watch_build_menu();
    void watch_select_sub(int index);
    void watch_load_addon_subtitles();
    void watch_auto_subtitles();
    void watch_next_episode();
    void watch_load_segments(int season, int episode);
    void watch_load_anime_segments(double duration);
    void watch_reset_anime();
    void watch_merge_segments();
    void watch_prefetch_next();
    bool watch_episode_update(double now, double position, double duration, bool advancing, bool ended = false);
    bool watch_episode_input(const hui::InputFrame &input);
    void upload_video();
    void torrent_stats_start(const std::string &url, const std::string &info_hash);
    void torrent_stats_poll();
    std::vector<std::string> pref_langs(const std::string &list) const;
    std::string server() const;

    Title make_title(const Item &item, bool with_progress) const;
    // The library's items carry no backdrop or description: both are asked of the
    // addons for the title under the cursor, kept here and filled in everywhere.
    void apply_meta(Item &item) const;
    void want_meta(const Item &item);

    std::string base_dir_, data_dir_, profile_dir_;
    json profiles_ = json::array(), active_profile_ = json::object();
    std::set<std::string> source_attempted_;
    unsigned profile_generation_ = 0, watched_revision_ = 0;
    bool profile_ready_ = false, profiles_loading_ = false, profile_prompt_pending_ = false;
    std::function<void(const std::string&)> profile_pin_submit_;
    bool inherit_addons_ = false, inherit_plugins_ = false;

    hui::gfx::Renderer *renderer_ = nullptr;
    TexCache tex_;
    Prefs prefs_;
    std::map<std::string, Progress> progress_;
    std::set<std::string> watched_;
    unsigned changed_ = 0;
    Page page_ = Page::board; // the page on show
    std::optional<Page> goto_;
    bool exit_ = false;
    AppUpdate updater_;
    AppUpdate::Phase update_phase_ = AppUpdate::Phase::idle;
    bool update_dialog_ = false;
    bool install_source_dialog_ = false, install_source_first_ = false;
    bool startup_update_checked_ = false, background_update_check_ = false;
    std::string notified_update_;
    void install_source_menu(bool first = false);
    void install_source_choose(int index);
    void update_menu();
    void update_status();
    std::vector<hui::audio::Cue> cues_;
    std::optional<std::pair<Title, std::string>> open_req_;
    std::string want_streams_video_; // open_title: show these streams as soon as the details are in
    bool resume_play_ = false;       // open_title: play the remembered stream as soon as the details are in
    bool resuming_ = false;          // the launch screen is up before play_stream has been called
    bool resumed_from_board_ = false; // the title's page is open behind the Board only for playing
    std::string resume_video_;
    double clock_ = 0;
    Cursor last_cursor_;
    double last_art_ = 0;
    bool art_dirty_ = true;
    double toast_until_ = 0;

    // addons and library
    std::vector<std::unique_ptr<::Addon>> addons_;
    bool addons_loading_ = false;
    bool board_loading_ = false; // some catalog of the Board has not answered yet
    bool boot_done_ = false;
    double boot_start_ = 0.0;
    int addons_gen_ = 0;
    std::map<std::string, json> library_; // library items by _id
    bool library_loaded_ = false;
    bool library_loading_ = false;
    double library_refresh_at_ = 0;

    // details of titles the library listed without them, by "type:id"
    std::map<std::string, Item> meta_cache_;
    std::set<std::string> meta_asked_;

    // board
    std::vector<BoardRow> board_;
    std::vector<int> board_map_; // content row -> board_ index
    int home_gen_ = 0;

    // discover
    struct DiscCatalog
    {
        const ::Addon *addon;
        const Catalog *catalog;
    };
    std::vector<std::string> disc_types_;
    std::string disc_type_;
    std::vector<DiscCatalog> disc_catalogs_;
    int disc_catalog_ = 0;
    std::string disc_genre_;
    std::vector<Item> disc_items_;
    bool disc_loading_ = false, disc_end_ = false;
    int disc_gen_ = 0;

    // library
    std::vector<Item> lib_items_;
    std::string lib_type_;
    int lib_sort_ = 0; // 0 recent, 1 A-Z, 2 most watched

    // dropdown
    std::function<void(int)> dropdown_chosen_;

    // sign-in
    std::string login_code_raw_, login_qr_url_;
    double login_next_poll_ = 0, login_expires_ = 0;
    bool login_polling_ = false;
    int login_interval_ = 3;
    int login_gen_ = 0;
    bool confirm_sign_out_ = false;
    json tracking_status_ = json::object();
    int tracking_generation_ = 0;
    int library_generation_ = 0;
    std::string progress_file_ = "progress.json";
    debrid::Settings services_;
    std::shared_ptr<debrid::PreparedLinks> prepared_ = std::make_shared<debrid::PreparedLinks>();
    int services_generation_ = 0;
    bool services_importing_ = false;
    std::string device_provider_;
    debrid::DeviceCode device_code_;
    std::shared_ptr<std::atomic<bool>> device_cancel_;

    // detail
    Item d_item_;
    ::Meta d_meta_;
    bool d_meta_loaded_ = false;
    std::vector<int> d_seasons_;
    int d_season_idx_ = 0;
    std::vector<const ::Video *> d_season_videos_;
    std::string d_video_id_;
    std::string d_stream_heading_;
    std::vector<::Stream> d_streams_; // in the order the screen shows them
    std::vector<::Stream> d_found_;   // as the addons sent them
    bool d_cache_warm_=false;
    int d_gen_ = 0, d_pending_ = 0, d_answered_ = 0, d_meta_gen_ = 0;
    StreamResultCache stream_result_cache_;
    std::string d_stream_cache_key_;
    std::set<std::string> d_cached_sources_;
    bool d_stream_errors_ = false;
    bool d_open_ = false;
    std::shared_ptr<std::atomic<bool>> d_stream_cancel_ = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> d_cancel_ = std::make_shared<std::atomic<bool>>(false);
    debrid::PreparedLinks::Scope d_prepare_scope_ = 0;
    std::string d_prepare_identity_, d_prepare_addon_;
    double d_prepare_at_ = 0, d_prepare_labels_at_ = 0;
    bool d_prepare_busy_ = false;

    // search
    Ime ime_;
    std::function<void(const std::string &)> text_done_;
    std::vector<BoardRow> search_;
    std::vector<int> search_map_;
    int search_gen_ = 0, search_pending_ = 0;

    // playing
    ConsoleLights::Status console_light_status_=ConsoleLights::Status::idle;
    Player player_;
    PresentationTiming::Picture timing_picture_;
    uint64_t timing_picture_id_=0;
    GLuint subtitle_texture_=0;
    int subtitle_width_=0, subtitle_height_=0;
    VideoSurface video_surface_;
    bool w_trailer_=false, d_trailer_loading_=false;
    Item w_item_;
    std::string w_video_id_;
    ::Stream w_stream_;
    int w_season_ = -1, w_episode_number_ = -1;
    std::vector<WatchSub> w_subs_;
    int w_sub_active_ = -1; // index in w_subs_, -1 off
    double w_sub_delay_ = 0;
    double w_info_until_ = 0, w_last_save_ = 0, w_last_saved_pos_ = -1;
    bool w_started_ = false, w_menu_built_ = false, w_auto_sub_done_ = false, w_heavy_warned_ = false;
    bool w_menu_dirty_ = false;
    double w_stats_at_ = 0, w_stats_logged_ = 0;
    int w_gen_ = 0;
    std::shared_ptr<std::atomic<bool>> launch_cancel_;
    std::string subtitle_shown_;
    // the torrent's statistics while it starts or buffers
    std::string t_stats_url_;
    int t_gen_ = 0;
    double t_next_poll_ = 0;
    bool t_polling_ = false, t_visible_ = false;
    int t_peers_ = 0, t_known_ = -1;
    double t_speed_ = 0, t_progress_ = -1;
    bool t_has_meta_ = false;
    // the next episode, picked once its streams are in
    bool autoplay_pending_ = false;
    ::Stream autoplay_previous_;
    episode_playback::Flow w_episode_;
    episode_playback::AnimeIds w_anime_;
    long w_aniskip_requested_ = 0, w_animeskip_requested_ = 0;
    double w_timing_duration_ = 0;
    std::vector<episode_playback::Segment> w_introdb_segments_, w_aniskip_segments_, w_animeskip_segments_;
    std::shared_ptr<std::atomic<bool>> w_animeskip_cancel_;
    std::optional<::Video> w_next_;
    std::vector<::Stream> w_next_streams_;
    int w_next_pending_ = 0, w_next_answered_ = 0, w_next_generation_ = 0;
    int w_next_pick_ = -1;
    double w_next_fetched_ = 0, w_episode_tick_ = 0;
    bool login_after_notes_ = false;
    bool w_eof_recorded_ = false;
    bool w_next_requested_ = false;
    std::shared_ptr<std::atomic<bool>> w_episode_cancel_;
    debrid::PreparedLinks::Scope w_next_prepare_scope_ = 0;
    double w_next_prepare_at_ = 0;
    bool w_next_prepare_busy_ = false;

    // "See all" on the Board: which catalog Discover should open on.
    std::string see_all_addon_, see_all_catalog_, see_all_type_;
    std::vector<std::string> addon_logos_;
};

template <typename T> void Session::bg(std::function<T()> work, std::function<void(T &)> done, bool urgent)
{
    const auto generation = profile_generation_;
    g_tasks.run<T>(std::move(work), [this, generation, done = std::move(done)](T& result) {
        if (generation == profile_generation_) done(result);
    }, urgent);
}

} // namespace sx
