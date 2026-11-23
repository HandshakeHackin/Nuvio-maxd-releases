// Native PS5 controls inspired by NuvioTVLauncher's channel management.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "live_channel_prefs.hpp"
#include "i18n.hpp"
namespace sx {
bool Session::live_edit_commit(const json& previous) {
 if(!live_save()){live_state_=previous;return false;}
 // Hiding/restoring can reveal channels outside the previous 500-row window.
 // The guide worker reuses its bounded raw document when this window changes.
 live_guide_window_.clear();
 live_build();return true;
}
void Session::live_channel_menu(int index) {
 if(index==0){live_menu();return;}
 if(!profile_ready_||backup_busy_||watching||resuming_||index<1||size_t(index-1)>=live_indices_.size())return;
 const auto channel=live_channels_[live_indices_[size_t(index-1)]];
 const auto view=live_source_;const auto source=unified_tv::owner(channel,view);
 const auto key=live_channel_prefs::key(view,channel);
 const auto profile=profile_generation_,generation=live_generation_;
 open_dropdown(live_channel_prefs::name(live_state_,source,channel),
  {i18n::tr("Watch channel"),i18n::tr("Rename channel"),i18n::tr("Hide channel"),i18n::tr("Backup channels"),i18n::tr("Live TV sources & filters")},0,
  [this,channel,key,source,view,profile,generation](int pick) {
   if(!profile_ready_||profile!=profile_generation_||generation!=live_generation_||view!=live_source_||watching||resuming_||backup_busy_)return;
   if(pick==0){live_play(channel);return;}
   if(pick==3){live_backups(key);return;}
   if(pick==4){live_menu();return;}
   if(pick==2) {
    const auto previous=live_state_;const bool hidden=true;
    if(!live_channel_prefs::set(live_state_,source,key,nullptr,&hidden)){say(i18n::tr("Channel preferences limit reached"),6);return;}
    if(live_edit_commit(previous))say(i18n::tr("Channel hidden. Restore it in Live TV sources & filters."),6);
   }
   if(pick!=1)return;
   ask_text([this,key,source,view,profile,generation](const std::string& value) {
    if(!profile_ready_||profile!=profile_generation_||generation!=live_generation_||view!=live_source_||watching||resuming_||backup_busy_)return;
    const auto name=trim(value);const auto previous=live_state_;
    if(!live_channel_prefs::text(name,live_channel_prefs::max_name)){say(i18n::tr("Use a shorter name on a single line."),7,true);return;}
    if(!live_channel_prefs::set(live_state_,source,key,&name,nullptr)) {
     say(i18n::tr("Channel preferences limit reached"),8,true);return;
    }
    live_edit_commit(previous);
   },i18n::tr("Channel name (empty restores the provider name)"));
  });
 dropdown.active=-1;
}
void Session::live_hidden_channels() {
 if(!profile_ready_||backup_busy_||watching||resuming_)return;
 const auto source=live_source_;const auto profile=profile_generation_,generation=live_generation_;
 std::vector<std::string> rows{i18n::tr("Show all hidden channels")},keys,owners;
 for(const auto& channel:live_channels_) {
  if(!live_channel_prefs::hidden(live_state_,source,channel))continue;
  rows.push_back(live_channel_prefs::name(live_state_,source,channel)+(channel.source_name.empty()?"":" · "+channel.source_name));keys.push_back(live_channel_prefs::key(source,channel));owners.push_back(unified_tv::owner(channel,source));
  if(keys.size()>=200)break;
 }
 if(keys.empty()){open_dropdown(i18n::tr("Hidden channels"),{i18n::tr("Close")},0,[](int){});dropdown.active=-1;dropdown.message=i18n::tr("No hidden channels in the current playlist.");return;}
 open_dropdown(i18n::tr("Hidden channels"),rows,0,[this,source,profile,generation,keys,owners](int pick) {
  if(!profile_ready_||profile!=profile_generation_||generation!=live_generation_||source!=live_source_||watching||resuming_||backup_busy_||pick<0||pick>int(keys.size()))return;
  const auto previous=live_state_;
  if(pick==0) {
   std::set<std::string> sources;
   for(const auto& c:live_channels_)sources.insert(unified_tv::owner(c,source));
   for(const auto& origin:sources) {
    auto& edits=live_state_["channel_edits"][origin];
    for(auto it=edits.begin();it!=edits.end();) {
     it.value().erase("hidden");if(it.value().empty())it=edits.erase(it);else ++it;
    }
    if(edits.empty())live_state_["channel_edits"].erase(origin);
   }
  } else {const bool hidden=false;live_channel_prefs::set(live_state_,owners[size_t(pick-1)],keys[size_t(pick-1)],nullptr,&hidden);}
  if(live_edit_commit(previous)&&pick)live_hidden_channels();
 });
 dropdown.message=i18n::tr("Select a channel to show it again. Up to 200 are listed; Show all restores every hidden channel in this playlist.");
 dropdown.active=-1;
}
void Session::live_backups(const std::string& key) {
 if(!profile_ready_||backup_busy_||resuming_||launch_open)return;
 const auto source=live_source_;const auto profile=profile_generation_,generation=live_generation_;
 const auto playing=watching;const auto watch_generation=w_gen_;
 if(playing&&w_item_.id!="live:"+key)return;
 auto current=std::find_if(live_channels_.begin(),live_channels_.end(),[&](const auto& c){return live_channel_prefs::key(source,c)==key;});
 if(current==live_channels_.end()){say(i18n::tr("Reload this playlist to choose backup channels."),6);return;}
 const auto copies=live_channel_prefs::backups(live_channels_,size_t(current-live_channels_.begin()));
 std::vector<std::string> labels;std::vector<live_tv::Channel> channels;
 for(auto index:copies) {
  const auto& channel=live_channels_[index];
  auto label=live_channel_prefs::name(live_state_,source,channel);
  if(!channel.group.empty())label+=" · "+channel.group;
  if(source==unified_tv::source_id&&!channel.source_name.empty())label+=" · "+channel.source_name;
  labels.push_back(std::move(label));channels.push_back(channel);
 }
 if(labels.empty()){open_dropdown(i18n::tr("Backup channels"),{i18n::tr("Close")},0,[](int){});dropdown.active=-1;dropdown.message=i18n::tr("No other copies were found in the current playlist.");return;}
 open_dropdown(i18n::tr("Backup channels"),labels,0,[this,source,profile,generation,playing,watch_generation,key,channels](int pick) {
  if(!profile_ready_||profile!=profile_generation_||generation!=live_generation_||source!=live_source_||resuming_||backup_busy_||watch_generation!=w_gen_||playing!=watching||pick<0||pick>=int(channels.size()))return;
  if(playing){if(w_item_.id!="live:"+key)return;watch_stop(false);}
  live_play(channels[size_t(pick)]);
 });
 dropdown.message=i18n::tr("Choose another copy from the current playlist. Hidden channels are included. Playback changes only after you choose a channel.");
 dropdown.active=-1;
}
}
