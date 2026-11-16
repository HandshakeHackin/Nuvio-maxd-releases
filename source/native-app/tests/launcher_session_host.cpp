#include "launcher_session_boundary.hpp"
#include "start_page.hpp"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
 assert(argc==2);sx::Session s;s.profile_dir_=argv[1];
 live_tv::Channel first;first.id="1";first.name="News HD";first.group="News";first.url="https://first.invalid/secret";first.headers={"Authorization: first"};
 auto second=first;second.id="2";second.name="News FHD";second.url="https://second.invalid/private";second.headers={"Cookie: second"};
 s.live_channels_={first,second};s.live_build();assert(s.live_indices_.size()==2);
 const auto key=live_channel_prefs::key("a",first);
 s.live_channel_menu(1);s.choose(1);s.type("My news");assert(s.live_content.items[1].name=="My news");
 assert(s.live_content.items[1].identity==key&&s.live_channels_[0].name=="News HD");
 s.live_query_="My news";s.live_build();assert(s.live_indices_.size()==1);s.live_query_="News HD";s.live_build();assert(s.live_indices_.size()==1);s.live_query_.clear();s.live_build();
 s.live_guide_window_="previous-window";
 s.live_channel_menu(1);s.choose(2);assert(s.live_indices_.size()==1&&s.live_guide_window_.empty());
 s.live_hidden_channels();assert(s.choices.size()==2&&s.choices[1]=="My news");s.choose(1);assert(s.live_indices_.size()==2);
 s.live_channel_menu(1);s.choose(2);s.live_channel_menu(1);s.choose(2);assert(s.live_indices_.empty());
 s.live_hidden_channels();s.choose(0);assert(s.live_indices_.size()==2&&s.live_content.items[1].name=="My news");
 const auto before=s.live_state_;s.live_guide_window_="retained-window";failed_live_write=true;s.live_channel_menu(1);s.choose(1);s.type("Lost edit");assert(s.live_state_==before&&s.live_content.items[1].name=="My news"&&s.live_guide_window_=="retained-window");failed_live_write=false;
 s.live_channel_menu(1);s.choose(1);auto stale=s.enter;++s.profile_generation_;stale("Other profile");assert(s.live_state_==before);
 s.live_channel_menu(1);s.choose(1);stale=s.enter;++s.live_generation_;stale("Reloaded list");assert(s.live_state_==before);
 const bool hidden=true;assert(live_channel_prefs::set(s.live_state_,"a",live_channel_prefs::key("a",second),nullptr,&hidden));
 s.live_backups(key);assert(s.choices.size()==1);s.choose(0);assert(s.played.url==second.url&&s.played.headers==second.headers&&s.stops==0);
 s.live_backups(s.w_item_.id.substr(5));assert(s.choices.size()==1);auto old=s.pick;++s.w_gen_;old(0);assert(s.stops==0);
 s.live_backups(s.w_item_.id.substr(5));s.choose(0);assert(s.stops==1&&s.played.url==first.url);
 s.watching=false;s.live_groups();assert(s.choices[1]=="News");s.choose(1);assert(s.live_group_=="News"&&live_channel_prefs::last_group(s.live_state_,"a")=="News");
 s.live_groups();assert(s.selected==1);failed_live_write=true;s.choose(0);assert(s.live_group_=="News");failed_live_write=false;
 json stored=s.live_state_;stored["selected"]="a";assert(save_json(s.profile_dir_+"/live-tv.json",stored));s.live_load();assert(s.live_group_=="News"&&s.live_source_=="a");
 s.prefs_.start_page="sports";s.sports_visible=false;s.start_page_apply();assert(s.goto_==sx::Page::board);
 s.profile_ready_=false;s.goto_=sx::Page::library;s.start_page_apply();assert(!s.goto_);s.profile_ready_=true;
 s.start_page_menu();s.choose(3);assert(s.prefs_.start_page=="livetv");s.start_page_apply();assert(s.goto_==sx::Page::livetv);
 s.fail_profile_write=true;s.start_page_menu();s.choose(2);assert(s.prefs_.start_page=="livetv");s.fail_profile_write=false;
 s.start_page_menu();old=s.pick;++s.profile_generation_;old(0);assert(s.prefs_.start_page=="livetv");
 std::cout<<"Launcher Session: production channel lists/actions, original/search identities, hidden restoration, save rollback, stale profile/list/playback guards, header-safe backup switching and start-page scheduling passed\n";
}
