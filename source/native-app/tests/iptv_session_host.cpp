#include "iptv_session_boundary.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
const char *kUserAgent = "Maxd-IPTV-Session-Test";
void http_setup_handle(void *) {}
const std::string &http_ca_bundle() {
  static std::string s;
  return s;
}
int main(int argc, char **argv) {
  assert(argc == 3);
  const std::string base = argv[1], dir = argv[2];
  auto source = [&](const std::string &id, const std::string &path) {
    return json{{"id", id},
                {"name", "My IPTV"},
                {"type", "xtream"},
                {"url", base + path},
                {"username", "fixture-user"},
                {"password", "fixture-pass"}};
  };
  sx::Session s;
  s.profile_dir_ = dir + "/a";
  std::filesystem::create_directories(s.profile_dir_);
  s.live_state_["sources"] = json::array({source("live", "/live")});
  s.iptv_load();
  assert(!s.iptv_visible);
  g_tasks.drain();
  assert(!s.iptv_visible);
  s.live_state_["sources"].push_back(source("vod", "/vod"));
  s.iptv_sources_changed();
  assert(!s.iptv_visible);
  // The new provider is checked first and enables its tab before other sources
  // finish.
  g_tasks.one_work();
  g_tasks.one_ready();
  assert(s.iptv_visible);
  g_tasks.drain();
  assert(s.iptv_source_ == "vod" && s.iptv_content.items.empty());
  s.iptv_reload();
  g_tasks.drain();
  assert(s.iptv_content.items.size() == 2 &&
         s.iptv_content.items[0].name == "Provider movie");
  s.iptv_activate(0);
  assert(s.played.size() == 1 &&
         s.played.back().url ==
             base + "/vod/movie/fixture-user/fixture-pass/19.mkv");
  assert(s.w_item_.id == "iptv:vod:movies:19" &&
         s.w_item_.type == "iptv-movie");
  s.iptv_progress();
  assert(s.iptv_resume(s.w_item_.id) == 30 && s.progress_.empty() &&
         s.library_ == json({{"addon-marker", 1}}));
  s.watching = false;
  auto stored = iptv::load_private(s.profile_dir_ + "/iptv-library.json");
  assert(stored.dump().find("fixture-pass") == std::string::npos &&
         stored.dump().find("/movie/") == std::string::npos);
  s.iptv_activate(0);
  assert(s.choices[0] == "Resume");
  auto choose = s.pick;
  choose(1);
  assert(s.iptv_resume("iptv:vod:movies:19") == 0);
  s.watching = false;
  s.iptv_filter(1);
  assert(s.choices.size() == 2);
  choose = s.pick;
  choose(1);
  g_tasks.drain();
  assert(s.iptv_kind_ == iptv::Kind::series &&
         s.iptv_content.items.size() == 1);
  s.iptv_activate(0);
  assert(s.iptv_series_ == "7");
  g_tasks.drain();
  assert(s.iptv_content.items.size() == 3 &&
         s.iptv_catalog_.entries[0].season == 1);
  s.iptv_filter(2);
  assert(s.choices.size() == 3);
  choose = s.pick;
  choose(2);
  assert(s.iptv_content.items.size() == 1);
  s.iptv_activate(0);
  assert(s.played.back().url ==
             base + "/vod/series/fixture-user/fixture-pass/42.mp4" &&
         s.w_item_.id == "iptv:vod:episodes:42");
  s.watching = false;
  assert(s.iptv_back());
  g_tasks.drain();
  assert(s.iptv_series_.empty() && s.iptv_content.items.size() == 1);
  // Async keyboard and selection callbacks must not affect another profile.
  s.iptv_search();
  auto old_text = s.text;
  s.iptv_filter(0);
  auto old_pick = s.pick;
  s.iptv_reset();
  ++s.profile_generation_;
  s.profile_dir_ = dir + "/b";
  std::filesystem::create_directories(s.profile_dir_);
  s.live_state_["sources"] = json::array();
  s.iptv_load();
  old_text("other profile");
  old_pick(0);
  g_tasks.drain();
  assert(!s.iptv_visible && s.iptv_query_.empty());
  // Saved capabilities show immediately after restart; an outage must retain
  // them and catalog cache.
  s.iptv_reset();
  ++s.profile_generation_;
  s.profile_dir_ = dir + "/a";
  s.live_state_["sources"] = json::array({source("vod", "/vod")});
  s.iptv_load();
  assert(s.iptv_visible);
  g_tasks.drain();
  auto toggle = [&](const std::string &path) {
    auto r = live_tv::download(base + path, nullptr, false, 1024);
    assert(r.ok());
  };
  toggle("/control/offline");
  s.iptv_probe();
  g_tasks.drain();
  assert(s.iptv_visible);
  s.iptv_reload();
  g_tasks.drain();
  assert(!s.iptv_catalog_.entries.empty() &&
         s.iptv_status_.find("Saved catalog retained") != std::string::npos);
  toggle("/control/online");
  s.iptv_reload(true);
  g_tasks.one_work(); // completed old catalog awaiting UI delivery
  toggle("/control/novod");
  s.iptv_probe();
  g_tasks.one_work();
  auto capability = std::move(g_tasks.ready.back());
  g_tasks.ready.pop_back();
  capability();
  assert(!s.iptv_visible && !s.iptv_loading_);
  g_tasks.drain();
  assert(!s.iptv_visible && s.iptv_catalog_.entries.empty());
  toggle("/control/online");
  s.iptv_probe();
  g_tasks.drain();
  assert(s.iptv_visible);
  // Deliver a provider reply after a profile reset: it must not resurrect the
  // old tab.
  toggle("/control/online");
  s.iptv_probe();
  g_tasks.one_work();
  s.iptv_reset();
  ++s.profile_generation_;
  g_tasks.drain();
  assert(!s.iptv_visible && s.iptv_state_.empty());
  s.profile_dir_ = dir + "/a";
  s.live_state_["sources"] = json::array({source("vod", "/vod")});
  s.iptv_load();
  g_tasks.drain();
  assert(s.iptv_visible);
  s.live_state_["sources"] = json::array();
  s.iptv_sources_changed();
  g_tasks.drain();
  assert(!s.iptv_visible && s.iptv_state_["sources"].empty() &&
         s.iptv_state_["progress"].empty());
  assert(std::filesystem::is_empty(s.profile_dir_ + "/iptv-cache"));
  std::cout << "IPTV Session: hidden/live-only, immediate VOD discovery, "
               "direct playback, lazy seasons, isolated resume, offline cache, "
               "profile races and removal passed\n";
}
