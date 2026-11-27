#pragma once
#include "stremio.h"
#include "i18n.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <tuple>
namespace sx {
struct Addon {std::string name,version,description,types,initial;bool local=false;unsigned colour=0;};
struct Session {
    struct {std::vector<std::string> extra_addons;json provider_enabled=json::object(),provider_settings=json::object();}prefs_;
    struct {std::string message;std::vector<std::string> options;int selected=0,danger=-1;}dropdown;
    std::string title,last_message;
    std::function<void(int)> callback;
    std::vector<std::unique_ptr<::Addon>> addons_;
    struct {std::vector<Addon> items;}addons;
    std::vector<std::string> addon_logos_;
    std::vector<std::tuple<std::string,bool,bool>> syncs;
    bool inherit_plugins_=false,inherit_addons_=false,art_dirty_=false;
    int changed_=0,reloads=0,retries=0,saves=0,settings=0;
    void addon_activate(int index,int provider_cursor=0);
    void refresh_addons();
    void open_dropdown(const std::string& name,const std::vector<std::string>& options,int selected,std::function<void(int)> fn) {
        title=name;dropdown.message.clear();dropdown.options=options;dropdown.selected=std::max(0,selected);dropdown.danger=-1;callback=std::move(fn);
    }
    void say(const std::string& message,double=0,bool=false){last_message=message;}
    void save_prefs(){++saves;}
    void sync_source(const std::string& url,bool plugin,bool remove){syncs.emplace_back(url,plugin,remove);}
    void load_addons(){++reloads;}
    void addons_reload(){++retries;}
    void provider_options(const std::string&){++settings;}
    void choose(int index){auto fn=callback;fn(index);}
};
inline constexpr int kAddons=1;
inline std::string first_letter(const std::string& value){return value.substr(0,1);}
inline unsigned colour_for(const std::string&){return 0x206ac8;}
}
