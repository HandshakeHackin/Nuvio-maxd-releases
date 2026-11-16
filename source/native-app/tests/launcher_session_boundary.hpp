// Fake UI/task/IO boundaries; production Session methods implement all actions.
#pragma once
#include "../gl/model.hpp"
#include "../ui/content.hpp"
#include "../src/live_guide.h"
#include "../src/config_backup.h"
#include "../src/live_channel_prefs.hpp"
#include <functional>
#include <optional>
#include <map>
#include "audio/cues.hpp"
inline bool failed_live_write=false;
inline std::map<std::string,std::string> live_writes;
namespace config_backup {
inline bool save(const std::string& path,const std::string& bytes) {if(failed_live_write)return false;live_writes[path]=bytes;return true;}
}
namespace sx {
struct Session {
 Prefs prefs_;json live_state_=json::object();std::string profile_dir_,live_source_="a",live_group_,live_query_,live_filter_;
 bool profile_ready_=true,backup_busy_=false,watching=false,resuming_=false,launch_open=false,live_loading_=false,sports_visible=true,fail_profile_write=false;
 unsigned profile_generation_=1,live_generation_=1,changed_=0;int w_gen_=0,live_view_=0,source_menus=0,stops=0,refreshes=0;
 size_t live_page_=0;enum {kLive=2048};
 std::vector<live_tv::Channel> live_channels_;std::vector<size_t> live_indices_;
 AddonsContent live_content;live_guide::Result live_guide_xml_;
 struct Guide {live_guide::Listings programmes;};std::map<std::string,Guide> live_guide_channels_;
 Item w_item_;live_tv::Channel played;std::optional<Page> goto_;
 struct {std::string message;int active=-1;} dropdown;std::string title,toast;std::vector<std::string> choices;int selected=-1;
 std::function<void(int)> pick;std::function<void(const std::string&)> enter;
 void say(const std::string& s,double=4,bool=false){toast=s;}
 void refresh_settings(){++refreshes;}
 void cue(hui::audio::Cue){}
 void save_profile_settings(){if(!fail_profile_write)save_json(profile_dir_+"/profile-settings.json",json{{"start_page",prefs_.start_page}});}
 void open_dropdown(const std::string& t,const std::vector<std::string>& c,int index,std::function<void(int)> done){title=t;choices=c;selected=index;pick=std::move(done);dropdown.message.clear();}
 void ask_text(std::function<void(const std::string&)> done,const std::string&){enter=std::move(done);}
 void choose(int n){auto fn=pick;fn(n);}void type(const std::string& value){auto fn=enter;fn(value);}
 void live_reset(){++live_generation_;live_state_=json::object();live_channels_.clear();live_indices_.clear();live_group_.clear();live_query_.clear();live_filter_.clear();live_view_=0;}
 void iptv_load(){}
 int live_guide_offset()const{return 0;}
 std::pair<std::string,std::string> live_guide_labels(const live_tv::Channel&)const{return {};}
 void live_menu(){++source_menus;}
 void live_play(const live_tv::Channel& c){played=c;watching=true;w_item_.id="live:"+live_channel_prefs::key(live_source_,c);++w_gen_;}
 void watch_stop(bool){watching=false;++stops;++w_gen_;}
 void live_load();bool live_save();void live_build();void live_groups();
 bool live_edit_commit(const json&);void live_channel_menu(int);void live_hidden_channels();void live_backups(const std::string&);
 void start_page_menu();void start_page_apply();
};
}
