#pragma once
#include "remote_bindings.hpp"
#include <functional>
#include <cassert>
inline double fake_remote_time=1;
inline double now_seconds(){return fake_remote_time;}
namespace sx {
struct Session {
 struct {std::vector<RemoteBinding> remote_bindings;} prefs_;
 unsigned profile_generation_=1,remote_capture_generation_=0;
 double remote_capture_until_=0;
 int remote_capture_action_=-1,saves=0;
 bool dropdown_open=false;
 struct {std::string title,message;} dropdown;
 std::vector<std::string> choices,messages;
 std::function<void(int)> dropdown_chosen_;
 nlohmann::json saved=nlohmann::json::object();
 #include "remote_learning_method.hpp"
 void remote_menu();void remote_learn(int);void remote_receive(const RemoteButton&);void remote_capture_tick();
 void open_dropdown(const std::string& title,const std::vector<std::string>& options,int,std::function<void(int)> fn){dropdown.title=title;dropdown.message.clear();choices=options;dropdown_chosen_=std::move(fn);dropdown_open=true;}
 void save_prefs(){++saves;save_remote_bindings(saved,prefs_.remote_bindings);}
 void refresh_settings(){}
 void say(const std::string& text,double=4,bool=false){messages.push_back(text);}
 void pick(int i){auto fn=dropdown_chosen_;dropdown_open=false;dropdown_chosen_=nullptr;if(fn)fn(i);}
};
}
