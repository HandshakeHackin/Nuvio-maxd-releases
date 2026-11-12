// Network/task/view boundaries; menu and mutation callbacks are production code.
#pragma once
#include "../gl/model.hpp"
#include "../ui/overlays.hpp"
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <map>
namespace sx {
struct Session {
 bool d_open_=true,watching=false,resuming_=false,title_action_busy_=false,d_meta_loaded_=true,profile_ready_=true,library_loading_=false,authenticated=true;
 unsigned profile_generation_=1,watched_revision_=0;int d_meta_gen_=1,tracking_generation_=0,library_generation_=0,d_season_idx_=0;
 DetailContent detail;Prefs prefs_;Item d_item_;Meta d_meta_;std::vector<int>d_seasons_{1};std::vector<const Video*>d_season_videos_;
 std::map<std::string,json>library_;std::set<std::string>watched_;json tracking_status_;
 DropdownState dropdown;std::function<void(int)>pick;std::deque<std::function<void()>>jobs;std::vector<std::string>messages;
 int saves=0,refreshes=0,reloads=0,trailers=0;
 void detail_actions();void detail_membership(bool);void detail_apply_marks(const json&,bool);void detail_toggle_watched(int);
 bool signed_in()const{return authenticated;}
 bool watched(const std::string& id)const{return watched_.count(id);}
 void say(const std::string& s,double=0,bool=false){messages.push_back(s);}
 void open_dropdown(const std::string& title,const std::vector<std::string>& choices,int,std::function<void(int)> cb,bool=false){dropdown.title=title;dropdown.options=choices;pick=cb;}
 void detail_trailers(){++trailers;}
 void save_progress(){++saves;}void refresh_board(){++refreshes;}void refresh_library(){++refreshes;}void detail_set_season(int){}
 void load_library(){++reloads;}void load_watched(){++reloads;}
 template<class T>void bg(std::function<T()> work,std::function<void(T&)> done){jobs.push_back([work,done]{auto r=work();done(r);});}
 void choose(int n){auto cb=pick;cb(n);}void complete(){auto cb=jobs.front();jobs.pop_front();cb();}
};
}
