#include "title_actions_boundary.hpp"
#include "title_actions.hpp"
#include "nuvio_account.h"
#include <cassert>
#include <iostream>
#include <fstream>
json last_entries,last_item;std::string service;bool last_mark=false,last_add=false;
int calls=0;ApiResult response{true,{{"synced",true}},""};
ApiResult api_watched_batch(const std::string& key,const json& rows,bool mark,const std::string& source){assert(key=="profile-key");++calls;last_entries=rows;service=source;last_mark=mark;return response;}
ApiResult api_library_membership_update(const std::string& key,const json& item,bool add,const std::string& source){assert(key=="profile-key");++calls;last_item=item;last_add=add;service=source;return response;}
int main(int argc,char** argv){
 json scenes=json::object();
 const auto capture=[&](const char* name,const sx::DropdownState& d){scenes[name]={{"title",d.title},{"options",d.options},{"message",d.message},{"dialog",d.dialog}};};
 using sx::Session;Session s;s.prefs_.auth_key="profile-key";s.d_item_.id="tt123";s.d_item_.type="movie";s.d_item_.name="Movie";
 s.detail_actions();assert(s.dropdown.options.size()==3&&s.dropdown.options[0]=="Add to library");
 capture("title-actions-movie",s.dropdown);
 s.choose(0);assert(s.title_action_busy_&&s.jobs.size()==1);s.detail_membership(false);assert(s.jobs.size()==1);
 s.complete();assert(last_add&&last_item["_id"]=="tt123"&&service=="nuvio"&&s.reloads==1&&!s.title_action_busy_);
 s.detail_actions();assert(s.dropdown.options[0]=="Remove from library");s.choose(0);s.complete();assert(!last_add&&s.library_["tt123"]["removed"]==true);
 s.detail_actions();s.choose(1);assert(s.watched_.empty());s.complete();assert(s.watched_.count("tt123")&&last_mark&&last_entries[0]["season"].is_null());
 s.detail_actions();s.choose(1);response.ok=false;s.complete();assert(s.watched_.count("tt123"));response.ok=true;
 s.detail_actions();s.choose(1);s.complete();assert(!s.watched_.count("tt123")&&!last_mark);
 s.detail_actions();s.choose(2);assert(s.trailers==1);
 s.d_item_.type="series";s.d_meta_.name="Show";s.d_meta_.videos={Video{"native-a","One","","","2020-01-01",1,1},Video{"native-b","Two","","","2020-01-02",1,2},Video{"future","Future","","","2199-01-01",1,3},Video{"unknown","Unknown","","","",1,4},Video{"s2","Other","","","2020-01-03",2,1}};
 s.detail_actions();assert(s.dropdown.options.size()==7);s.choose(6);assert(s.tvdb_menus==1);s.detail_actions();capture("title-actions",s.dropdown);s.choose(3);capture("watched-confirm",s.dropdown);assert(s.jobs.empty());s.choose(0);assert(s.jobs.empty());
 s.detail_actions();s.choose(3);s.choose(1);s.complete();assert(last_entries.size()==2&&s.watched_.count("tt123:1:1")&&s.watched_.count("tt123:1:2")&&!s.watched_.count("tt123:1:3"));
 s.detail_actions();s.choose(2);s.choose(1);s.complete();assert(last_entries.size()==3&&!s.watched_.count("tt123:1:1"));
 auto original=s.d_meta_.videos[0];s.d_season_videos_.push_back(&original);s.detail_toggle_watched(0);s.complete();assert(last_entries.size()==1&&last_entries[0]["video_id"]=="native-a");
 // Failed or stale completions cannot change another profile's display or history.
 s.detail_actions();s.choose(1);s.choose(1);++s.profile_generation_;s.watched_.clear();s.title_action_busy_=false;s.complete();assert(s.watched_.empty());
 s.detail_actions();s.choose(1);++s.d_meta_gen_;s.choose(1);assert(s.jobs.empty());
 s.authenticated=false;s.detail_membership(true);assert(s.jobs.empty());
 s.authenticated=true;s.d_item_.type="movie";s.watched_.clear();s.detail_actions();s.choose(1);++s.tracking_generation_;s.complete();assert(s.watched_.empty());
 // A malformed/future date cannot be bulk marked; clear explicitly existing marks.
 Meta meta;meta.videos={Video{"bad","","","","2026-02-30",1,1},Video{"unknown","","","","",1,2}};
 auto rows=sx::title_actions::episodes(meta,"tt1",-1,true,{},now_epoch_ms());assert(rows.empty());
 rows=sx::title_actions::episodes(meta,"tt1",-1,false,{"tt1:1:2"},now_epoch_ms());assert(rows.size()==1&&rows[0]["episode"]==2);
 meta.videos.clear();for(int i=1;i<=5001;++i)meta.videos.push_back(Video{std::to_string(i),"","","","2020-01-01",1,i});
 assert(!sx::title_actions::episodes(meta,"tt1",-1,true,{},now_epoch_ms()).is_array());
 if(argc==2){std::ofstream out(argv[1]);out<<scenes.dump(2);}
 std::cout<<"Title actions: production menus, library toggle, movie/season/series and episode marks, future/unknown/invalid dates, confirmation/cancel, bounded scopes, persistence failures and stale profile/title guards passed\n";
}
