#include "live_guide_session_boundary.hpp"
#include <cassert>
#include <iostream>
const char *kUserAgent = "Maxd-Guide-Session-Fixture";
void http_setup_handle(void *) {}
const std::string &http_ca_bundle() {
  static std::string empty;
  return empty;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string base = argv[1];
  sx::Session s;
  s.live_state_ = {{"sources", json::array({{{"id", "source"},
                                             {"type", "xtream"},
                                             {"url", base},
                                             {"username", "fixture user"},
                                             {"password", "fixture&pass"}}})}};
  for (int i = 0; i < 12; ++i) {
    live_tv::Channel c;
    c.id = std::to_string(i + 42);
    c.epg_id = "one";
    s.live_channels_.push_back(c);
    s.live_indices_.push_back(i);
  }
  s.live_guide_update(0);
  assert(g_tasks.work.size() == 2 && s.live_guide_inflight_ == 2);
  s.live_guide_update(0);
  assert(g_tasks.work.size() == 2);
  g_tasks.drain();
  assert(s.live_guide_channels_.size() == 2);
  auto labels = s.live_guide_labels(s.live_channels_[0]);
  assert(labels.first.find("News") != std::string::npos &&
         labels.second.find("Next") != std::string::npos);
  s.live_guide_tick_ = 0;
  s.live_guide_update(0);
  g_tasks.work_one();
  ++s.profile_generation_;
  s.live_guide_reset();
  g_tasks.drain();
  assert(s.live_guide_channels_.empty() && s.live_guide_inflight_ == 0);
  s.live_guide_update(0);
  g_tasks.work_one();
  s.live_guide_reset();
  g_tasks.drain();
  assert(s.live_guide_channels_.empty());
  s.live_state_["guide_enabled"] = false;
  s.live_guide_update(0);
  assert(g_tasks.work.empty() &&
         s.live_guide_labels(s.live_channels_[0]).first.empty());
  s.live_state_["guide_enabled"] = true;
  s.watching = true;
  s.live_guide_update(0);
  assert(g_tasks.work.empty());
  s.watching = false;
  s.live_guide_auto_url_ = base + "/guide.xml.gz";
  s.live_guide_reset();
  s.live_guide_update(0);
  assert(g_tasks.work.size() == 1);
  g_tasks.drain();
  assert(!s.live_guide_xml_.channels.empty() &&
         s.live_guide_labels(s.live_channels_[0]).first.find("News") !=
             std::string::npos);
  auto document = s.live_guide_document_;
  s.live_filter_ = "new group";
  s.live_guide_tick_ = 0;
  s.live_guide_update(0);
  g_tasks.drain();
  assert(document && s.live_guide_document_ == document &&
         !s.live_guide_xml_.channels.empty());
  const auto channels = s.live_channels_.size();
  s.live_guide_auto_url_ = base + "/unavailable";
  s.live_guide_reset();
  s.live_guide_update(0);
  g_tasks.drain();
  assert(s.live_channels_.size() == channels &&
         s.live_guide_refresh_ > now_seconds());
  s.live_guide_tick_ = 0;
  s.live_guide_update(0);
  assert(g_tasks.work.empty());
  s.live_guide_configure();
  auto stale = s.typed;
  s.live_source_ = "other";
  stale(base + "/guide.xml.gz");
  assert(s.saves == 0);
  std::cout
      << "PASS: guide background bounds, profile/source cancellation, "
         "disabled/playback states, XMLTV override and failure cooldown\n";
}
