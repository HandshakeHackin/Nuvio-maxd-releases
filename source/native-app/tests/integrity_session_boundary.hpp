// App/task/UI boundaries only. Production verification scheduling is compiled.
#pragma once
#include "build_integrity.h"
#include "overlays.hpp"
#include "../update/core.hpp"
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
struct IntegrityTasks {
 std::deque<std::function<void()>> pending;
 template<class T>void run(std::function<T()> work,std::function<void(T&)> done,bool=false){pending.push_back([work,done]{auto result=work();done(result);});}
 void one(){auto work=std::move(pending.front());pending.pop_front();work();}
};
inline IntegrityTasks g_tasks;
namespace sx {
struct Session {
 std::string base_dir_="/fixture-app";
 build_integrity::Result integrity_result_;
 std::shared_ptr<std::atomic<bool>> integrity_cancel_=std::make_shared<std::atomic<bool>>(false);
 bool integrity_checking_=false,integrity_warning_pending_=false,integrity_warning_shown_=false;
 bool boot_done_=false,watching=false,resuming_=false,pin_open=false,release_notes_open=false;
 bool dropdown_open=false,sign_in_open=false,install_source_dialog_=false,keyboard=false;
 unsigned profile_generation_=0;int refreshes=0,dialogs=0;
 DropdownState dropdown;std::function<void(int)> pick;
 bool typing()const{return keyboard;}
 void refresh_settings(){++refreshes;}
 void open_dropdown(const std::string& title,const std::vector<std::string>& choices,int,std::function<void(int)> done){dropdown={};dropdown.title=title;dropdown.options=choices;dropdown_open=true;pick=std::move(done);++dialogs;}
 void integrity_start();void integrity_menu();void integrity_maybe_warn();
};
}
