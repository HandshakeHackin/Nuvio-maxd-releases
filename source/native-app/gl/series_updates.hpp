// New means a dated, aired episode after a known completed regular episode.
#pragma once
#include "airtime.hpp"
#include "stremio.h"
#include <algorithm>
#include <map>
#include <set>
namespace sx {
class SeriesUpdates {
  struct Episode {
    std::string id, released;
    int season = 0, number = 0;
  };
  struct Entry {
    std::vector<Episode> episodes;
    double expires = 0, touched = 0;
  };
  std::map<std::string, Entry> entries_;

public:
  void clear() { entries_.clear(); }
  bool due(const std::string &id, double now) {
    auto it = entries_.find(id);
    if (it == entries_.end())
      return true;
    it->second.touched = now;
    return it->second.expires <= now;
  }
  void put(const std::string &id, const std::vector<Video> &videos, double now,
           bool success = true) {
    if (!entries_.count(id) && entries_.size() >= 128) {
      auto old = std::min_element(entries_.begin(), entries_.end(),
                                  [](const auto &a, const auto &b) {
                                    return a.second.touched < b.second.touched;
                                  });
      entries_.erase(old);
    }
    auto &e = entries_[id];
    e.touched = now;
    e.expires = now + (success ? 3600 : 300);
    if (!success)
      return;
    e.episodes.clear();
    if (videos.size() > 2000)
      return;
    for (const auto &v : videos)
      if (v.season > 0 && v.episode > 0 && !v.id.empty())
        e.episodes.push_back({v.id, v.released, v.season, v.episode});
  }
  std::string badge(const std::string &id, const std::set<std::string> &watched,
                    int64_t now, bool history_ready,
                    const std::string &in_progress = {}) const {
    if (!history_ready)
      return {};
    auto it = entries_.find(id);
    if (it == entries_.end())
      return {};
    std::pair<int, int> completed{0, 0};
    auto canonical = [&](const Episode &v) {
      return id + ":" + std::to_string(v.season) + ":" +
             std::to_string(v.number);
    };
    for (const auto &v : it->second.episodes)
      if (watched.count(v.id) || watched.count(canonical(v)))
        completed = std::max(completed, {v.season, v.number});
    if (completed.first <= 0)
      return {};
    const Episode *next = nullptr;
    for (const auto &v : it->second.episodes) {
      if (std::pair{v.season, v.number} <= completed || watched.count(v.id) ||
          watched.count(canonical(v)) || v.id == in_progress ||
          canonical(v) == in_progress)
        continue;
      auto released = airtime::iso(v.released);
      if (!released || *released > now)
        continue;
      if (!next ||
          std::pair{v.season, v.number} < std::pair{next->season, next->number})
        next = &v;
    }
    return next ? "New: S" + std::to_string(next->season) + " E" +
                      std::to_string(next->number)
                : "";
  }
};
} // namespace sx
