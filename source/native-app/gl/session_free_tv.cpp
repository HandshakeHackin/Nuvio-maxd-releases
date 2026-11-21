// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "../ui/i18n.hpp"
#include "live_channel_prefs.hpp"

namespace sx {
void Session::free_tv_toggle() {
 if(!profile_ready_){say(i18n::tr("Choose a profile first."),6);return;}
 if(free_tv::enabled(live_state_)){free_tv_apply(false,free_tv::country(live_state_));return;}
 const auto profile=profile_generation_,generation=free_tv_generation_;
 open_dropdown(i18n::tr("Free live TV disclaimer"),{i18n::tr("Cancel"),i18n::tr("I understand — enable free TV")},0,[this,profile,generation](int pick){
  if(pick!=1||profile!=profile_generation_||generation!=free_tv_generation_||!profile_ready_||backup_busy_)return;
  free_tv_apply(true,free_tv::country(live_state_));
 });
 dropdown.message=i18n::tr(free_tv::disclaimer);dropdown.wide=true;
}
void Session::free_tv_apply(bool on,const std::string& region) {
 if(!profile_ready_||backup_busy_||!free_tv::valid_country(region))return;
 const auto previous=live_state_;const auto previous_source=live_source_;
 const auto old_country=free_tv::country(live_state_);const bool was_enabled=free_tv::enabled(live_state_);
 live_state_["free_tv_enabled"]=on;live_state_["free_tv_country"]=free_tv::normal_country(region);
 if(on)live_state_["free_tv_disclaimer_version"]=1;
 if(on&&live_source_.empty())live_source_=free_tv::source_id;
 if(!on&&live_source_==free_tv::source_id) {
  const auto rows=free_tv::sources(live_state_);live_source_=rows.empty()?"":jstr(rows[0],"id");
 }
 if(!live_save()){live_state_=previous;live_source_=previous_source;refresh_settings();return;}
 if(free_tv_cancel_)*free_tv_cancel_=true;
 ++free_tv_generation_;free_tv_country_loading_=false;
 if(previous_source!=live_source_ || (live_source_==free_tv::source_id&&(old_country!=free_tv::country(live_state_)||!was_enabled))) {
  if(previous_source==free_tv::source_id && (watching||multiview_active))watch_stop(false);
  if(live_cancel_)*live_cancel_=true;
  ++live_generation_;live_loading_=false;
  live_channels_.clear();live_indices_.clear();live_group_=live_channel_prefs::last_group(live_state_,live_source_);
  live_query_.clear();live_view_=0;live_guide_reset();live_guide_auto_url_.clear();
  if(!live_source_.empty())live_reload();else live_build();
 }
 refresh_settings();
}
void Session::free_tv_country_menu() {
 if(!profile_ready_){say(i18n::tr("Choose a profile first."),6);return;}
 if(!free_tv::enabled(live_state_)){say(i18n::tr("Read the disclaimer and enable Free live TV first."),7);return;}
 if(free_tv_country_loading_)return;
 if(free_tv_countries_.empty()) {
  if(free_tv_cancel_)*free_tv_cancel_=true;
  free_tv_cancel_=std::make_shared<std::atomic<bool>>(false);const auto cancel=free_tv_cancel_;
  const auto profile=profile_generation_,generation=free_tv_generation_;
  free_tv_country_loading_=true;say(i18n::tr("Loading free TV countries..."));
  bg<free_tv::Countries>([cancel]{return free_tv::countries(cancel.get());},[this,profile,generation,cancel](auto& result){
   if(profile!=profile_generation_||generation!=free_tv_generation_||*cancel||!profile_ready_||!free_tv::enabled(live_state_))return;
   free_tv_country_loading_=false;
   if(!result.error.empty()){say(i18n::tr(result.error.c_str()),7,true);return;}
   free_tv_countries_=std::move(result.rows);if(page_==Page::settings&&!dropdown_open)free_tv_country_menu();
  });return;
 }
 std::vector<std::string> labels;std::vector<std::string> codes;int selected=0;
 for(const auto& row:free_tv_countries_) {
  if(row.code==free_tv::country(live_state_))selected=int(codes.size());
  labels.push_back(row.name);codes.push_back(row.code);
 }
 const auto profile=profile_generation_,generation=free_tv_generation_;
 open_dropdown(i18n::tr("Free TV country"),labels,selected,[this,profile,generation,codes](int pick){
  if(profile!=profile_generation_||generation!=free_tv_generation_||!profile_ready_||!free_tv::enabled(live_state_)||pick<0||size_t(pick)>=codes.size())return;
  free_tv_apply(true,codes[size_t(pick)]);
 });
}
void Session::free_tv_open() {
 if(!profile_ready_||!free_tv::enabled(live_state_)){say(i18n::tr("Read the disclaimer and enable Free live TV first."),7);return;}
 if(live_source_!=free_tv::source_id) {
  const auto previous=live_source_;const auto previous_state=live_state_;live_source_=free_tv::source_id;
  if(!live_save()){live_source_=previous;live_state_=previous_state;return;}
  live_channels_.clear();live_indices_.clear();live_group_=live_channel_prefs::last_group(live_state_,live_source_);
  live_query_.clear();live_view_=0;live_reload();
 }
 if(live_channels_.empty()&&!live_loading_)live_reload();
 goto_=Page::livetv;
}
} // namespace sx
