// TVDB v4 metadata. Only verified one-to-one correspondences may change order.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "feature_http.h"
#include "stremio.h"
namespace tvdb {
struct Credentials {
  std::string key, pin;
};
struct Order {
  std::string id, name;
};
struct Episode {
  std::string id, title, aired;
  int season = 0, episode = 0;
};
struct Result {
  Meta meta;
  std::vector<Order> orders;
  std::string series_id, error;
};
// Provider order is always available without a key. No fuzzy series lookup.
Result load(const Meta &, const Credentials &, const std::string &order,
            const std::atomic<bool> *, const feature_http::Request & = {});
bool test(const Credentials &, std::string &error, const std::atomic<bool> *,
          const feature_http::Request & = {});
bool reorder(Meta &, const std::vector<Episode> &aired,
             const std::vector<Episode> &selected, std::string &error);
} // namespace tvdb
