// Native Stalker/Ministra protocol client for a provider-authorised account.
// Session tokens exist only for the duration of a worker operation.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "feature_http.h"
#include "iptv_library.h"
#include "live_guide.h"
namespace stalker {
struct Source {
  std::string portal, mac, username, password, serial;
};
Source source(const nlohmann::json &);
std::string endpoint(const std::string &);
bool valid(const Source &);
live_tv::Result channels(const Source &, const std::atomic<bool> *,
                         const feature_http::Request & = {});
struct Link {
  std::string url, error;
  std::vector<std::string> headers;
};
Link resolve(const Source &, const std::string &command, bool vod, int episode,
             const std::atomic<bool> *, const feature_http::Request & = {});
live_guide::Result guide(const Source &, const std::string &channel,
                         int64_t now, const std::atomic<bool> *,
                         const feature_http::Request & = {});
iptv::Capabilities probe(const Source &, const std::atomic<bool> *,
                         const feature_http::Request & = {});
iptv::Catalog catalog(const Source &, iptv::Kind, const std::string &category,
                      const std::atomic<bool> *,
                      const feature_http::Request & = {});
constexpr const char *marker = "nuvio-stalker:";
} // namespace stalker
