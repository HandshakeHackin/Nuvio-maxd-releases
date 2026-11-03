#pragma once
#include "media_server.h"
#include <functional>
namespace plex {
using Request = std::function<HttpResponse(const media_server::Connection &,
                                           const std::string &, const json *,
                                           const std::atomic<bool> *)>;
struct Pair {
  std::string device, key, kid, id, code, url, token, error;
  int expires = 600;
  bool pending = true;
};
struct Server {
  std::string id, name, token;
  std::vector<std::string> urls;
};
struct Servers {
  std::vector<Server> items;
  std::string error;
};
class Client {
public:
  explicit Client(Request request = {}) : request_(std::move(request)) {}
  Pair start(const std::atomic<bool> *cancel = nullptr) const;
  Pair poll(Pair, const std::atomic<bool> *cancel = nullptr) const;
  Servers servers(const Pair &,
                  const std::atomic<bool> *cancel = nullptr) const;
  media_server::Result connect(media_server::Connection &, const Pair &,
                               const Server &,
                               const std::atomic<bool> *cancel = nullptr) const;
  media_server::Result browse(const media_server::Connection &,
                              const std::string &, const std::string &,
                              const std::string &, size_t,
                              const std::atomic<bool> *cancel = nullptr) const;
  media_server::Playback
  playback(const media_server::Connection &, const std::string &,
           const std::string &, bool,
           const std::atomic<bool> *cancel = nullptr) const;
  media_server::Result report(const media_server::Connection &,
                              const media_server::Playback &,
                              const std::string &, double, bool,
                              const std::string &,
                              const std::atomic<bool> *cancel = nullptr) const;
  HttpResponse image(const media_server::Connection &, const std::string &,
                     const std::atomic<bool> *cancel = nullptr) const;

private:
  Request request_;
  HttpResponse call(const media_server::Connection &, const std::string &,
                    const json *, const std::atomic<bool> *) const;
  media_server::Result cloud(const std::string &, const std::string &,
                             const std::string &, const json *,
                             const std::atomic<bool> *) const;
  media_server::Result prepare(media_server::Connection &, bool,
                               const std::string &,
                               const std::atomic<bool> *) const;
  HttpResponse server(media_server::Connection, const std::string &,
                      const json *, const std::atomic<bool> *) const;
};
std::string encode(const std::string &);
std::string base64url(const unsigned char *, size_t);
std::string device_jwt(const Pair &, const std::string &nonce = "");
} // namespace plex
