// Platform/API/task boundaries; watched Session methods are extracted production code.
#pragma once
#include "../gl/model.hpp"
#include "../gl/series_updates.hpp"
#include "../ui/content.hpp"
#include "../ui/i18n.hpp"
#include "nuvio_account.h"
#include <deque>
#include <functional>
namespace tracking {
inline std::string selected="mdblist";
inline bool provider(const std::string& s){return s=="mdblist"||s=="trakt"||s=="simkl";}
struct Store {explicit Store(const std::string&){} json status()const{return {{"progressPreferred",selected}};}};
}
namespace sx {
struct Session {
 enum {kDiscover=2,kSearch=256,kDetail=16};
 bool profile_ready_=true,d_open_=true,series_history_ready_=false,authenticated=true;
 unsigned watched_revision_=0,profile_generation_=0,changed_=0;
 int d_season_idx_=0;int season_refreshes=0;
 Prefs prefs_;std::string profile_dir_,progress_file_="progress-mdblist.json";
 std::set<std::string>watched_;std::map<std::string,int64_t>watched_scopes_,watched_exclusions_;
 std::map<std::string,Progress>progress_;std::map<std::string,json>library_;
 Item d_item_;Meta d_meta_;bool d_meta_loaded_=false;
 DetailContent detail;DiscoverContent discover;BoardContent board,search;LibraryContent library;
 SeriesUpdates series_updates_;std::deque<std::function<void()>>jobs;std::vector<std::string>messages;
 bool signed_in()const{return authenticated;}
 void say(const std::string& s,double=0,bool=false){messages.push_back(s);}
 void refresh_board(){Item it;it.id="tt123";it.type="movie";Row r;r.items.push_back(make_title(it,false));board.rows={r};}
 void refresh_library(){Item it;it.id="tt123";it.type="movie";library.items={make_title(it,true)};}
 void detail_set_season(int){++season_refreshes;}
 void load_watched();void refresh_watched_views();void load_progress();void save_progress();
 void detail_fill_title();bool watched(const std::string&)const;Title make_title(const Item&,bool)const;
 template<class T>void bg(std::function<T()> work,std::function<void(T&)> done){const auto profile=profile_generation_;jobs.push_back([this,profile,work,done]{auto r=work();if(profile==profile_generation_)done(r);});}
 void complete(){auto f=std::move(jobs.front());jobs.pop_front();f();}
};
}
