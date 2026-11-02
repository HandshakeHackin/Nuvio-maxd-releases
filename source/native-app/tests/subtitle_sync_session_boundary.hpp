#pragma once
#include "subtitles.h"
#include "subtitle_sync.hpp"
#include "stremio.h"
#include "util.h"
#include <deque>
#include <functional>
#include <map>
#include <memory>
struct SyncTasks {
    std::deque<std::function<void()>> work,ready;
    template<class T> void run(std::function<T()> w,std::function<void(T&)> done) {
        work.push_back([this,w,done]{auto result=std::make_shared<T>(w());ready.push_back([result,done]{done(*result);});});
    }
    void work_one(){auto f=std::move(work.front());work.pop_front();f();}
    void ready_one(){auto f=std::move(ready.front());ready.pop_front();f();}
    void drain(){while(!work.empty() || !ready.empty()){if(!work.empty())work_one();if(!ready.empty())ready_one();}}
};
inline SyncTasks sync_tasks;
namespace sx {
struct Session {
    struct {bool subtitle_auto_sync=false,vlc_player=false;} prefs_;
    struct {std::string type="movie";} w_item_;
    bool w_trailer_=false,watching=true,w_sub_sync_busy_=false,w_menu_dirty_=false;
    int w_gen_=1,w_sub_active_=0,profile_generation_=1;
    std::shared_ptr<std::atomic<bool>> w_episode_cancel_=std::make_shared<std::atomic<bool>>(false);
    struct {
        std::map<int,std::vector<subtitle_sync::Timing>> timelines;
        int reads=0;
        std::vector<subtitle_sync::Timing> embedded_timeline(int id){++reads;return timelines[id];}
    } player_;
#include "subtitle_sync_session_fields.hpp"
    std::vector<WatchSub> w_subs_;
    void say(const std::string&,double){}
    void watch_sync_subtitles();
    template<class T> void bg(std::function<T()> work,std::function<void(T&)> done) {
        const auto profile=profile_generation_;
        sync_tasks.run<T>(std::move(work),[this,profile,done](auto& result){if(profile==profile_generation_)done(result);});
    }
};
}
