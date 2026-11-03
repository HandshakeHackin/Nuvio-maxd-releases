#pragma once
#include "../gl/model.hpp"
#include "../ui/content.hpp"
#include "../ui/overlays.hpp"
#include "iptv_library.h"
#include "media_server.h"
#include "plex.h"
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
struct MediaTasks {
  std::deque<std::function<void()>> work, ready;
  template <class T, class W, class D> void run(W w, D d) {
    work.push_back([this, w, d]() mutable {
      auto r = std::make_shared<T>(w());
      ready.push_back([r, d]() mutable { d(*r); });
    });
  }
  void run(std::function<void()> w) { work.push_back(std::move(w)); }
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
        throw std::runtime_error("task loop");
    }
  }
};
inline MediaTasks g_tasks;
namespace sx {
struct Session {
#include "media_session_fields.hpp"
  SignInState sign_in;
  bool sign_in_open = false, login_polling_ = false;
  int login_gen_ = 0;
  double login_next_poll_ = 0, login_expires_ = 0;
  std::string device_provider_;
  std::shared_ptr<std::atomic<bool>> device_cancel_;
  void sign_in_cancel() {
    media_plex_cancel();
    ++login_gen_;
    sign_in_open = false;
    device_provider_.clear();
  }
  LibraryContent media_content;
  bool media_visible = false;
  void media_menu();
  void media_search();
  void media_filter(int);
  void media_activate(int);
  bool media_back();
  unsigned changed_ = 0, profile_generation_ = 1;
  enum { kMedia = 16384, kMediaNew = 32768 };
  bool profile_ready_ = true, watching = false, w_started_ = false,
       art_dirty_ = false, d_meta_loaded_ = false;
  std::string profile_dir_, d_video_id_;
  Item d_item_, w_item_;
  Meta d_meta_;
  DetailContent detail;
  std::optional<Page> goto_;
  struct {
    double pos = 10, dur = 100;
    bool pause = false;
    double position() { return pos; }
    double duration() { return dur; }
    bool paused() { return pause; }
  } player_;
  std::function<void(int)> pick;
  std::function<void(const std::string &)> typed;
  std::vector<std::string> choices, messages;
  int plays = 0, stops = 0;
  ::Stream last_stream;
  void say(const std::string &s, double = 0, bool = false) {
    messages.push_back(s);
  }
  void open_dropdown(const std::string &, const std::vector<std::string> &c,
                     int, std::function<void(int)> f) {
    choices = c;
    pick = std::move(f);
  }
  void ask_text(std::function<void(const std::string &)> f,
                const std::string & = "", bool = false) {
    typed = std::move(f);
  }
  void refresh_settings() {}
  void cancel_autoplay() {}
  void next_preparation_cancel() {}
  void close_detail() {}
  void play_stream(const ::Stream &s, bool) {
    ++plays;
    last_stream = s;
    w_item_ = d_item_;
    watching = true;
  }
  void watch_stop(bool) {
    ++stops;
    media_tick(true);
    watching = false;
  }
};
} // namespace sx
