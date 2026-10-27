// Profile-local M3U live channel support. No playlist URLs are logged.
#pragma once
#include "http.h"
#include <atomic>
#include <string>
#include <vector>
namespace live_tv {
constexpr size_t max_bytes = 128u << 20;
constexpr size_t max_metadata_bytes = 8u << 20;
constexpr size_t max_channels = 100000;
struct Channel {
  std::string id, name, group, logo, url;
  std::vector<std::string> headers;
  std::vector<std::string> groups; // all Xtream memberships; group is the display label
  bool in_group(const std::string& name) const;
};
struct Result {
  std::vector<Channel> channels;
  std::string error;
};
// Bounded requests shared with the separate IPTV VOD library.
HttpResponse download(const std::string&,const std::atomic<bool>*,bool redirects,size_t limit=max_bytes);
bool http_url(const std::string &url);
Result parse(const std::string &body, const std::string &base,
             const std::atomic<bool> *cancel = nullptr);
Result parse_xtream(const std::string &channels, const std::string &categories,
                    const std::string &server, const std::string &user,
                    const std::string &password,
                    const std::string &output = "ts",
                    const std::atomic<bool> *cancel = nullptr);
Result fetch_xtream(const std::string &server, const std::string &user,
                    const std::string &password,
                    const std::atomic<bool> *cancel);
Result fetch(const std::string &url, const std::atomic<bool> *cancel);
} // namespace live_tv
