#include "free_tv_session_boundary.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
void free_tv_core_cases();
static int public_requests=0;
namespace feature_http {
Response perform(const Query& q,const std::atomic<bool>* cancel) {
 ++public_requests;assert(q.url.starts_with(free_tv::data_root)&&q.headers.empty()&&q.method=="GET"&&q.body.empty());
 Response r;if(cancel&&*cancel){r.error="cancelled";return r;}r.status=200;
 r.body=q.url.ends_with("countries.json")?R"({"us":{"name":"United States"},"uk":{"name":"United Kingdom"}})":R"([{"id":"fixture.us","n":"Fixture TV","c":["news"],"s":[{"u":"https://public.invalid/live.m3u8","o":"iptv","k":1}]}])";
 if(q.url.find("/g/")!=std::string::npos){const auto now=now_epoch_ms()/1000;r.body=json::array({{{"s",now-30},{"e",now+600},{"t","Fixture schedule"}}}).dump();}
 return r;
}
}
int main(int argc,char** argv) {
 assert(argc==2);free_tv_core_cases();
 const auto root=std::filesystem::path(argv[1]);std::filesystem::create_directories(root/"a");std::filesystem::create_directories(root/"b");
 sx::Session s;s.profile_dir_=(root/"a").string();s.live_load();
 assert(!free_tv::enabled(s.live_state_)&&public_requests==0&&g_tasks.work.empty());
 s.free_tv_country_menu();assert(g_tasks.work.empty());
 s.free_tv_toggle();assert(!free_tv::enabled(s.live_state_)&&s.dropdown.wide&&s.dropdown.selected==0);
 assert(s.dropdown.message==free_tv::disclaimer&&s.dropdown.message.find("YMMV")!=std::string::npos&&s.dropdown.message.find("do not report")!=std::string::npos);
 s.choose(0);assert(!free_tv::enabled(s.live_state_)&&public_requests==0);
 s.free_tv_toggle();s.choose(1);assert(free_tv::enabled(s.live_state_)&&s.live_source_==free_tv::source_id&&g_tasks.work.size()==1);
 json disk;assert(load_json(s.profile_dir_+"/live-tv.json",disk)&&free_tv::enabled(disk));
 struct stat facts{};assert(stat((s.profile_dir_+"/live-tv.json").c_str(),&facts)==0&&(facts.st_mode&0777)==0600);
 g_tasks.drain();assert(s.live_channels_.size()==1&&s.live_channels_[0].epg_id=="fixture.us");
 s.free_tv_country_menu();g_tasks.drain();assert(s.dropdown_open&&s.dropdown.options.size()==2);
 s.choose(0);assert(free_tv::country(s.live_state_)=="uk");g_tasks.drain();
 s.page_=sx::Page::livetv;
 for(int i=0;i<8;++i)s.live_channels_.push_back({"guide"+std::to_string(i),"Guide fixture"});
 s.live_build();s.live_guide_update(0);s.live_guide_update(0);
 assert(g_tasks.work.size()==2&&s.live_guide_inflight_==2);g_tasks.drain();
 assert(s.live_guide_channels_.size()==2&&s.live_guide_labels(s.live_channels_[0]).first.find("Fixture schedule")!=std::string::npos);
 s.live_guide_update(4);assert(g_tasks.work.empty()); // Preserve the one-second request gate.
 s.live_guide_tick_=0;s.live_guide_update(4);assert(g_tasks.work.size()==2);g_tasks.work_one();
 s.live_reload();g_tasks.work_one(); // Reply queued before disabling.
 s.watching=true;s.free_tv_toggle();assert(!free_tv::enabled(s.live_state_)&&s.stops==1&&s.live_channels_.empty());
 g_tasks.drain();assert(s.live_channels_.empty()&&s.live_source_.empty()&&s.live_guide_channels_.empty());
 s.free_tv_toggle();assert(s.dropdown.selected==0&&!free_tv::enabled(s.live_state_));s.choose(0);
 // A stale disclaimer from another profile cannot opt the next profile in.
 s.free_tv_toggle();auto stale=s.picked;++s.profile_generation_;s.profile_dir_=(root/"b").string();s.live_load();stale(1);
 assert(!free_tv::enabled(s.live_state_)&&g_tasks.work.empty());
 s.free_tv_toggle();s.choose(1);g_tasks.work_one();++s.profile_generation_;s.live_load();g_tasks.drain();assert(s.live_channels_.empty());
 // Retain a personal source and its active channel while enabling Free TV.
 s.live_state_={{"sources",json::array({{{"id","own"},{"type","m3u"},{"name","Personal"},{"url","https://private.invalid/list?secret=private"}}})},{"favorites",json::array({"own:favorite"})},{"recent",json::array()}};
 const auto own=s.live_state_["sources"];s.live_source_="own";s.live_channels_.push_back({"own-channel","Personal"});
 const int before=public_requests;s.free_tv_toggle();s.choose(1);
 assert(s.live_source_=="own"&&s.live_channels_.size()==1&&s.live_state_["sources"]==own&&public_requests==before&&g_tasks.work.empty());
 s.free_tv_open();assert(s.goto_==sx::Page::livetv&&s.live_source_==free_tv::source_id);g_tasks.drain();
 s.free_tv_toggle();assert(s.live_source_=="own"&&s.live_state_["sources"]==own&&s.live_state_["favorites"][0]=="own:favorite");
 // Do not run the personal playlist request in this transport fixture.
 g_tasks.work.clear();g_tasks.ready.clear();
 auto previous=s.live_state_;s.profile_dir_=(root/"missing").string();s.free_tv_toggle();s.choose(1);
 assert(s.live_state_==previous&&s.live_source_=="own"&&g_tasks.work.empty());
 s.profile_dir_=(root/"b").string();
 std::cout<<"Free TV session: disclaimer acceptance/cancel, private save, rollback, country changes, personal-source preservation, disable/late replies and profile boundaries passed\n";
}
