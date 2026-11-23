#include "free_tv_session_boundary.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include "config_backup.h"
void free_tv_core_cases();
void live_refresh_cases(const std::filesystem::path&);
static int public_requests=0;
bool public_fails=false,public_fails_uk=false;
static std::vector<std::string> guide_paths;
namespace feature_http {
Response perform(const Query& q,const std::atomic<bool>* cancel) {
 ++public_requests;assert(q.url.starts_with(free_tv::data_root)&&q.headers.empty()&&q.method=="GET"&&q.body.empty());
 Response r;if(cancel&&*cancel){r.error="cancelled";return r;}
 if(public_fails||(public_fails_uk&&q.url.ends_with("c/uk.json"))){r.error="private fixture failure";return r;}
 r.status=200;
 r.body=q.url.ends_with("countries.json")?R"({"us":{"name":"United States"},"uk":{"name":"United Kingdom"}})":R"([{"id":"fixture.us","n":"Fixture TV","c":["news"],"s":[{"u":"https://public.invalid/live.m3u8","o":"iptv","k":1}]}])";
 if(q.url.ends_with("c/uk.json")){auto rows=json::parse(r.body);rows[0]["id"]="fixture.uk";r.body=rows.dump();}
 if(q.url.find("/g/")!=std::string::npos){guide_paths.push_back(q.url);const auto now=now_epoch_ms()/1000;r.body=json::array({{{"s",now-30},{"e",now+600},{"t","Fixture schedule"}}}).dump();}
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
 s.free_tv_country_menu();g_tasks.drain();assert(s.dropdown_open&&s.dropdown.options.size()==6);
 assert(!s.dropdown.checked[4]&&s.dropdown.checked[5]); // UK, US alphabetically.
 s.choose(4);assert(s.dropdown.checked[4]&&s.dropdown.checked[5]);
 assert(free_tv::selected_countries(s.live_state_)==std::vector<std::string>{"us"}); // Still a draft.
 s.choose(3);assert(free_tv::selected_countries(s.live_state_).size()==1); // Cancel.
 s.free_tv_country_menu();s.choose(2);s.choose(0);assert(s.dropdown_open); // Cannot apply none.
 s.choose(1);assert(s.dropdown.checked[4]&&s.dropdown.checked[5]);s.choose(3); // Select all remains a draft.
 s.free_tv_country_menu();s.choose(4);s.choose(0);
 assert((free_tv::selected_countries(s.live_state_)==std::vector<std::string>{"us","uk"}));
 g_tasks.drain();assert(s.live_channels_.size()==2&&s.live_channels_[1].guide_country=="uk");
 assert(load_json(s.profile_dir_+"/live-tv.json",disk)&&disk["free_tv_countries"]==json::array({"us","uk"}));
 // Capture applies the production backup whitelist, then restore into another directory.
 const auto backup=config_backup::capture(s.profile_dir_,std::string(64,'a'));
 assert(backup.error.empty());
 const auto packed=config_backup::encrypt(backup.snapshot,"multiple-country-fixture");assert(packed.error.empty());
 const auto unpacked=config_backup::decrypt(packed.bytes,"multiple-country-fixture",std::string(64,'a'));assert(unpacked.error.empty());
 std::string restore_error;
 assert(config_backup::restore((root/"b").string(),unpacked.snapshot,std::string(64,'a'),restore_error));
 json restored;assert(load_json((root/"b/live-tv.json").string(),restored)&&restored["free_tv_countries"]==disk["free_tv_countries"]);
 // Leave the second profile off for the independent profile-isolation cases.
 restored["free_tv_enabled"]=false;assert(config_backup::save((root/"b/live-tv.json").string(),restored.dump()));
 // A pending country picker cannot write into another profile/generation.
 s.free_tv_country_menu();auto stale_countries=s.picked;++s.free_tv_generation_;stale_countries(2);
 assert(free_tv::selected_countries(s.live_state_).size()==2);s.dropdown_open=false;
 s.page_=sx::Page::livetv;
 for(int i=0;i<8;++i)s.live_channels_.push_back({"guide"+std::to_string(i),"Guide fixture"});
 s.live_build();s.live_guide_update(0);s.live_guide_update(0);
 assert(g_tasks.work.size()==2&&s.live_guide_inflight_==2);g_tasks.drain();
 assert(guide_paths.size()==2&&guide_paths[0].ends_with("g/us/fixture.us.json")&&guide_paths[1].ends_with("g/uk/fixture.uk.json"));
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
 // The real combined loader includes the enabled public catalogue and keeps origins.
 if(!free_tv::enabled(s.live_state_)){s.free_tv_toggle();s.choose(1);g_tasks.drain();}s.live_unified_open();g_tasks.drain();
 assert(s.live_source_==unified_tv::source_id&&!s.live_channels_.empty());
 assert(s.live_channels_[0].source_id==free_tv::source_id);
 const auto unified_backup=config_backup::capture(s.profile_dir_,std::string(64,'a'));assert(unified_backup.error.empty());
 const auto unified_saved=json::parse(unified_backup.snapshot["files"]["live-tv.json"].get<std::string>());assert(unified_saved["selected"]==unified_tv::source_id);
 s.page_=sx::Page::livetv;s.live_guide_update(0);g_tasks.drain();
 assert(!s.live_guide_channels_.empty()&&s.live_guide_channels_.begin()->first.starts_with("builtin-free-tv:"));
 s.watching=true;s.free_tv_toggle();assert(!free_tv::enabled(s.live_state_)&&s.live_channels_.empty());
 g_tasks.drain();assert(s.live_channels_.empty()&&s.live_guide_channels_.empty());
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
 live_refresh_cases(root/"refresh");
 std::cout<<"Free TV session: disclaimer acceptance/cancel, private save, rollback, country changes, personal-source preservation, disable/late replies and profile boundaries passed\n";
}
