// Deterministic scheduler around the production editor and suggestion methods.
#pragma once
#include "search_keyboard.hpp"
#include "search_suggestions.hpp"
#include "stremio.h"
#include "i18n.hpp"
#include <deque>
#include <functional>
#include <memory>
struct SearchTasks {
 std::deque<std::function<void()>> work;
 template<class T>void run(std::function<T()> fn,std::function<void(T&)> done){work.push_back([fn,done]{auto result=fn();done(result);});}
 void finish(){auto job=std::move(work.front());work.pop_front();job();}
};
extern SearchTasks g_tasks;
namespace sx {
struct SearchIme {bool opened=false,secret=true;std::string initial;bool active()const{return opened;}bool open(const std::string&,bool s,const std::string& value){opened=true;secret=s;initial=value;return true;}};
class Session {
public:
 bool watching=false,pin_open=false,search_keyboard_open=false,addons_loading_=false;
 search_keyboard::State search_entry;
 SearchIme ime_;
 unsigned profile_generation_=0;
 std::vector<std::unique_ptr<::Addon>> addons_;
 std::function<void(const std::string&)> text_done_;
 int saves=0;void save_profile_settings(){++saves;}
 void say(const std::string&,int,bool){}
 void ask_search(const std::function<void(const std::string&)>&,const std::string&,const std::string& = "",bool=false);
 void search_keyboard_input(const hui::InputFrame&);
 void search_keyboard_cancel();
 void search_keyboard_changed();
 void search_suggestions_tick();
 std::function<void(const std::string&)> search_keyboard_done_;
 std::string search_keyboard_language_="en";
 std::shared_ptr<std::atomic<bool>> suggest_cancel_=std::make_shared<std::atomic<bool>>(false);
 unsigned suggest_generation_=0;
 int suggest_inflight_=0,suggest_completed_=0,suggest_errors_=0;
 double suggest_after_=0,suggest_deadline_=0;
 bool suggest_prepared_=false;
 std::vector<std::string> suggest_urls_;
 std::size_t suggest_next_=0;
 std::vector<std::vector<search_keyboard::Suggestion>> suggest_rows_;
};
}
