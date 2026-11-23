// Task/network boundaries only. Session people methods and completion guards are production code.
#pragma once
#include "../gl/model.hpp"
#include "../ui/person.hpp"
#include <deque>
#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include "audio/cues.hpp"
struct PeopleTasks {
 std::deque<std::function<void()>> pending;
 template<class T>void run(std::function<T()> work,std::function<void(T&)> done,bool){pending.push_back([work,done]{auto result=work();done(result);});}
 void one(){auto f=std::move(pending.front());pending.pop_front();f();}
};
inline PeopleTasks g_tasks;
namespace sx {
struct Session {
 enum{kDetail=32,kPerson=262144};unsigned changed_=0,profile_generation_=0,person_generation_=0,cast_generation_=0;
 int d_meta_gen_=0;bool d_meta_loaded_=false,resume_play_=false,d_open_=true,watching=false,resuming_=false,person_open=false,art_dirty_=false;
 Item d_item_;Meta d_meta_;DetailContent detail;PersonContent person;people::Member selected_person_;
 Prefs prefs_;bool profile_ready_=true,fail_save=false;std::string profile_dir_;
 int saves=0,errors=0,refreshes=0;std::function<void(int)> choose;std::vector<std::string> choices;
 void age_rating_country_menu();
 void save_profile_settings(){++saves;if(fail_save)return;json j;load_json(profile_dir_+"/profile-settings.json",j);age_ratings::save_preference(j,prefs_.age_rating_country);assert(save_json(profile_dir_+"/profile-settings.json",j));}
 void refresh_settings(){++refreshes;}
 void cue(hui::audio::Cue){}
 void say(const std::string&,double=6,bool error=false){errors+=error;}
 void open_dropdown(const std::string&,const std::vector<std::string>& values,int,std::function<void(int)> done){choices=values;choose=std::move(done);}
 std::optional<Item> detail_seed_,person_target_;
 std::shared_ptr<std::atomic<bool>> person_cancel_=std::make_shared<std::atomic<bool>>(false),cast_cancel_=std::make_shared<std::atomic<bool>>(false);
 std::vector<std::unique_ptr<::Addon>> addons_;
 void cast_load();bool person_show(int);void person_retry();void person_leave();void person_restore(PersonContent);void person_pick(const people::Credit&);void open_title_item(const Item&);
 void open_title(const Title& t){assert(detail_seed_&&detail_seed_->id==t.id);d_item_=*detail_seed_;detail_seed_.reset();}
 template<class T>void bg(std::function<T()>,std::function<void(T&)>,bool);
};
}
