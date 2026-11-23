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
  // Unified XMLTV: the same provider ID/EPG ID in two sources must stay isolated.
  sx::Session unified;unified.live_source_=unified_tv::source_id;
  unified.live_state_={{"sources",json::array({{{"id","a"},{"type","m3u"},{"name","A"},{"epg_url",base+"/unused"},{"guide_offset_minutes",60}},{{"id","b"},{"type","m3u"},{"name","B"},{"epg_url",base+"/unused"},{"guide_offset_minutes",-30}}})}};
  live_tv::Channel a,b;a.id=b.id="42";a.epg_id=b.epg_id="one";a.source_id="a";b.source_id="b";
  unified.live_channels_={a,b};unified.live_indices_={0,1};
  const auto xml_for=[&](const std::string& title,int offset) {
   std::string from,until;char text[40];std::tm tm{};
   for(int edge:{-30,300}){time_t at=time(nullptr)-offset*60+edge;gmtime_r(&at,&tm);std::strftime(text,sizeof(text),"%Y%m%d%H%M%S +0000",&tm);(edge<0?from:until)=text;}
   return std::make_shared<const std::string>("<tv><programme channel='one' start='"+from+"' stop='"+until+"'><title>"+title+"</title></programme></tv>");
  };
  const auto doc_a=xml_for("Source A programme",60),doc_b=xml_for("Source B programme",-30);
  unified.live_guide_documents_["a"]={doc_a,now_seconds()+300,now_seconds()};
  unified.live_guide_documents_["b"]={doc_b,now_seconds()+300,now_seconds()};
  unified.live_guide_update(0);assert(g_tasks.work.size()==2&&unified.live_guide_inflight_==2);g_tasks.drain();
  assert(unified.live_guide_channels_.size()==2&&unified.live_guide_inflight_==0);
  assert(unified.live_guide_labels(a).first.find("Source A programme")!=std::string::npos);
  assert(unified.live_guide_labels(b).first.find("Source B programme")!=std::string::npos);
  assert(unified.live_guide_documents_["a"].body==doc_a&&unified.live_guide_documents_["b"].body==doc_b);
  assert(unified.live_guide_offset(a)==60&&unified.live_guide_offset(b)==-30);
  unified.live_guide_clock("b");unified.picked(26);
  assert(unified.live_guide_offset(a)==60&&unified.live_guide_offset(b)==60&&unified.live_source_==unified_tv::source_id);
  assert(unified.live_guide_documents_["a"].body==doc_a); // Raw timestamps stay raw.
  unified.live_guide_update(0);g_tasks.work_one();++unified.profile_generation_;unified.live_guide_reset();g_tasks.drain();
  assert(unified.live_guide_channels_.empty()&&unified.live_guide_documents_.empty()&&unified.live_guide_inflight_==0);
  // A failed provider is cooled down without blocking another source.
  unified.live_state_["sources"][0]["epg_url"]=base+"/unavailable";
  unified.live_guide_update(0);g_tasks.drain();assert(unified.live_guide_source_retry_["a"]>now_seconds());
  assert(unified.live_guide_channels_.count("a:42")&&unified.live_guide_channels_.count("b:42"));
  // Colliding Xtream stream IDs use the matching account, never the selected view ID.
  sx::Session accounts;accounts.live_source_=unified_tv::source_id;accounts.live_channels_={a,b};accounts.live_indices_={0,1};
  accounts.live_state_={{"sources",json::array({{{"id","a"},{"type","xtream"},{"url",base+"/a"},{"username","user-a"},{"password","pass-a"}},{{"id","b"},{"type","xtream"},{"url",base+"/b"},{"username","user-b"},{"password","pass-b"}}})}};
  accounts.live_guide_update(0);assert(g_tasks.work.size()==2);g_tasks.drain();
  assert(accounts.live_guide_labels(a).first.find("Provider a")!=std::string::npos&&accounts.live_guide_labels(b).first.find("Provider b")!=std::string::npos);
  // Successful downloads share a raw-document memory budget and parsed-channel LRU.
  unified.live_guide_reset();unified.live_state_["sources"][0]["epg_url"]=base+"/unused";
  unified.live_state_["sources"][1]["epg_url"]=base+"/guide.xml.gz";unified.live_state_["sources"][1]["guide_offset_minutes"]=0;
  auto full=std::make_shared<const std::string>(*doc_a+std::string(live_guide::max_bytes-doc_a->size(),' '));
  unified.live_guide_documents_["a"]={full,now_seconds()+300,now_seconds()-60};
  for(int n=0;n<256;++n)unified.live_guide_channels_["old:"+std::to_string(n)].touched=now_seconds()-300;
  unified.live_guide_update(0);g_tasks.drain();
  size_t retained=0;for(const auto& row:unified.live_guide_documents_)retained+=row.second.body->size();
  assert(retained<=live_guide::max_bytes&&unified.live_guide_documents_.count("b")&&!unified.live_guide_documents_.count("a"));
  assert(unified.live_guide_channels_.size()==256&&unified.live_guide_channels_.count("a:42")&&unified.live_guide_channels_.count("b:42"));
  std::cout<<"PASS: unified source/EPG collisions, per-source clocks, shared concurrency, raw XML reuse, failure cooldown and profile cancellation\n";
  std::cout
      << "PASS: guide background bounds, profile/source cancellation, "
         "disabled/playback states, XMLTV override and failure cooldown\n";
}
