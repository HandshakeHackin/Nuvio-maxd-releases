#pragma once
#include "../gl/model.hpp"
#include "../ui/content.hpp"
#include "../ui/overlays.hpp"
#include "../src/torrent/engine.h"
#include "../src/util.h"
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sx {

inline bool load_json(const std::string& path, json& out) {
    std::string raw;
    if (!read_file(path, raw) || raw.empty()) return false;
    out = json::parse(raw, nullptr, false);
    return !out.is_discarded();
}

inline bool save_json(const std::string& path, const json& j) {
    return write_file(path, j.dump(2));
}

struct Session {
    void diagnostic_menu();void diagnostic_qr();void watch_fail(const std::string&);
    void watch_stop(bool){watching=false;++w_gen_;}
    void detail_load_streams(const std::string& video,const std::string&){requested_video=video;}
    std::string requested_video;
    int w_gen_=0,services_generation_=0,media_generation_=0;
    bool w_trailer_=false,w_transcode_=false,start_over_next_=false,d_open_=false;
    ::Stream w_stream_;std::vector<::Stream> d_streams_;
    enum {kDetail=2};
    void torrent_set_cache(int gb);
    void torrent_poll_drop();
    bool torrent_drop_open_=false,profile_ready_=true;
    int profile_generation_=0;
    void torrent_reset();
    void torrent_load();
    void torrent_save();
    void torrent_build();
    void torrent_progress(bool finished = false);
    double torrent_resume(const std::string &id) const;
    void torrent_rescan();
    void torrent_cycle();
    void torrent_search();
    void torrent_show_web_drop_qr();
    void torrent_menu(bool settings_only = false);
    void torrent_activate(int index, bool secondary = false);
    void torrent_open_input(const std::string &input, bool force_file_picker = false);
    bool torrent_try_search_input(const std::string &query);
    void torrent_play_hash(const std::string &info_hash, const std::string &name,
                           const std::vector<std::string> &trackers,
                           const std::vector<std::string> &webseeds, int file_idx, bool from_beginning = false);
    void torrent_present_files(const std::string &info_hash, const std::string &name,
                               const std::vector<bt::FileInfo> &files,
                               const std::vector<std::string> &trackers,
                               const std::vector<std::string> &webseeds,
                               int preferred_idx, bool force_picker);

    json torrent_state_ = json::object();
    std::vector<bt::LocalTorrentFile> torrent_local_files_;
    struct TorrentEntry {
        bool is_local_file = false;
        std::string id;
        std::string name;
        std::string path_or_uri;
        std::string subtitle;
        std::vector<std::string> trackers;
        std::vector<std::string> webseeds;
    };
    std::vector<TorrentEntry> torrent_entries_;
    std::string torrent_query_;
    int torrent_view_ = 0;
    std::shared_ptr<std::atomic<bool>> torrent_cancel_;

    AddonsContent torrents_content;
    bool torrents_visible = true;
    enum { kTorrentPage = 65536 };
    unsigned changed_ = 0;
    std::string profile_dir_, data_dir_;
    Prefs prefs_;
    bool watching = false, resuming_ = false, d_meta_loaded_ = false;
    Item d_item_, w_item_;
    ::Meta d_meta_;
    std::string d_video_id_;
    DetailContent detail;
    SignInState sign_in;
    bool sign_in_open = false;
    struct { std::string message; } dropdown;
    struct {
        double time = 42.0, duration_value = 120.0;
        double position() const { return time; }
        double duration() const { return duration_value; }
    } player_;

    std::vector<std::string> choices, messages;
    std::string dropdown_title;
    std::function<void(int)> pick;
    std::function<void(const std::string &)> text_cb;
    std::vector<::Stream> played;

    template <typename T>
    void bg(std::function<T()> work, std::function<void(T &)> done, bool = false) {
        T r = work();
        done(r);
    }

    void open_dropdown(const std::string &title, const std::vector<std::string> &options, int,
                       std::function<void(int)> chosen) {
        dropdown_title=title;
        choices = options;
        pick = std::move(chosen);
    }
    void ask_text(const std::function<void(const std::string &)> &done, const std::string & = "") {
        text_cb = done;
    }
    void say(const std::string &msg, double = 4.0, bool = false) {
        messages.push_back(msg);
    }
    void save_prefs() {}
    void refresh_settings() {}
    void cancel_autoplay() {}
    void next_preparation_cancel() {}
    void close_detail() {}
    void play_stream(const ::Stream &st, bool, bool = false) {
        played.push_back(st);
        w_item_ = d_item_;
        watching = true;
    }
};

} // namespace sx
