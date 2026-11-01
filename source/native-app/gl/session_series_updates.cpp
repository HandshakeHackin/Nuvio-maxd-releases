#include "session.hpp"
namespace sx {
void Session::series_updates_reset() {
  if (series_updates_cancel_)
    *series_updates_cancel_ = true;
  series_updates_cancel_ = std::make_shared<std::atomic<bool>>(false);
  ++series_updates_generation_;
  series_updates_.clear();
  series_updates_pending_.clear();
  series_updates_inflight_ = 0;
  series_history_ready_ = false;
  series_updates_tick_ = 0;
}
void Session::want_series_updates(const Item &item) {
  if (item.type != "series" || !series_history_ready_ ||
      series_updates_inflight_ >= 2 || series_updates_pending_.count(item.id))
    return;
  auto member = library_.find(item.id);
  if (member == library_.end() || jbool(member->second, "removed") ||
      jbool(member->second, "temp") ||
      !series_updates_.due(item.id, now_seconds()))
    return;
  std::vector<std::string> urls;
  for (const auto &addon : addons_)
    if (addon->supports("meta", "series", item.id)) {
      urls.push_back(addon->resource_url("meta", "series", item.id));
      if (urls.size() == 4)
        break;
    }
  if (urls.empty())
    return;
  if (!series_updates_cancel_)
    series_updates_cancel_ = std::make_shared<std::atomic<bool>>(false);
  const auto cancel = series_updates_cancel_;
  const auto generation = series_updates_generation_;
  const auto id = item.id;
  series_updates_pending_.insert(id);
  ++series_updates_inflight_;
  struct Result {
    bool ok = false;
    std::vector<Video> videos;
  };
  bg<Result>(
      [urls, id, cancel] {
        Result r;
        for (const auto &url : urls) {
          if (*cancel)
            break;
          json body;
          std::string error;
          if (!fetch_json(url, body, error, 10, cancel.get()) ||
              !jobj(body, "meta").is_object())
            continue;
          auto meta = parse_meta(body["meta"]);
          if (!meta.id.empty() && meta.id != id)
            continue;
          if (meta.videos.size() > 2000)
            continue;
          if (!meta.videos.empty()) {
            r.videos = std::move(meta.videos);
            r.ok = true;
            break;
          }
        }
        return r;
      },
      [this, id, generation](Result &r) {
        if (generation != series_updates_generation_)
          return;
        --series_updates_inflight_;
        series_updates_pending_.erase(id);
        series_updates_.put(id, r.videos, now_seconds(), r.ok);
        refresh_board();
        refresh_library();
      });
}
} // namespace sx
