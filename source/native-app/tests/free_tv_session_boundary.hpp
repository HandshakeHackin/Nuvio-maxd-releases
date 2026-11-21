// Fake UI/task/transport boundaries, with production Session methods and file IO.
#pragma once
#include "../src/free_tv.h"
#include "../src/live_channel_prefs.hpp"
#include "../ui/content.hpp"
#include "../ui/overlays.hpp"
#include <deque>
#include <functional>
#include <optional>
struct FreeTvTasks {
 std::deque<std::function<void()>> work,ready;
 template<class T,class W,class D>void run(W w,D d) {
  work.push_back([this,w,d]{auto result=std::make_shared<T>(w());ready.push_back([d,result]{d(*result);});});
 }
 void work_one(){auto fn=work.front();work.pop_front();fn();}
 void ready_one(){auto fn=ready.front();ready.pop_front();fn();}
 void drain(){int n=0;while(!work.empty()||!ready.empty()){if(!work.empty())work_one();if(!ready.empty())ready_one();if(++n>30)throw std::runtime_error("task loop");}}
};
inline FreeTvTasks g_tasks;
namespace sx {
struct Session {
#include "free_tv_session_fields.hpp"
#include "free_tv_guide_fields.hpp"
 enum {kLive=2048};
 bool profile_ready_=true,backup_busy_=false,watching=false,multiview_active=false,dropdown_open=false,live_loading_=false;
 unsigned profile_generation_=1,live_generation_=0,changed_=0;
 int builds=0,refreshes=0,stops=0,live_view_=0;size_t live_page_=0;
 std::string profile_dir_,live_source_,live_group_,live_query_,live_filter_,toast;
 json live_state_={{"sources",json::array()},{"favorites",json::array()},{"recent",json::array()}};
 std::vector<live_tv::Channel> live_channels_;std::vector<size_t> live_indices_;
 std::shared_ptr<std::atomic<bool>> live_cancel_,live_usb_cancel_;
 AddonsContent live_content;DropdownState dropdown;Page page_=Page::settings;std::optional<Page> goto_;
 bool sports_visible=false;std::vector<size_t> sports_guide_indices_;int sports_guide_cursor_=0;
 std::function<void(int)> picked;
 std::function<void(const std::string&)> typed;
 void say(const std::string& text,double=4,bool=false){toast=text;}
 void open_dropdown(const std::string& title,const std::vector<std::string>& options,int active,std::function<void(int)> done) {
  dropdown=DropdownState{};dropdown.title=title;dropdown.options=options;dropdown.active=dropdown.selected=active;dropdown_open=true;picked=std::move(done);
 }
 void choose(int n){auto fn=picked;dropdown_open=false;fn(n);}
 void ask_text(std::function<void(const std::string&)> fn,const std::string&){typed=std::move(fn);}
 void refresh_settings(){++refreshes;}
 void iptv_reset(){}void iptv_load(){}void iptv_probe(){}
 void live_build(){++builds;live_indices_.clear();for(size_t i=0;i<live_channels_.size();++i)live_indices_.push_back(i);}
 void multi_stop(){multiview_active=false;}
 void watch_stop(bool){watching=false;multiview_active=false;++stops;}
 void live_reset();void live_load();bool live_save();void live_reload();
 template<class T,class W,class D>void bg(W w,D d){g_tasks.run<T>(w,d);}
};
}
