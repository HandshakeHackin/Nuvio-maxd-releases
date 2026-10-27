#include "live_tv.h"
#include "session.hpp"
#include <algorithm>
#include <cstdio>
#include <fcntl.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace sx {
namespace {
std::string channel_key(const std::string &source, const live_tv::Channel &c) {
  return std::to_string(fnv1a(source + "\n" + c.id + "\n" + c.name));
}
std::string label(const std::string &s) {
  std::string o;
  for (unsigned char c : s)
    if (c >= 32 && c != 127)
      o += char(c);
  return o.substr(0, 100);
}
bool contains(const json &a, const std::string &s) {
  if (a.is_array())
    for (const auto &v : a)
      if (v.is_string() && v.get<std::string>() == s)
        return true;
  return false;
}
} // namespace
void Session::live_reset() {
  if(multiview_active)multi_stop();
  if (live_cancel_)
    *live_cancel_ = true;
  ++live_generation_;
  live_loading_ = false;
  live_channels_.clear();
  live_indices_.clear();
  live_state_ = json::object();
  live_source_.clear();
  live_group_.clear();
  live_query_.clear();
  live_view_ = 0;
  live_page_ = 0;
  live_filter_.clear();
  live_content.items.clear();
  changed_ |= kLive;
}
void Session::live_load() {
  live_reset();
  load_json(profile_dir_ + "/live-tv.json", live_state_);
  if (!live_state_.is_object())
    live_state_ = json::object();
  if (!jobj(live_state_, "sources").is_array())
    live_state_["sources"] = json::array();
  if (!jobj(live_state_, "favorites").is_array())
    live_state_["favorites"] = json::array();
  if (!jobj(live_state_, "recent").is_array())
    live_state_["recent"] = json::array();
  live_source_ = jstr(live_state_, "selected");
  live_build();
}
void Session::live_save() {
  if (!profile_ready_ || profile_dir_.empty())
    return;
  live_state_["selected"] = live_source_;
  const auto path = profile_dir_ + "/live-tv.json";
  const auto tmp = path + ".tmp", body = live_state_.dump(1, '\t');
  int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  bool ok = fd >= 0;
  if (ok) {
    ok = fchmod(fd, 0600) == 0;
    size_t at = 0;
    while (ok && at < body.size()) {
      auto n = write(fd, body.data() + at, body.size() - at);
      if (n <= 0)
        ok = false;
      else
        at += size_t(n);
    }
    ok = (close(fd) == 0) && ok;
  }
  if (ok)
    ok = rename(tmp.c_str(), path.c_str()) == 0;
  if (!ok) {
    remove(tmp.c_str());
    say("Could not save Live TV settings", 7, true);
  }
}
void Session::live_build() {
  live_content.items.clear();
  live_indices_.clear();
  const auto filter = live_source_ + "\n" + live_group_ + "\n" + live_query_ +
                      "\n" + std::to_string(live_view_);
  if (filter != live_filter_) {
    live_page_ = 0;
    live_filter_ = filter;
  }
  sx::Addon manage;
  manage.name = "Live TV sources & filters";
  manage.initial = "TV";
  manage.colour = 0x0ea5a4;
  manage.version = live_loading_
                       ? "Loading channels..."
                       : std::to_string(live_channels_.size()) + " channels";
  manage.types = "M3U Xtream";
  manage.description = "X: sources and filters · Triangle: channel search · "
                       "L1: groups · R1: All / Favorites / Recent";
  if (!live_group_.empty())
    manage.version += " · " + live_group_;
  if (!live_query_.empty())
    manage.version += " · Search: " + live_query_;
  if (live_view_ == 1)
    manage.version += " · Favorites";
  else if (live_view_ == 2)
    manage.version += " · Recent";
  live_content.items.push_back(manage);
  std::vector<size_t> indices;
  const auto query = lower(live_query_);
  for (size_t i = 0; i < live_channels_.size(); ++i) {
    const auto &c = live_channels_[i];
    if (!live_group_.empty() && live_group_ != c.group)
      continue;
    if (!query.empty() &&
        lower(c.name).find(query) == std::string::npos)
      continue;
    const auto key = live_view_ ? channel_key(live_source_, c) : std::string();
    if (live_view_ == 1 && !contains(live_state_["favorites"], key))
      continue;
    if (live_view_ == 2 && !contains(live_state_["recent"], key))
      continue;
    indices.push_back(i);
  }
  if (live_view_ == 2) {
    const auto recent = live_state_["recent"];
    std::stable_sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
      const auto rank = [&](size_t i) {
        auto key = channel_key(live_source_, live_channels_[i]);
        size_t r = 0;
        for (const auto &v : recent) {
          if (v == key)
            break;
          ++r;
        }
        return r;
      };
      return rank(a) < rank(b);
    });
  }
  // Render pages remain bounded; the channel search/group filters cover larger
  // lists.
  live_page_ = std::min(
      live_page_, indices.empty() ? size_t(0) : (indices.size() - 1) / 500);
  for (size_t at = live_page_ * 500;
       at < indices.size() && at < (live_page_ + 1) * 500; ++at) {
    auto i = indices[at];
    const auto &c = live_channels_[i];
    sx::Addon row;
    const auto key = channel_key(live_source_, c);
    row.name = c.name;
    row.version = c.group;
    row.types =
        contains(live_state_["favorites"], key) ? "Favorite Live" : "Live";
    row.initial = std::to_string(live_indices_.size() + 1);
    row.colour = 0x0ea5a4;
    row.description = "X: watch channel · Square: toggle favorite";
    live_content.items.push_back(std::move(row));
    live_indices_.push_back(i);
  }
  if (indices.size() > 500)
    live_content.items[0].description =
        "Page " + std::to_string(live_page_ + 1) + " of " +
        std::to_string((indices.size() + 499) / 500) +
        ". Open sources and filters for the next/previous page, or search.";
  else if (!live_loading_ && live_indices_.empty())
    live_content.items[0].description =
        live_channels_.empty()
            ? "Add an M3U playlist or Xtream Codes account to start watching."
            : "No channels match these filters. Open sources and filters to "
              "change them.";
  changed_ |= kLive;
}
void Session::live_reload() {
  if (!profile_ready_) {
    say("Choose a profile first", 5);
    return;
  }
  json source;
  for (const auto &s : live_state_["sources"])
    if (jstr(s, "id") == live_source_)
      source = s;
  if (!source.is_object()) {
    live_build();
    return;
  }
  if (live_cancel_)
    *live_cancel_ = true;
  live_cancel_ = std::make_shared<std::atomic<bool>>(false);
  const auto cancel = live_cancel_;
  const auto generation = ++live_generation_, profile = profile_generation_;
  live_loading_ = true;
  live_build();
  g_tasks.run<live_tv::Result>(
      [source, cancel] {
        if (jstr(source, "type") == "xtream")
          return live_tv::fetch_xtream(jstr(source, "url"),
                                       jstr(source, "username"),
                                       jstr(source, "password"), cancel.get());
        return live_tv::fetch(jstr(source, "url"), cancel.get());
      },
      [this, generation, profile](live_tv::Result &r) {
        if (generation != live_generation_ || profile != profile_generation_)
          return;
        live_loading_ = false;
        if (!r.error.empty()) {
          say(r.error, 10, true);
          live_build();
          return;
        }
        live_channels_ = std::move(r.channels);
        live_build();
        say("Live TV channels loaded");
      });
}
void Session::live_groups() {
  std::vector<std::string> groups{"All groups"};
  std::set<std::string> seen;
  for (const auto &c : live_channels_)
    if (seen.insert(c.group).second)
      groups.push_back(c.group);
  const auto generation = live_generation_;
  open_dropdown("Live TV groups", groups, 0, [this, groups, generation](int i) {
    if (generation != live_generation_ || i < 0 || size_t(i) >= groups.size())
      return;
    live_group_ = i ? groups[i] : "";
    live_build();
  });
}
void Session::live_search() {
  const auto generation = live_generation_;
  ask_text(
      [this, generation](const std::string &text) {
        if (generation != live_generation_)
          return;
        live_query_ = label(trim(text));
        live_build();
      },
      "Search live channels (empty clears search)");
}
void Session::live_cycle() {
  live_view_ = (live_view_ + 1) % 3;
  live_build();
}
void Session::live_activate(int index, bool favorite) {
  if (index == 0) {
    live_menu();
    return;
  }
  if (index < 1 || size_t(index - 1) >= live_indices_.size() || watching ||
      resuming_)
    return;
  const auto &channel = live_channels_[live_indices_[index - 1]];
  const auto key = channel_key(live_source_, channel);
  if (favorite) {
    auto &f = live_state_["favorites"];
    auto it = std::find(f.begin(), f.end(), json(key));
    if (it != f.end())
      f.erase(it);
    else if (f.size() < 1000)
      f.push_back(key);
    else {
      say("Favorites limit reached", 5);
      return;
    }
    live_save();
    live_build();
    return;
  }
  live_play(channel);
}
void Session::live_play(const live_tv::Channel& channel) {
  const auto key=channel_key(live_source_,channel);
  // Live channels never enter title Continue Watching, Trakt or episode flows.
  d_item_ = Item{};
  d_item_.id = "live:" + key;
  d_item_.name = channel.name;
  d_item_.type = "tv";
  d_meta_ = Meta{};
  d_meta_loaded_ = false;
  d_video_id_ = d_item_.id;
  detail = DetailContent{};
  ::Stream stream;
  stream.url = channel.url;
  stream.name = channel.name;
  stream.addon = "Live TV";
  stream.request_headers = channel.headers;
  play_stream(stream, false);
  if (watching) {
    auto &recent = live_state_["recent"];
    recent.erase(std::remove(recent.begin(), recent.end(), json(key)),
                 recent.end());
    recent.insert(recent.begin(), key);
    while (recent.size() > 30)
      recent.erase(recent.end() - 1);
    live_save();
    live_build();
  }
}
void Session::live_add(json source, int step) {
  if (!profile_ready_)
    return;
  const auto profile = profile_generation_;
  const bool xtream = jstr(source, "type") == "xtream";
  const std::vector<std::string> prompts =
      xtream
          ? std::vector<std::string>{"Live TV source name",
                                     "Xtream server URL (http:// or https://)",
                                     "Xtream username", "Xtream password"}
          : std::vector<std::string>{"Live TV source name",
                                     "M3U playlist URL (http:// or https://)"};
  if (step == int(prompts.size())) {
    if (live_state_["sources"].size() >= 16) {
      say("You can save up to 16 Live TV sources", 5);
      return;
    }
    source["id"] =
        std::to_string(fnv1a(std::to_string(now_epoch_ms()) +
                             jstr(source, "url") + jstr(source, "name")));
    live_source_ = jstr(source, "id");
    live_state_["sources"].push_back(source);
    live_channels_.clear();
    live_group_.clear();
    live_query_.clear();
    live_view_ = 0;
    live_save();
    live_reload();
    return;
  }
  const char *key = step == 0   ? "name"
                    : step == 1 ? "url"
                    : step == 2 ? "username"
                                : "password";
  ask_text(
      [this, source, step, key, profile](const std::string &value) mutable {
        if (profile != profile_generation_ || !profile_ready_)
          return;
        auto v = step == 3 ? value : trim(value);
        if (v.empty() || v.size() > (step == 1 ? 8192u : 256u)) {
          say("The field is empty or too long. Add the source again.", 6);
          return;
        }
        if (step == 1 && !live_tv::http_url(v)) {
          say("Enter an HTTP or HTTPS URL. Add the source again.", 6);
          return;
        }
        source[key] = step == 0 ? label(v) : v;
        live_add(source, step + 1);
      },
      prompts[step]);
}
void Session::live_menu() {
  if (!profile_ready_) {
    say("Choose a profile before configuring Live TV", 6);
    return;
  }
  const auto generation = profile_generation_;
  std::vector<std::string> options{
      "Add M3U playlist",  "Add Xtream Codes account",
      "Choose source",     "Reload channels",
      "Channel groups",    "Search channels",
      "All channels",      "Favorites",
      "Recent channels",   "Remove current source",
      "Next channel page", "Previous channel page", "Four-screen TV (experimental)"};
  open_dropdown("Live TV", options, 0, [this, generation](int i) {
    if (generation != profile_generation_)
      return;
    if (i == 12) {multi_choose();return;}
    if (i == 0 || i == 1) {
      live_add(json{{"type", i ? "xtream" : "m3u"}}, 0);
      return;
    }
    if (i == 2) {
      std::vector<std::string> names, ids;
      for (const auto &s : live_state_["sources"]) {
        names.push_back(jstr(s, "name"));
        ids.push_back(jstr(s, "id"));
      }
      if (names.empty()) {
        say("Add a Live TV source first", 5);
        return;
      }
      open_dropdown("Choose Live TV source", names, 0,
                    [this, ids, generation](int n) {
                      if (generation != profile_generation_ || n < 0 ||
                          size_t(n) >= ids.size())
                        return;
                      live_source_ = ids[n];
                      live_channels_.clear();
                      live_group_.clear();
                      live_query_.clear();
                      live_view_ = 0;
                      live_save();
                      live_reload();
                    });
      return;
    }
    if (i == 3) {
      live_reload();
      return;
    }
    if (i == 4) {
      live_groups();
      return;
    }
    if (i == 5) {
      live_search();
      return;
    }
    if (i >= 6 && i <= 8) {
      live_view_ = i - 6;
      live_group_.clear();
      live_query_.clear();
      live_build();
      return;
    }
    if (i == 10 || i == 11) {
      if (i == 10)
        ++live_page_;
      else if (live_page_)
        --live_page_;
      live_build();
      return;
    }
    if (i == 9) {
      const auto source = live_source_;
      open_dropdown(
          "Remove this Live TV source?", {"Cancel", "Remove source"}, 0,
          [this, source, generation](int n) {
            if (n != 1 || generation != profile_generation_)
              return;
            if (live_cancel_)
              *live_cancel_ = true;
            ++live_generation_;
            live_loading_ = false;
            // Remove this source's associated favorites and recent entries too.
            for (const auto &c : live_channels_) {
              auto key = channel_key(source, c);
              for (auto field : {"favorites", "recent"}) {
                auto &list = live_state_[field];
                list.erase(std::remove(list.begin(), list.end(), json(key)),
                           list.end());
              }
            }
            auto &sources = live_state_["sources"];
            sources.erase(std::remove_if(sources.begin(), sources.end(),
                                         [&](const json &s) {
                                           return jstr(s, "id") == source;
                                         }),
                          sources.end());
            live_source_ = sources.empty() ? "" : jstr(sources[0], "id");
            live_channels_.clear();
            live_group_.clear();
            live_query_.clear();
            live_view_ = 0;
            live_save();
            live_build();
            live_reload();
          });
    }
  });
  dropdown.message =
      "Use your own M3U playlist or Xtream Codes subscription. Sources, "
      "favorites and recent channels are saved locally for this profile.";
}
} // namespace sx
