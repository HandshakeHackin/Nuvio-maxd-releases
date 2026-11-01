// Fake task/UI boundaries; all guide Session methods are production code.
#pragma once
#include "../src/live_guide.h"
#include "../src/util.h"
#include "../ui/content.hpp"
#include <deque>
#include <functional>
#include <memory>
struct GuideTasks {
  std::deque<std::function<void()>> work, ready;
  template <class T>
  void run(std::function<T()> w, std::function<void(T &)> d, bool) {
    work.push_back([this, w, d] {
      auto result = std::make_shared<T>(w());
      ready.push_back([d, result] { d(*result); });
    });
  }
  void work_one() {
    auto fn = std::move(work.front());
    work.pop_front();
    fn();
  }
  void ready_one() {
    auto fn = std::move(ready.front());
    ready.pop_front();
    fn();
  }
  void drain() {
    while (!work.empty() || !ready.empty()) {
      if (!work.empty())
        work_one();
      if (!ready.empty())
        ready_one();
    }
  }
};
inline GuideTasks g_tasks;
namespace sx {
struct Session {
  Page page_ = Page::livetv;
  bool watching = false, multiview_active = false, live_loading_ = false;
  unsigned profile_generation_ = 1;
  json live_state_;
  std::string live_source_ = "source", live_filter_;
  size_t live_page_ = 0;
  std::vector<live_tv::Channel> live_channels_;
  std::vector<size_t> live_indices_;
  std::function<void(const std::string &)> typed;
  int builds = 0, saves = 0;
  void ask_text(const std::function<void(const std::string &)> &fn,
                const std::string &) {
    typed = fn;
  }
  void say(const std::string &, double, bool = false) {}
  void live_build() { ++builds; }
  void live_save() { ++saves; }
#include "live_guide_session_fields.hpp"
  template <class T>
  void bg(std::function<T()> w, std::function<void(T &)> d,
          bool urgent = false) {
    const auto profile = profile_generation_;
    g_tasks.run<T>(
        w,
        [this, profile, d](T &r) {
          if (profile == profile_generation_)
            d(r);
        },
        urgent);
  }
};
} // namespace sx
