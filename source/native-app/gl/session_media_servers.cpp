// Experimental MediaBrowser libraries. Credentials and watch state belong to
// the active profile; addon discovery and Nuvio/tracker source preferences stay
// intact.
#include "session.hpp"
#include <algorithm>
#include <cmath>
namespace sx {
namespace {
std::string connection_id() {
  return "maxd-" + std::to_string(now_epoch_ms()) + "-" +
         std::to_string(std::rand());
}
bool container(const json &j) {
  auto t = jstr(j, "Type");
  return t == "Series" || t == "Season" || t == "CollectionFolder" ||
         t == "Folder" || t == "BoxSet";
}
} // namespace
void Session::media_reset() {
  if (device_provider_ == "plex") {
    media_plex_cancel();
    device_provider_.clear();
    sign_in_open = false;
    ++login_gen_;
  }
  media_saved_connections_ = json::array();
  if (watching && w_item_.type == "media-video")
    watch_stop(false);
  media_play_account_ = {};
  media_playback_ = {};
  media_play_item_.clear();
  media_report_started_ = false;
  if (media_cancel_)
    *media_cancel_ = true;
  if (media_play_cancel_)
    *media_play_cancel_ = true;
  ++media_generation_;
  ++media_play_generation_;
  media_play_pending_ = false;
  media_accounts_.clear();
  media_items_ = json::array();
  media_stack_.clear();
  media_location_ = {};
  media_account_ = 0;
  media_total_ = 0;
  media_loading_ = false;
  media_error_.clear();
  media_visible = false;
  media_content = {};
  {
    std::lock_guard<std::mutex> l(media_art_->mutex);
    media_art_->accounts.clear();
  }
  changed_ |= kMedia | kMediaNew;
}
void Session::media_load() {
  media_reset();
  auto saved = iptv::load_private(profile_dir_ + "/media-servers.json");
  auto a = jobj(saved, "connections");
  if (a.is_array())
    for (const auto &j : a) {
      auto c = media_server::Connection::restore(j);
      if (c.valid() && media_accounts_.size() < 8 &&
          std::none_of(media_accounts_.begin(), media_accounts_.end(),
                       [&](const auto &x) { return x.id == c.id; }))
        media_accounts_.push_back(c);
    }
  media_saved_connections_ = json::array();
  for (const auto &c : media_accounts_)
    media_saved_connections_.push_back(c.saved());
  media_visible = !media_accounts_.empty();
  {
    std::lock_guard<std::mutex> l(media_art_->mutex);
    media_art_->accounts = media_accounts_;
  }
  media_build(true);
}
void Session::media_save() {
  if (!profile_ready_)
    return;
  json a = json::array();
  for (const auto &c : media_accounts_)
    a.push_back(c.saved());
  if (!iptv::save_private(profile_dir_ + "/media-servers.json",
                          {{"connections", a}}))
    say("Could not save media server connection", 7, true);
  else
    media_saved_connections_ = a;
  media_visible = !media_accounts_.empty();
  {
    std::lock_guard<std::mutex> l(media_art_->mutex);
    media_art_->accounts = media_accounts_;
  }
  refresh_settings();
}
void Session::media_connect(const std::string &kind,
                            const std::string &replace_id) {
  if (kind == "plex") {
    media_plex_start(replace_id);
    return;
  }
  if (!profile_ready_) {
    say("Select a Nuvio profile first", 6);
    return;
  }
  if (replace_id.empty() && media_accounts_.size() >= 8) {
    say("Remove a connection before adding another server (maximum 8)", 6);
    return;
  }
  const auto profile = profile_generation_;
  ask_text(
      [this, profile, kind, replace_id](const std::string &base) {
        if (profile != profile_generation_ || base.empty())
          return;
        if (media_server::normalize(base, kind).empty()) {
          say("Enter an HTTP or HTTPS server address without a query or "
              "embedded credentials",
              8, true);
          return;
        }
        ask_text(
            [this, profile, kind, base, replace_id](const std::string &user) {
              if (profile != profile_generation_ || user.empty())
                return;
              ask_text(
                  [this, profile, kind, base, user,
                   replace_id](const std::string &password) {
                    if (profile != profile_generation_)
                      return;
                    if (media_cancel_)
                      *media_cancel_ = true;
                    media_cancel_ = std::make_shared<std::atomic<bool>>(false);
                    auto cancel = media_cancel_;
                    const auto generation = ++media_generation_;
                    media_server::Connection c;
                    c.id = connection_id();
                    c.device = c.id;
                    c.kind = kind;
                    c.base = base;
                    say("Connecting to " +
                            std::string(kind == "emby" ? "Emby" : "Jellyfin") +
                            "…",
                        5);
                    struct Auth {
                      media_server::Connection c;
                      media_server::Result r;
                    };
                    g_tasks.run<Auth>(
                        [c, user, password, cancel] {
                          Auth a;
                          a.c = c;
                          a.r = media_server::authenticate(a.c, user, password,
                                                           cancel.get());
                          a.r.data = {};
                          return a;
                        },
                        [this, profile, generation, cancel,
                         replace_id](Auth &a) {
                          if (profile != profile_generation_ ||
                              generation != media_generation_ || cancel->load())
                            return;
                          if (!a.r.ok()) {
                            say(a.r.error, 9, true);
                            return;
                          }
                          if (!replace_id.empty() && watching &&
                              w_item_.type == "media-video" &&
                              media_play_account_.id == replace_id)
                            watch_stop(false);
                          auto existing = std::find_if(
                              media_accounts_.begin(), media_accounts_.end(),
                              [&](const auto &old) {
                                return old.id == replace_id;
                              });
                          if (existing == media_accounts_.end()) {
                            media_accounts_.push_back(std::move(a.c));
                            media_account_ = media_accounts_.size() - 1;
                          } else {
                            media_account_ =
                                size_t(existing - media_accounts_.begin());
                            *existing = std::move(a.c);
                          }
                          media_location_ = {};
                          media_stack_.clear();
                          media_save();
                          media_reload();
                          goto_ = Page::servers;
                          say("Media server connected", 5);
                        });
                  },
                  "Server password (used only to sign in)", true);
            },
            "Server username");
      },
      std::string(kind == "emby" ? "Emby" : "Jellyfin") +
          " server URL (include port and base path)");
}
void Session::media_menu() {
  const auto profile = profile_generation_;
  std::vector<std::string> labels{"Connect Jellyfin (experimental)",
                                  "Connect Emby (experimental)",
                                  "Connect Plex (experimental)"};
  for (const auto &a : media_accounts_)
    labels.push_back(a.server_name + " · " + a.user_name);
  open_dropdown("Media servers", labels, 0, [this, profile](int n) {
    if (profile != profile_generation_ || n < 0)
      return;
    if (n < 3) {
      media_connect(n == 2 ? "plex" : n ? "emby" : "jellyfin");
      return;
    }
    size_t index = size_t(n - 3);
    if (index >= media_accounts_.size())
      return;
    const auto id = media_accounts_[index].id;
    open_dropdown(
        "Media server connection",
        {"Browse libraries", "Continue watching", "Search server",
         "Refresh current page", "Next page", "Previous page",
         "Disconnect and remove", "Reconnect / sign in again"},
        0, [this, profile, id](int action) {
          if (profile != profile_generation_ || action < 0)
            return;
          auto it = std::find_if(media_accounts_.begin(), media_accounts_.end(),
                                 [&](const auto &c) { return c.id == id; });
          if (it == media_accounts_.end())
            return;
          auto index = size_t(it - media_accounts_.begin());
          if (media_account_ != index) {
            media_stack_.clear();
            media_location_ = {};
          }
          media_account_ = index;
          if (action == 7) {
            media_connect(it->kind, it->id);
            return;
          }
          if (action == 6) {
            auto c = *it;
            open_dropdown(
                "Remove this profile's server connection?",
                {"Keep connection", "Disconnect and remove"}, 0,
                [this, profile, id, c](int confirm) {
                  if (confirm != 1 || profile != profile_generation_)
                    return;
                  if (watching && w_item_.type == "media-video" &&
                      media_play_account_.id == id)
                    watch_stop(false);
                  if (media_cancel_)
                    *media_cancel_ = true;
                  if (media_play_cancel_)
                    *media_play_cancel_ = true;
                  ++media_generation_;
                  ++media_play_generation_;
                  media_play_pending_ = false;
                  media_accounts_.erase(
                      std::remove_if(media_accounts_.begin(),
                                     media_accounts_.end(),
                                     [&](const auto &a) { return a.id == id; }),
                      media_accounts_.end());
                  media_account_ = 0;
                  media_location_ = {};
                  media_stack_.clear();
                  media_items_ = json::array();
                  media_save();
                  media_build(true);
                  if (c.kind != "plex")
                    g_tasks.run([c] {
                      json body = json::object();
                      media_server::request(c, "/Sessions/Logout", &body);
                    });
                  if (media_visible)
                    media_reload();
                });
            return;
          }
          if (action == 0) {
            media_location_ = {};
            media_stack_.clear();
          }
          if (action == 1) {
            media_location_ = {};
            media_location_.type = "resume";
            media_location_.label = "Continue watching";
            media_stack_.clear();
          }
          if (action == 2) {
            media_search();
            return;
          }
          if (action == 4) {
            if (media_location_.offset + media_items_.size() >= media_total_)
              return;
            media_location_.offset += 100;
          }
          if (action == 5)
            media_location_.offset = media_location_.offset >= 100
                                         ? media_location_.offset - 100
                                         : 0;
          media_reload();
          goto_ = Page::servers;
        });
  });
}
void Session::media_build(bool reset) {
  media_content = {};
  media_content.heading = "Media Servers";
  media_content.notice = "Experimental · Your server libraries and watch "
                         "progress · Options: connections and pages";
  if (media_account_ < media_accounts_.size()) {
    const auto &c = media_accounts_[media_account_];
    media_content.heading = c.server_name.empty()
                                ? (c.kind == "plex"   ? "Plex"
                                   : c.kind == "emby" ? "Emby"
                                                      : "Jellyfin")
                                : c.server_name;
    media_content.heading +=
        " · " +
        (media_location_.label.empty() ? "Libraries" : media_location_.label);
  }
  media_content.filters = {{"Server", "Choose"},
                           {"Browse", "Libraries"},
                           {"Continue watching", "Resume"}};
  for (const auto &j : media_items_) {
    Title t;
    t.id = jstr(j, "Id");
    t.name = jstr(j, "Name");
    t.type = "media-item";
    auto year = jnum(j, "ProductionYear");
    if (year > 0)
      t.year = std::to_string(int(year));
    t.badge = jstr(j, "Type");
    if (jstr(j, "Type") == "Episode")
      t.badge = "S" + std::to_string(int(jnum(j, "ParentIndexNumber"))) + " E" +
                std::to_string(int(jnum(j, "IndexNumber")));
    auto u = jobj(j, "UserData");
    t.watched = jbool(u, "Played");
    double dur = jnum(j, "RunTimeTicks");
    if (dur > 0 && !t.watched)
      t.progress =
          float(std::clamp(jnum(u, "PlaybackPositionTicks") / dur, 0.0, 1.0));
    media_content.items.push_back(std::move(t));
  }
  media_content.status =
      media_loading_ ? "Loading media server…"
      : media_error_.empty()
          ? "No items here. Open Options to choose a server, search or refresh."
          : media_error_;
  media_content.count = std::to_string(media_total_) + " items · Page " +
                        std::to_string(media_location_.offset / 100 + 1);
  changed_ |= kMedia;
  if (reset)
    changed_ |= kMediaNew;
  art_dirty_ = true;
}
void Session::media_reload() {
  if (!profile_ready_ || media_account_ >= media_accounts_.size())
    return;
  if (media_cancel_)
    *media_cancel_ = true;
  media_cancel_ = std::make_shared<std::atomic<bool>>(false);
  auto cancel = media_cancel_;
  auto c = media_accounts_[media_account_];
  auto loc = media_location_;
  const auto generation = ++media_generation_, profile = profile_generation_;
  media_loading_ = true;
  media_error_.clear();
  media_items_ = json::array();
  media_build(true);
  g_tasks.run<media_server::Result>(
      [c, loc, cancel, client = media_plex_client_] {
        if (c.kind == "plex")
          return client.browse(c, loc.parent, loc.type, loc.query, loc.offset,
                               cancel.get());
        return media_server::browse(c, loc.parent, loc.type, loc.query,
                                    loc.offset, cancel.get());
      },
      [this, profile, generation, cancel](media_server::Result &r) {
        if (profile != profile_generation_ || generation != media_generation_ ||
            cancel->load())
          return;
        media_loading_ = false;
        media_error_ = r.error;
        if (r.ok()) {
          for (const auto &item : r.data["Items"])
            if (media_server::safe_id(jstr(item, "Id")))
              media_items_.push_back(item);
          media_total_ = size_t(std::clamp(
              jnum(r.data, "TotalRecordCount", double(media_items_.size())),
              0.0, 1000000.0));
        } else
          media_total_ = 0;
        media_build(true);
      });
}
void Session::media_filter(int n) {
  if (n == 0) {
    media_menu();
    return;
  }
  if (media_account_ >= media_accounts_.size())
    return;
  media_stack_.clear();
  media_location_ = {};
  if (n == 2) {
    media_location_.type = "resume";
    media_location_.label = "Continue watching";
  }
  media_reload();
}
void Session::media_search() {
  if (media_account_ >= media_accounts_.size()) {
    media_menu();
    return;
  }
  auto profile = profile_generation_;
  auto id = media_accounts_[media_account_].id;
  ask_text(
      [this, profile, id](const std::string &q) {
        if (profile != profile_generation_ ||
            media_account_ >= media_accounts_.size() ||
            media_accounts_[media_account_].id != id)
          return;
        if (q.size() > 256) {
          say("Search is too long", 5);
          return;
        }
        media_stack_.push_back(media_location_);
        media_location_ = {};
        media_location_.type = "items";
        media_location_.query = q;
        media_location_.label = q.empty() ? "All titles" : "Search: " + q;
        media_reload();
        goto_ = Page::servers;
      },
      "Search this media server");
}
bool Session::media_back() {
  if (media_play_pending_) {
    if (media_play_cancel_)
      *media_play_cancel_ = true;
    ++media_play_generation_;
    media_play_pending_ = false;
    say("Cancelled media server playback", 4);
    return true;
  }
  if (media_stack_.empty())
    return false;
  media_location_ = media_stack_.back();
  media_stack_.pop_back();
  media_reload();
  return true;
}
void Session::media_activate(int n) {
  if (media_loading_ || n < 0 || size_t(n) >= media_items_.size() ||
      media_account_ >= media_accounts_.size())
    return;
  auto item = media_items_[size_t(n)];
  if (container(item)) {
    media_stack_.push_back(media_location_);
    media_location_ = {};
    media_location_.parent = jstr(item, "Id");
    media_location_.type =
        jstr(item, "Type") == "Series" ? "episodes" : "items";
    media_location_.label = jstr(item, "Name");
    media_reload();
    return;
  }
  auto profile = profile_generation_;
  auto id = media_accounts_[media_account_].id;
  std::vector<std::string> labels{"Play / resume", "Play from beginning",
                                  "Compatible playback (server HLS)"};
  std::vector<std::string> sources;
  auto a = jobj(item, "MediaSources");
  if (a.is_array())
    for (const auto &v : a)
      if (media_server::safe_id(jstr(v, "Id"))) {
        labels.push_back("Version: " + jstr(v, "Name", jstr(v, "Container")));
        sources.push_back(jstr(v, "Id"));
      }
  open_dropdown(jstr(item, "Name"), labels, 0,
                [this, profile, id, item, sources](int action) mutable {
                  if (profile != profile_generation_ || action < 0 ||
                      media_account_ >= media_accounts_.size() ||
                      media_accounts_[media_account_].id != id)
                    return;
                  if (action == 1)
                    item["UserData"]["PlaybackPositionTicks"] = 0;
                  media_play(item, action == 2,
                             action >= 3 && size_t(action - 3) < sources.size()
                                 ? sources[size_t(action - 3)]
                                 : "");
                });
}
void Session::media_play(const json &item, bool compatible,
                         const std::string &source) {
  if (watching || media_play_pending_ ||
      media_account_ >= media_accounts_.size())
    return;
  auto c = media_accounts_[media_account_];
  auto id = jstr(item, "Id");
  if (!media_server::safe_id(id))
    return;
  media_play_pending_ = true;
  if (media_play_cancel_)
    *media_play_cancel_ = true;
  media_play_cancel_ = std::make_shared<std::atomic<bool>>(false);
  auto cancel = media_play_cancel_;
  auto generation = ++media_play_generation_, profile = profile_generation_;
  say("Preparing server playback… Circle cancels.", 5);
  g_tasks.run<media_server::Playback>(
      [c, id, source, compatible, cancel, client = media_plex_client_] {
        if (c.kind == "plex")
          return client.playback(c, id, source, compatible, cancel.get());
        return media_server::playback(c, id, source, compatible, cancel.get());
      },
      [this, c, id, item, profile, generation,
       cancel](media_server::Playback &p) {
        if (profile != profile_generation_ ||
            generation != media_play_generation_ || cancel->load())
          return;
        media_play_pending_ = false;
        if (!p.error.empty()) {
          say(p.error, 10, true);
          return;
        }
        cancel_autoplay();
        next_preparation_cancel();
        close_detail();
        d_item_ = {};
        d_item_.id = "media:" + c.id + ":" + id;
        d_item_.type = "media-video";
        d_item_.name = jstr(item, "Name");
        d_meta_ = {};
        d_meta_loaded_ = false;
        d_video_id_ = id;
        detail = {};
        media_play_account_ = c;
        media_play_item_ = id;
        media_playback_ = p;
        media_report_started_ = false;
        media_resume_ = std::max(
            0.0,
            jnum(jobj(item, "UserData"), "PlaybackPositionTicks") / 10000000.0);
        if (jbool(jobj(item, "UserData"), "Played"))
          media_resume_ = 0;
        ::Stream s;
        s.url = p.url;
        s.name = d_item_.name;
        s.addon = c.kind == "plex"   ? "Plex"
                  : c.kind == "emby" ? "Emby"
                                     : "Jellyfin";
        s.request_headers = p.headers;
        play_stream(s, false);
      });
}
void Session::media_tick(bool stop) {
  if (!watching || w_item_.type != "media-video" || !media_reporter_ ||
      !w_started_)
    return;
  auto now = now_seconds();
  bool paused = player_.paused();
  double pos = std::max(0.0, player_.position());
  if (!media_report_started_) {
    media_reporter_->push(media_play_account_, media_playback_,
                          media_play_item_, pos, paused, "start");
    media_report_started_ = true;
    media_report_at_ = now;
    media_report_position_ = pos;
    media_paused_ = paused;
  }
  if (stop) {
    media_reporter_->push(media_play_account_, media_playback_,
                          media_play_item_, pos, paused, "stop");
    media_report_started_ = false;
    for (auto &j : media_items_)
      if (jstr(j, "Id") == media_play_item_) {
        j["UserData"]["PlaybackPositionTicks"] = int64_t(pos * 10000000);
        double dur = player_.duration();
        if (dur > 0 && pos >= dur - 5)
          j["UserData"]["Played"] = true;
      }
    media_build();
  } else if (now - media_report_at_ >= 10 || paused != media_paused_ ||
             std::abs(pos - media_report_position_ -
                      (media_paused_ ? 0 : now - media_report_at_)) > 3) {
    media_reporter_->push(media_play_account_, media_playback_,
                          media_play_item_, pos, paused, "progress");
    media_report_at_ = now;
    media_report_position_ = pos;
    media_paused_ = paused;
  }
}
} // namespace sx
