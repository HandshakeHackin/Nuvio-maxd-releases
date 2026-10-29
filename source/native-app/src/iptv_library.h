// Profile-local Xtream VOD. IDs and caches never enter addon catalogs or
// trackers. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "http.h"
#include "json.hpp"
#include <atomic>
#include <functional>
#include <string>
#include <vector>
namespace iptv {
using Json = nlohmann::json;
enum class Kind { movies, series, episodes };
struct Account {
  std::string id, name, server, user, password;
};
struct Category {
  std::string id, name;
};
struct Entry {
  std::string id, name, category, poster, extension, year;
  int season = 0, episode = 0;
};
struct Catalog {
  std::vector<Entry> entries;
  std::string error;
};
struct Capabilities {
  std::vector<Category> movies, series;
  bool movies_ok = false, series_ok = false;
  std::string error;
};
constexpr size_t max_items = 100000, max_catalog_bytes = 128u << 20,
                 max_info_bytes = 8u << 20;
constexpr size_t max_cache_file = 32u << 20, max_cache_total = 128u << 20;
using Request = std::function<HttpResponse(const std::string &, size_t,
                                           const std::atomic<bool> *)>;
std::vector<Account> accounts(const Json &sources);
std::string kind_name(Kind kind);
bool library_type(const std::string &type);
// User-facing errors never echo authenticated URLs or imply every failure is an
// outage.
std::string failure_message(const std::string &error, long http_status = 0,
                            bool playback = true);
std::string identity(const std::string &source, Kind kind,
                     const std::string &id);
std::string playback_url(const Account &, Kind, const Entry &);
Capabilities probe(const Account &, const std::atomic<bool> *,
                   const Request &request = {});
Catalog fetch(const Account &, Kind, const std::string &category_or_series,
              const std::atomic<bool> *, const Request &request = {});
Catalog parse_catalog(const std::string &, Kind,
                      const std::atomic<bool> *cancel = nullptr);
Catalog parse_episodes(const std::string &,
                       const std::atomic<bool> *cancel = nullptr);
// Successful empty responses remove that capability; outages retain known data.
void apply_capabilities(Json &state, const std::string &source,
                        const Capabilities &);
void reconcile(Json &state, const std::vector<Account> &);
bool available(const Json &state, const std::vector<Account> &);
std::vector<Category> categories(const Json &state, const std::string &source,
                                 Kind);
bool save_private(const std::string &path, const Json &);
Json load_private(const std::string &path);
bool save_cache(const std::string &dir, const std::string &source, Kind,
                const std::string &category, const Catalog &,
                const std::atomic<bool> *cancel = nullptr);
bool load_cache(const std::string &dir, const std::string &source, Kind,
                const std::string &category, Catalog &,
                const std::atomic<bool> *cancel = nullptr);
// Search only saved catalogs belonging to the supplied active-profile accounts.
struct SearchHit {
  std::string source, provider;
  Kind kind;
  Entry entry;
};
struct SearchResults {
  std::vector<SearchHit> hits;
  bool limited = false;
};
std::string search_key(const std::string &);
std::string initial(const std::string &);
SearchResults search_saved(const std::string &dir, const std::vector<Account> &,
                           const std::string &query,
                           const std::atomic<bool> *cancel);
void remove_cache(const std::string &dir, const std::string &source);
} // namespace iptv
