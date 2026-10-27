// Only platform/UI boundaries are faked; all Session IPTV methods are
// production code.
#pragma once
#include "../gl/model.hpp"
#include "../ui/content.hpp"
#include "iptv_library.h"
#include "live_tv.h"
#include <deque>
#include <functional>
#include <map>
#include <memory>
struct IptvTasks {
  std::deque<std::function<void()>> work, ready;
  template <class T, class W, class D> void run(W w, D d) {
    work.push_back([this, w, d] {
      auto result = std::make_shared<T>(w());
      ready.push_back([d, result] { d(*result); });
    });
  }
  void one_work() {
    auto f = std::move(work.front());
    work.pop_front();
    f();
  }
  void one_ready() {
    auto f = std::move(ready.front());
    ready.pop_front();
    f();
  }
  void drain() {
    int n = 0;
    while (!work.empty() || !ready.empty()) {
      if (!work.empty())
        one_work();
      if (!ready.empty())
        one_ready();
      if (++n > 100)
        throw std::runtime_error("unexpected task loop");
    }
  }
};
inline IptvTasks g_tasks;
namespace sx {
struct Session {
#include "iptv_session_fields.hpp"
  LibraryContent iptv_content;
  bool iptv_visible = false;
  void iptv_menu();
  void iptv_search();
  void iptv_filter(int);
  void iptv_activate(int);
  bool iptv_back();
  enum { kIptv = 4096, kIptvNew = 8192 };
  unsigned changed_ = 0, profile_generation_ = 1;
  bool profile_ready_ = true, watching = false, resuming_ = false,
       art_dirty_ = false, d_meta_loaded_ = false;
  std::string profile_dir_;
  json live_state_ = json::object();
  Page page_ = Page::board;
  Item d_item_, w_item_;
  Meta d_meta_;
  std::string d_video_id_;
  DetailContent detail;
  struct {
    double time = 30, duration_value = 100;
    double position() const { return time; }
    double duration() const { return duration_value; }
  } player_;
  std::vector<std::string> choices, messages;
  std::function<void(int)> pick;
  std::function<void(const std::string &)> text;
  struct {
    std::string message;
  } dropdown;
  std::vector<::Stream> played;
  std::map<std::string, Progress> progress_;
  json library_ = json{{"addon-marker", 1}};
  void open_dropdown(const std::string &,
                     const std::vector<std::string> &values, int,
                     std::function<void(int)> done) {
    choices = values;
    pick = std::move(done);
  }
  void ask_text(const std::function<void(const std::string &)> &done,
                const std::string &) {
    text = done;
  }
  void say(const std::string &message, float = 5, bool = false) {
    messages.push_back(message);
  }
  void cancel_autoplay() {}
  void next_preparation_cancel() {}
  void close_detail() {}
  void play_stream(const ::Stream &stream, bool) {
    played.push_back(stream);
    w_item_ = d_item_;
    watching = true;
  }
};
} // namespace sx
