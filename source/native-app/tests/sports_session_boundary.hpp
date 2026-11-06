#pragma once
#include "content.hpp"
#include "sports_hub.hpp"
#include <deque>
#include <functional>
#include <memory>
#include <optional>
namespace sx {
struct Session {
  unsigned profile_generation_ = 1, live_generation_ = 1;
  bool profile_ready_ = true, watching = false, resuming_ = false,
       multiview_active = false, live_loading_ = false;
  std::string base_dir_, data_dir_, live_source_ = "source";
  Page page_ = Page::sports;
  std::optional<Page> goto_;
  unsigned changed_ = 0;
  enum { kSports = 131072 };
  AddonsContent sports_content;
  SettingsContent settings;
  void sports_settings_snapshot();
  enum {
    kSportsEnable,
    kSportsOpen,
    kSportsRegion,
    kSportsBrowse,
    kSportsFollowing,
    kSportsScores,
    kSportsChannels,
    kSportsClock
  };
  bool sports_visible = false;
  void sports_toggle();
  void sports_menu(int selected = 0);
  void sports_region();
  void sports_browse(const std::string &sport = "");
  void sports_following();
  void sports_scores();
  void sports_channel_filter();
  void sports_clock();
  void sports_activate(int);
  void sports_search();
  void sports_reload(bool force = false);
#include "sports_session_fields.hpp"
  std::vector<live_tv::Channel> live_channels_;
  live_guide::Result live_guide_xml_;
  struct GuideEntry {
    live_guide::Listings programmes;
  };
  std::map<std::string, GuideEntry> live_guide_channels_;
  int saves = 0, plays = 0, reloads = 0, offset = 0;
  live_tv::Channel played;
  int live_guide_offset() const { return offset; }
  void save_profile_settings() { ++saves; }
  void refresh_settings() {}
  void live_reload() { ++reloads; }
  void live_play(const live_tv::Channel &channel) {
    ++plays;
    played = channel;
  }
  std::vector<std::string> choices;
  std::function<void(int)> choice;
  std::function<void(const std::string &)> typed;
  struct {
    std::string title, message;
    int selected = 0;
  } dropdown;
  std::string notice;
  void say(const std::string &n, double, bool = false) { notice = n; }
  void open_dropdown(const std::string &title,
                     const std::vector<std::string> &rows, int selected,
                     std::function<void(int)> cb) {
    dropdown.title = title;
    dropdown.selected = selected;
    dropdown.message.clear();
    choices = rows;
    choice = std::move(cb);
  }
  void ask_text(std::function<void(const std::string &)> cb,
                const std::string &) {
    typed = std::move(cb);
  }
  void pick(int n) {
    auto cb = choice;
    cb(n);
  }
  std::deque<std::function<void()>> jobs;
  template <class T>
  void bg(std::function<T()> work, std::function<void(T &)> done,
          bool = false) {
    const auto p = profile_generation_;
    jobs.push_back([this, p, work, done] {
      auto result = work();
      if (p == profile_generation_)
        done(result);
    });
  }
  void finish() {
    while (!jobs.empty()) {
      auto job = std::move(jobs.front());
      jobs.pop_front();
      job();
    }
  }
};
} // namespace sx
