// Profile-local TVDB credentials and per-series order selection.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "config_backup.h"
#include "i18n.hpp"
#include "session.hpp"
#include "tvdb.h"
#include <algorithm>
namespace sx {
namespace {
tvdb::Credentials credentials(const std::string &dir) {
  auto j = iptv::load_private(dir + "/tvdb.json");
  return {jstr(j, "api_key"), jstr(j, "pin")};
}
Meta provider_meta(Meta m) {
  for (auto &v : m.videos) {
    v.order_season = v.order_episode = -1;
    v.order_id.clear();
  }
  std::stable_sort(
      m.videos.begin(), m.videos.end(), [](const auto &a, const auto &b) {
        return std::pair(a.season, a.episode) < std::pair(b.season, b.episode);
      });
  return m;
}
} // namespace
void Session::tvdb_menu() {
  if (!profile_ready_) {
    say(i18n::tr("Choose a profile first."));
    return;
  }
  const auto profile = profile_generation_;
  open_dropdown(
      i18n::tr("TheTVDB episode ordering"),
      {i18n::tr("Set TVDB API key"),
       i18n::tr("Set TVDB subscriber PIN (optional)"),
       i18n::tr("Test TVDB connection"), i18n::tr("Remove TVDB credentials")},
      0, [this, profile](int n) {
        if (profile != profile_generation_ || !profile_ready_ || tvdb_busy_)
          return;
        if (n == 0 || n == 1) {
          ask_text(
              [this, profile, n](const auto &value) {
                if (profile != profile_generation_ || !profile_ready_ ||
                    value.size() > 4096)
                  return;
                auto j = iptv::load_private(profile_dir_ + "/tvdb.json");
                if (!j.is_object())
                  j = json::object();
                j[n == 0 ? "api_key" : "pin"] = trim(value);
                const bool ok =
                    config_backup::save(profile_dir_ + "/tvdb.json", j.dump());
                ++tvdb_generation_;
                say(i18n::tr(ok ? "TVDB settings saved"
                                : "Could not save TVDB settings"),
                    6, !ok);
              },
              i18n::tr(n == 0 ? "TVDB API key"
                              : "TVDB subscriber PIN (optional)"),
              true);
          return;
        }
        if (n == 3) {
          auto j = iptv::load_private(profile_dir_ + "/tvdb.json");
          if (!j.is_object())
            j = json::object();
          j.erase("api_key");
          j.erase("pin");
          const bool ok =
              config_backup::save(profile_dir_ + "/tvdb.json", j.dump());
          ++tvdb_generation_;
          say(i18n::tr(ok ? "TVDB credentials removed"
                          : "Could not save TVDB settings"),
              6, !ok);
          return;
        }
        if (n == 2) {
          tvdb_busy_ = true;
          const auto c = credentials(profile_dir_);
          const auto cancel = features_cancel_;
          const auto gen = ++tvdb_generation_;
          bg<std::string>(
              [c, cancel] {
                std::string error;
                tvdb::test(c, error, cancel.get());
                return error;
              },
              [this, profile, gen](auto &error) {
                if (profile != profile_generation_ || gen != tvdb_generation_)
                  return;
                tvdb_busy_ = false;
                say(error.empty() ? i18n::tr("TVDB connection successful")
                                  : error,
                    8, !error.empty());
              });
        }
      });
  dropdown.message =
      i18n::tr("Metadata provided by TheTVDB.com. Use your own licensed API "
               "access. Credentials stay in this profile; tokens are never "
               "saved. Choose an order from a series title's Options menu.");
}
void Session::detail_tvdb_menu() {
  if (!profile_ready_ || !d_open_ || watching || resuming_ ||
      d_item_.type != "series" || tvdb_busy_)
    return;
  const auto profile = profile_generation_;
  const auto title = d_meta_gen_;
  const auto gen = ++tvdb_generation_;
  const auto cancel = d_cancel_;
  const auto c = credentials(profile_dir_);
  const auto meta = provider_meta(d_meta_);
  tvdb_busy_ = true;
  bg<tvdb::Result>(
      [meta, c, cancel] { return tvdb::load(meta, c, {}, cancel.get()); },
      [this, profile, title, gen](auto &r) {
        if (profile != profile_generation_ || gen != tvdb_generation_)
          return;
        tvdb_busy_ = false;
        if (title != d_meta_gen_ || !d_open_ || watching || resuming_)
          return;
        std::vector<std::string> names{i18n::tr("Provider order (default)")},
            ids{""};
        for (const auto &order : r.orders) {
          names.push_back(order.name);
          ids.push_back(order.id);
        }
        const auto preferences =
            iptv::load_private(profile_dir_ + "/tvdb.json");
        const auto selected =
            jstr(jobj(preferences, "orders"), d_item_.id.c_str());
        int active = 0;
        for (size_t i = 1; i < ids.size(); ++i)
          if (ids[i] == selected)
            active = int(i);
        open_dropdown(i18n::tr("Episode order (TheTVDB)"), names, active,
                      [this, profile, title, ids](int n) {
                        if (profile != profile_generation_ ||
                            title != d_meta_gen_ || !d_open_ || watching ||
                            resuming_ || n < 0 || n >= int(ids.size()))
                          return;
                        detail_tvdb_load(ids[n]);
                      });
        dropdown.message =
            r.error.empty()
                ? i18n::tr(
                      "Selected numbering is used for source searches. Watched "
                      "history and resume keep their original identity. Only "
                      "verified one-to-one orders are supported.")
                : r.error;
      });
}
bool Session::detail_tvdb_restore() {
  if (d_item_.type != "series" || d_meta_.videos.empty())
    return false;
  const auto j = iptv::load_private(profile_dir_ + "/tvdb.json");
  const auto order = jstr(jobj(j, "orders"), d_item_.id.c_str());
  if (order.empty())
    return false;
  detail_tvdb_load(order, true);
  return true;
}
void Session::detail_tvdb_load(const std::string &order, bool restoring) {
  if (!profile_ready_ || !d_open_ || watching || (!restoring && resuming_) ||
      tvdb_busy_)
    return;
  const auto profile = profile_generation_;
  const auto title = d_meta_gen_;
  const auto gen = ++tvdb_generation_;
  const auto meta = provider_meta(d_meta_);
  const auto c = credentials(profile_dir_);
  const auto cancel = d_cancel_;
  auto finish = [this, profile, title, gen, order, restoring](tvdb::Result &r) {
    if (profile != profile_generation_ || gen != tvdb_generation_)
      return;
    tvdb_busy_ = false;
    if (title != d_meta_gen_ || !d_open_ || watching)
      return;
    if (r.error.empty()) {
      *d_stream_cancel_ = true;
      ++d_gen_;
      d_found_.clear();
      d_streams_.clear();
      d_video_id_.clear();
      detail.streams.clear();
      detail_preparation_cancel();
      next_preparation_cancel();
      stream_result_cache_.clear();
      d_season_videos_.clear();
      d_meta_ = std::move(r.meta);
      if (!restoring) {
        auto j = iptv::load_private(profile_dir_ + "/tvdb.json");
        if (!j.is_object())
          j = json::object();
        if (!jobj(j, "orders").is_object())
          j["orders"] = json::object();
        if (order.empty())
          j["orders"].erase(d_item_.id);
        else
          j["orders"][d_item_.id] = order;
        if (j["orders"].size() > 2000)
          j["orders"].erase(j["orders"].begin());
        if (!config_backup::save(profile_dir_ + "/tvdb.json", j.dump()))
          say(i18n::tr("Order applied for this session; preference could not "
                       "be saved."),
              8, true);
      }
    } else
      say(r.error, 10, true);
    detail_apply_meta();
    if (detail.people.empty() && !detail.people_loading)
      cast_load();
    refresh_board();
    refresh_library();
    if (resume_play_)
      start_remembered_stream();
  };
  tvdb_busy_ = true;
  detail.episodes_status = i18n::tr("Loading episode order...");
  changed_ |= kDetail;
  if (order.empty()) {
    tvdb::Result r;
    r.meta = meta;
    finish(r);
    return;
  }
  bg<tvdb::Result>(
      [meta, c, order, cancel] {
        return tvdb::load(meta, c, order, cancel.get());
      },
      finish);
}
} // namespace sx
