#pragma once
#include "overlays.hpp"
#include <atomic>
#include <functional>
#include <memory>
namespace sx {
struct LocaleCancelSession {
 DropdownState dropdown;bool dropdown_open=false,update_dialog_=false,install_source_dialog_=false,install_source_first_=false;
 double remote_capture_until_=0;std::function<void(int)> dropdown_chosen_;
 std::shared_ptr<std::atomic<bool>> live_usb_cancel_=std::make_shared<std::atomic<bool>>(false);
 struct {bool cancelled=false;void cancel(){cancelled=true;}} updater_;
 void install_source_choose(int){};void dropdown_cancel();void open_dropdown(const std::string&,const std::vector<std::string>&,int,std::function<void(int)>);
};
}
#include "experience_preferences.hpp"
#include "util.h"
namespace sx {
inline constexpr int kUiLanguage=1;
struct LanguageSession {
 DropdownState dropdown;
 std::vector<std::string> choices;
 int active=-1;
 ExperiencePreferences prefs_;
 std::string data_dir_,message;
 bool write_allowed=true,error=false;
 std::function<void(int)> choice;
 void settings_activate(int);
 void open_dropdown(const std::string&,const std::vector<std::string>& names,int selected,std::function<void(int)> fn){choices=names;active=selected;choice=std::move(fn);}
 void save_prefs(){if(write_allowed){json j;save_experience(j,prefs_);save_json(data_dir_+"/settings.json",j);}}
 void refresh_settings(){}
 void say(const std::string& s,double,bool e=false){message=s;error=e;}
};
}
