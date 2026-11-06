// ReSeq-inspired sports flow adapted to the native controller UI.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include <algorithm>
#include <sys/stat.h>
namespace sx {
namespace {
const char *views[] = {"All events", "My Sports", "On now / starting",
                       "Upcoming", "Results"};
std::string offset_label(int n) {
  char b[32];
  std::snprintf(b, sizeof(b), "UTC %c%02d:%02d", n < 0 ? '-' : '+',
                std::abs(n) / 60, std::abs(n) % 60);
  return b;
}
} // namespace
void Session::sports_reset() {
  if (sports_cancel_)
    *sports_cancel_ = true;
  sports_cancel_ = std::make_shared<std::atomic<bool>>(false);
  ++sports_generation_;
  ++sports_dialog_;
  sports_loading_ = sports_score_loading_ = sports_team_loading_ = false;
  sports_snapshot_ = {};
  sports_rows_.clear();
  sports_content.items.clear();
  sports_visible = false;
  sports_prefs_ = {};
  sports_last_feed_ = -1000;
  sports_view_ = 0;
  sports_page_ = 0;
  sports_total_ = 0;
  sports_guide_indices_.clear();
  sports_guide_cursor_ = 0;
  sports_query_.clear();
  sports_notice_.clear();
  sports_teams_.clear();
  sports_team_expiry_.clear();
  sports_score_expiry_.clear();
  sports_next_feed_ = sports_next_tick_ = sports_next_score_ = 0;
  changed_ |= kSports;
}
void Session::sports_load(const json &j) {
  sports_reset();
  sports_prefs_ = sports::load(j);
  sports_visible = sports_prefs_.enabled;
  if (sports_catalogue_.leagues.empty()) {
    std::string raw;
    if (read_file(base_dir_ + "/sports/catalogue.json", raw))
      sports_catalogue_ = sports::catalogue(raw);
  }
  sports_build();
}
void Session::sports_save() {
  save_profile_settings();
  refresh_settings();
  sports_build();
}
void Session::sports_toggle() {
  if (!profile_ready_ || watching || resuming_)
    return;
  sports_prefs_.enabled = !sports_prefs_.enabled;
  sports_visible = sports_prefs_.enabled;
  if (sports_cancel_)
    *sports_cancel_ = true;
  sports_cancel_ = std::make_shared<std::atomic<bool>>(false);
  ++sports_generation_;
  ++sports_dialog_;
  sports_loading_ = sports_score_loading_ = sports_team_loading_ = false;
  sports_next_tick_ = 0;
  if (!sports_visible) {
    sports_rows_.clear();
    sports_content.items.clear();
    sports_next_feed_ = 0;
    if (page_ == Page::sports)
      goto_ = Page::settings;
  }
  sports_save();
  say(sports_visible
          ? "Sports Hub enabled. Choose your sports and teams in this section."
          : "Sports Hub disabled. Your selections are saved.",
      7);
}
void Session::sports_scores() {
  if (!sports_visible)
    return;
  sports_prefs_.hide_scores = !sports_prefs_.hide_scores;
  sports_save();
}
void Session::sports_channel_filter() {
  if (!sports_visible)
    return;
  sports_prefs_.only_channels = !sports_prefs_.only_channels;
  sports_save();
}
std::vector<sports::Guide> Session::sports_guide() const {
  std::vector<sports::Guide> out;
  const int64_t offset = int64_t(live_guide_offset()) * 60;
  for (size_t i = 0; i < live_channels_.size() && out.size() < 10000; ++i) {
    const auto &c = live_channels_[i];
    const live_guide::Listings *list = nullptr;
    const auto xml =
        live_guide_xml_.channels.find(c.epg_id.empty() ? c.id : c.epg_id);
    if (xml != live_guide_xml_.channels.end())
      list = &xml->second;
    const auto epg = live_guide_channels_.find(c.id);
    if (epg != live_guide_channels_.end())
      list = &epg->second.programmes;
    if (list)
      for (auto p : *list) {
        p.start += offset;
        p.end += offset;
        const auto normalized = sports::norm(p.title);
        out.push_back({i, std::move(p), normalized});
        if (out.size() >= 10000)
          break;
      }
  }
  return out;
}
void Session::sports_build() {
  sports_content.items.clear();
  sports_rows_.clear();
  if (!sports_visible) {
    changed_ |= kSports;
    return;
  }
  const auto now = now_epoch_ms() / 1000;
  auto combined = sports_snapshot_;
  if (now - combined.generated > sports::max_age)
    combined.events.clear();
  const auto guide = sports_guide();
  std::vector<std::string> names = sports_prefs_.words;
  for (const auto &team : sports_prefs_.teams)
    names.push_back(team.name);
  auto events = sports::guide_events(sports_catalogue_, guide, now, names);
  for (auto &e : events)
    combined.events.push_back(std::move(e));
  auto rows = sports::visible(combined, sports_catalogue_, sports_prefs_,
                              sports_view_, now, sports_query_);
  sports::ChannelIndex channel_index;
  if (sports_prefs_.only_channels)
    channel_index = sports::index_channels(live_channels_);
  if (sports_prefs_.only_channels)
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [&](const auto &e) {
                                return sports::matches(e, live_channels_, guide,
                                                       sports_prefs_.region,
                                                       &channel_index)
                                    .empty();
                              }),
               rows.end());
  sports_total_ = rows.size();
  sports_page_ =
      std::min(sports_page_, rows.empty() ? size_t(0) : (rows.size() - 1) / 60);
  Addon settings;
  settings.identity = "sports:settings";
  settings.name = "Sports Hub";
  settings.version =
      std::string(views[sports_view_]) + " · " + sports_prefs_.region;
  settings.description =
      "X: preferences · Triangle: search · Options: selections";
  settings.initial = "S";
  sports_content.items.push_back(settings);
  Addon filter;
  filter.identity = "sports:view";
  filter.name = "Choose view";
  filter.version = offset_label(sports_prefs_.utc_offset);
  filter.description = "All events, My Sports, On now, Upcoming or Results. "
                       "Times use your selected UTC offset.";
  filter.initial = "V";
  sports_content.items.push_back(filter);
  Addon paging;
  paging.identity = "sports:paging";
  paging.name = "Events and refresh";
  paging.version = std::to_string(rows.size()) + " events · Page " +
                   std::to_string(sports_page_ + 1);
  paging.initial = "R";
  paging.description =
      sports_loading_           ? "Loading public sports schedule..."
      : !sports_notice_.empty() ? sports_notice_
      : sports_snapshot_.generated
          ? "Schedule: " +
                sports::clock(sports_snapshot_.generated,
                              sports_prefs_.utc_offset) +
                " · ReSeq public feed / ESPN. Scores may be delayed."
          : "No schedule loaded. Refresh here; guide events work with your "
            "configured Live TV source.";
  sports_content.items.push_back(paging);
  const auto first = sports_page_ * 60,
             last = std::min(rows.size(), first + 60);
  for (size_t i = first; i < last; ++i) {
    auto e = rows[i];
    const auto *l = sports::league(sports_catalogue_, e.league);
    Addon card;
    card.identity = e.id;
    card.name = e.title;
    card.version = sports::event_time(e, sports_prefs_.utc_offset);
    card.description = (l ? l->name : e.league) + " · " +
                       sports::event_status(e, sports_prefs_.hide_scores, now);
    card.types = sports::follows(e, sports_prefs_) ? "Following" : "";
    card.initial = l ? l->name.substr(0, 1) : "S";
    card.colour = e.state == "in" && e.fresh ? 0x138b69 : 0x334b75;
    sports_content.items.push_back(std::move(card));
    sports_rows_.push_back(std::move(e));
  }
  if (rows.empty() && !sports_loading_ && sports_notice_.empty())
    sports_content.items[2].description =
        sports_query_.empty()
            ? "No events match this view. Choose sports, follow teams, change "
              "view or refresh. Free schedule coverage varies."
            : "No events match your search. Clear or change it with Triangle.";
  changed_ |= kSports;
}
void Session::sports_reload(bool force) {
  if (!sports_visible || sports_loading_ || watching || resuming_ ||
      now_seconds() < sports_next_feed_)
    return;
  if (sports_catalogue_.leagues.empty()) {
    sports_notice_ = "Sports catalogue is missing from this build.";
    sports_build();
    return;
  }
  if (now_seconds() - sports_last_feed_ < 60) {
    sports_next_feed_ = sports_last_feed_ + 60;
    return;
  }
  sports_last_feed_ = now_seconds();
  sports_loading_ = true;
  sports_next_feed_ = now_seconds() + 900;
  sports_build();
  const auto cancel = sports_cancel_;
  const auto gen = sports_generation_;
  const auto catalog = sports_catalogue_;
  const auto path = data_dir_ + "/sports-shared-feed.json";
  struct Result {
    sports::Snapshot snapshot;
    std::string notice;
  };
  bg<Result>(
      [cancel, catalog, path, force] {
        Result r;
        std::string raw;
        struct stat st{};
        const auto now = now_epoch_ms() / 1000;
        if (lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
            st.st_size > 0 && uint64_t(st.st_size) <= sports::max_feed_bytes &&
            read_file(path, raw))
          r.snapshot = sports::parse_feed(raw, catalog, now);
        if (!force && r.snapshot.error.empty() && r.snapshot.generated &&
            now - st.st_mtime >= 0 && now - st.st_mtime < 21600)
          return r;
        const auto previous =
            r.snapshot.error.empty() ? r.snapshot.generated : 0;
        const auto response = live_tv::download(sports::feed_url, cancel.get(),
                                                true, sports::max_feed_bytes);
        if (cancel && *cancel)
          return Result{};
        if (response.ok()) {
          auto fresh =
              sports::parse_feed(response.body, catalog, now, previous);
          if (fresh.error.empty()) {
            r.snapshot = std::move(fresh);
            if (!write_file(path, response.body))
              r.notice =
                  "Schedule loaded, but could not save the public cache.";
            return r;
          }
        }
        r.notice = "Schedule refresh failed. Showing valid cached/guide "
                   "events; try again later.";
        if (!r.snapshot.error.empty())
          r.snapshot = {};
        return r;
      },
      [this, gen, cancel](auto &r) {
        if (gen != sports_generation_ || !sports_visible || *cancel)
          return;
        sports_loading_ = false;
        sports_snapshot_ = std::move(r.snapshot);
        sports_notice_ = r.notice;
        sports_next_feed_ = now_seconds() + (r.notice.empty() ? 21600 : 900);
        sports_build();
      });
}
void Session::sports_tick(int focus) {
  if (!sports_visible || page_ != Page::sports || watching || resuming_ ||
      multiview_active)
    return;
  const double time = now_seconds();
  if (time < sports_next_tick_)
    return;
  sports_next_tick_ = time + 15;
  sports_reload();
  sports_build();
  sports_guide_indices_.clear();
  if (focus >= 3 && size_t(focus - 3) < sports_rows_.size())
    for (const auto &m :
         sports::matches(sports_rows_[size_t(focus - 3)], live_channels_, {},
                         sports_prefs_.region))
      sports_guide_indices_.push_back(m.channel);
  for (size_t i = 0;
       i < live_channels_.size() && sports_guide_indices_.size() < 128; ++i) {
    const auto text =
        lower(live_channels_[i].name + " " + live_channels_[i].group);
    if (text.find("sport") != std::string::npos ||
        text.find("espn") != std::string::npos ||
        text.find("bein") != std::string::npos ||
        text.find("tnt") != std::string::npos)
      if (std::find(sports_guide_indices_.begin(), sports_guide_indices_.end(),
                    i) == sports_guide_indices_.end())
        sports_guide_indices_.push_back(i);
  }
  if (!sports_guide_indices_.empty())
    sports_guide_cursor_ =
        (sports_guide_cursor_ + 6) % int(sports_guide_indices_.size());
  if (sports_score_loading_ || time < sports_next_score_ || focus < 3 ||
      size_t(focus - 3) >= sports_rows_.size())
    return;
  const auto event = sports_rows_[size_t(focus - 3)];
  const auto *league = sports::league(sports_catalogue_, event.league);
  const auto now = now_epoch_ms() / 1000;
  if (!league || league->path.empty() || event.state == "post" ||
      event.start > now + 600 || event.start < now - 8 * 3600 ||
      time < sports_score_expiry_[league->id])
    return;
  sports_score_loading_ = true;
  sports_next_score_ = time + 30;
  sports_score_expiry_[league->id] = time + 60;
  const auto cancel = sports_cancel_;
  const auto gen = sports_generation_;
  const auto l = *league;
  const auto region = sports_prefs_.region;
  bg<std::vector<sports::Event>>(
      [l, cancel, event, region] {
        const auto primary = sports::espn_url(l, false, event.start);
        auto response =
            live_tv::download(primary, cancel.get(), true, 8u << 20);
        auto rows =
            response.ok()
                ? sports::scoreboard(json::parse(response.body, nullptr, false),
                                     l, now_epoch_ms() / 1000, true, region)
                : std::vector<sports::Event>{};
        // ESPN calendars use US dates. A second adjacent-date request handles
        // UTC midnight without a timezone database.
        const auto previous =
            sports::espn_url(l, false, event.start - 6 * 3600);
        if (previous != primary && !(cancel && *cancel) &&
            std::none_of(rows.begin(), rows.end(),
                         [&](const auto &e) { return e.id == event.id; })) {
          response = live_tv::download(previous, cancel.get(), true, 8u << 20);
          if (response.ok())
            for (auto &e :
                 sports::scoreboard(json::parse(response.body, nullptr, false),
                                    l, now_epoch_ms() / 1000, true, region))
              if (std::none_of(rows.begin(), rows.end(),
                               [&](const auto &old) { return old.id == e.id; }))
                rows.push_back(std::move(e));
        }
        return rows;
      },
      [this, gen, cancel](auto &events) {
        if (gen != sports_generation_ || !sports_visible || *cancel)
          return;
        sports_score_loading_ = false;
        if (!events.empty() && !sports_snapshot_.generated)
          sports_snapshot_.generated = now_epoch_ms() / 1000;
        for (auto &e : events) {
          auto old = std::find_if(sports_snapshot_.events.begin(),
                                  sports_snapshot_.events.end(),
                                  [&](const auto &x) { return x.id == e.id; });
          if (old != sports_snapshot_.events.end())
            *old = std::move(e);
          else if (sports_snapshot_.events.size() < 20000)
            sports_snapshot_.events.push_back(std::move(e));
        }
        sports_build();
      });
}
void Session::sports_activate(int index) {
  if (!sports_visible || watching || resuming_ || index < 0)
    return;
  if (index == 0) {
    sports_menu();
    return;
  }
  if (index == 1) {
    const auto p = profile_generation_, g = sports_generation_;
    open_dropdown("Sports view",
                  {views[0], views[1], views[2], views[3], views[4]},
                  sports_view_, [this, p, g](int i) {
                    if (p != profile_generation_ || g != sports_generation_ ||
                        !sports_visible || i < 0 || i > 4)
                      return;
                    sports_view_ = i;
                    sports_page_ = 0;
                    sports_build();
                  });
    return;
  }
  if (index == 2) {
    const auto p = profile_generation_, g = sports_generation_;
    open_dropdown("Sports events",
                  {"Next page", "Previous page", "Refresh schedule",
                   "Reload Live TV / guide"},
                  0, [this, p, g](int i) {
                    if (p != profile_generation_ || g != sports_generation_ ||
                        !sports_visible)
                      return;
                    if (i == 0 && sports_total_ > (sports_page_ + 1) * 60)
                      ++sports_page_;
                    else if (i == 1 && sports_page_)
                      --sports_page_;
                    else if (i == 2) {
                      sports_next_feed_ = 0;
                      sports_reload(true);
                    } else if (i == 3)
                      live_reload();
                    sports_build();
                  });
    return;
  }
  if (size_t(index - 3) < sports_rows_.size())
    sports_event(sports_rows_[size_t(index - 3)]);
}
void Session::sports_search() {
  if (!sports_visible)
    return;
  const auto p = profile_generation_, g = sports_generation_;
  ask_text(
      [this, p, g](const std::string &q) {
        if (p != profile_generation_ || g != sports_generation_ ||
            !sports_visible)
          return;
        sports_query_ = sports::clean(q, 80);
        sports_page_ = 0;
        sports_build();
      },
      "Search Sports Hub (empty clears)");
}
} // namespace sx
