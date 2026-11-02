#pragma once
#include "http.h"
#include "util.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace media_server {
struct Connection {
  std::string id, kind, base, user, user_name, server_name, token, device;
  bool valid() const;
  json saved() const;
  static Connection restore(const json &);
  std::vector<std::string> headers(bool scoped = false) const;
};
struct Result {
  json data;
  std::string error;
  bool ok() const { return error.empty(); }
};
struct Playback {
  std::string url, source, session, method, error;
  std::vector<std::string> headers;
  double resume = 0;
};
class Reporter {
public:
  Reporter();
  ~Reporter();
  void push(const Connection &, const Playback &, const std::string &, double,
            bool, const std::string &);

private:
  struct Job {
    Connection c;
    Playback p;
    std::string item, event;
    double pos;
    bool paused;
  };
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<Job> jobs_;
  std::atomic<bool> stop_{false};
  std::thread thread_;
};
std::string normalize(std::string base, const std::string &kind);
std::string origin(const std::string &url);
bool safe_id(const std::string &);
Result request(const Connection &, const std::string &path,
               const json *body = nullptr,
               const std::atomic<bool> *cancel = nullptr);
Result authenticate(Connection &, const std::string &username,
                    const std::string &password,
                    const std::atomic<bool> *cancel = nullptr);
Result browse(const Connection &, const std::string &parent,
              const std::string &type, const std::string &query, size_t offset,
              const std::atomic<bool> *cancel = nullptr);
Playback playback(const Connection &, const std::string &item,
                  const std::string &source, bool compatible,
                  const std::atomic<bool> *cancel = nullptr);
Result report(const Connection &, const Playback &, const std::string &item,
              double position, bool paused, const std::string &event,
              const std::atomic<bool> *cancel = nullptr);
HttpResponse image(const Connection &, const std::string &item,
                   const std::atomic<bool> *cancel = nullptr);
} // namespace media_server
