// Profile-local M3U live channel support. No playlist URLs are logged.
#pragma once
#include <atomic>
#include <string>
#include <vector>
namespace live_tv {
constexpr size_t max_bytes = 8u << 20;
constexpr size_t max_channels = 10000;
struct Channel {
  std::string id, name, group, logo, url;
  std::vector<std::string> headers;
};
struct Result {
  std::vector<Channel> channels;
  std::string error;
};
bool http_url(const std::string &url);
Result parse(const std::string &body, const std::string &base);
Result parse_xtream(const std::string &channels, const std::string &categories,
                    const std::string &server, const std::string &user,
                    const std::string &password,
                    const std::string &output = "ts");
Result fetch_xtream(const std::string &server, const std::string &user,
                    const std::string &password,
                    const std::atomic<bool> *cancel);
Result fetch(const std::string &url, const std::atomic<bool> *cancel);
} // namespace live_tv
