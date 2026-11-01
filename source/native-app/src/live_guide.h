// Bounded provider EPG snapshots; guide failures never stop channel playback.
#pragma once
#include "live_tv.h"
#include <cstdint>
#include <map>
#include <memory>
#include <set>
namespace live_guide {
constexpr size_t max_bytes = 32u << 20;
struct Programme {
  int64_t start = 0, end = 0;
  std::string title;
};
using Listings = std::vector<Programme>;
struct Result {
  std::map<std::string, Listings> channels;
  std::string error;
  bool limited = false;
  std::shared_ptr<const std::string>
      document{}; // Bounded XMLTV body, reused across channel pages.
};
struct NowNext {
  const Programme *now = nullptr;
  const Programme *next = nullptr;
};
NowNext current(const Listings &, int64_t now);
std::string label(const Programme &);
Result parse_xmltv(const std::string &, const std::set<std::string> &wanted,
                   int64_t now, const std::atomic<bool> *cancel = nullptr);
Result parse_xtream(const std::string &, const std::string &channel,
                    int64_t now);
Result fetch_xmltv(const std::string &, const std::set<std::string> &wanted,
                   int64_t now, const std::atomic<bool> *);
Result fetch_xtream(const std::string &server, const std::string &user,
                    const std::string &password, const std::string &channel,
                    int64_t now, const std::atomic<bool> *);
} // namespace live_guide
