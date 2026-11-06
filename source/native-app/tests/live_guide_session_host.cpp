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
  sx::Session clock;
  clock.live_state_={{"sources",json::array({{{"id","source"},{"type","m3u"}}})}};
  assert(clock.live_guide_offset()==0);
  auto raw=std::make_shared<const std::string>("raw provider XML");
  clock.live_guide_document_=raw;clock.live_guide_document_expires_=now_seconds()+100;
  clock.live_guide_clock();assert(clock.choices.size()==49);clock.picked(26);
  assert(clock.live_guide_offset()==60 && clock.saves==1 && clock.live_guide_document_==raw);
  clock.live_guide_clock();clock.picked(26);assert(clock.live_guide_offset()==60 && *raw=="raw provider XML");
  clock.live_guide_clock();auto old_pick=clock.picked;++clock.profile_generation_;old_pick(0);assert(clock.live_guide_offset()==60);
  clock.live_guide_clock();clock.picked(24);assert(clock.live_guide_offset()==0);
  assert(live_guide::offset_minutes({{"guide_offset_minutes",99999}})==720);
  assert(live_guide::offset_minutes({{"guide_offset_minutes","bad"}})==0);
  const auto stamp=now_epoch_ms()/1000;
  clock.live_state_["sources"][0]["guide_offset_minutes"]=60;
  clock.live_guide_channels_["test"].programmes={{stamp-3600-10,stamp-3600+100,"shifted now"},{stamp-3600+100,stamp-3600+200,"shifted next"}};
  live_tv::Channel corrected;corrected.id="test";
  auto corrected_labels=clock.live_guide_labels(corrected);assert(corrected_labels.first.find("shifted now")!=std::string::npos && corrected_labels.second.find("shifted next")!=std::string::npos);
  sx::Session sports;
  sports.page_=sx::Page::sports;
  sports.live_state_={{"sources",json::array({{{"id","source"},{"type","m3u"},{"epg_url",base+"/unused"}}})}};
  live_tv::Channel general, sport;general.id="general";sport.id="sports";
  sports.live_channels_={general,sport};sports.live_indices_={0};sports.sports_guide_indices_={1};
  const time_t before=time(nullptr)-30,after=time(nullptr)+300;std::tm tm{};char from[40],until[40];
  gmtime_r(&before,&tm);std::strftime(from,sizeof(from),"%Y%m%d%H%M%S +0000",&tm);
  gmtime_r(&after,&tm);std::strftime(until,sizeof(until),"%Y%m%d%H%M%S +0000",&tm);
  std::string xml="<tv>";for(const auto& id:{"general","sports"})xml+="<programme channel=\""+std::string(id)+"\" start=\""+from+"\" stop=\""+until+"\"><title>NBA fixture</title></programme>";xml+="</tv>";
  sports.live_guide_document_=std::make_shared<const std::string>(xml);sports.live_guide_document_expires_=now_seconds()+300;
  sports.live_guide_update(0);assert(g_tasks.work.empty());
  sports.sports_visible=true;sports.live_guide_update(0);assert(g_tasks.work.size()==1);g_tasks.drain();
  assert(sports.live_guide_xml_.channels.count("sports")==1&&sports.live_guide_xml_.channels.count("general")==0);
  sports.sports_guide_indices_={0};sports.live_guide_tick_=0;sports.live_guide_update(0);g_tasks.drain();
  assert(sports.live_guide_xml_.channels.count("general")==1&&sports.live_guide_xml_.channels.count("sports")==0);
  std::cout
      << "PASS: guide background bounds, profile/source cancellation, "
         "disabled/playback states, XMLTV override and failure cooldown\n";
}
