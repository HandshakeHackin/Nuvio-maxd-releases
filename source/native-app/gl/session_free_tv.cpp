// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "../ui/i18n.hpp"
#include "live_channel_prefs.hpp"

namespace sx {
void Session::free_tv_toggle() {
 if(!profile_ready_){say(i18n::tr("Choose a profile first."),6);return;}
 if(free_tv::enabled(live_state_)){free_tv_apply(false,free_tv::selected_countries(live_state_));return;}
 const auto profile=profile_generation_,generation=free_tv_generation_;
 open_dropdown(i18n::tr("Free live TV disclaimer"),{i18n::tr("Cancel"),i18n::tr("I understand — enable free TV")},0,[this,profile,generation](int pick){
  if(pick!=1||profile!=profile_generation_||generation!=free_tv_generation_||!profile_ready_||backup_busy_)return;
  free_tv_apply(true,free_tv::selected_countries(live_state_));
 });
 dropdown.message=i18n::tr(free_tv::disclaimer);dropdown.wide=true;
}
void Session::free_tv_apply(bool on,const std::vector<std::string>& regions) {
 if(!profile_ready_||backup_busy_||!free_tv::valid_selection(json(regions)))return;
 const auto previous=live_state_;const auto previous_source=live_source_;
 const auto old_countries=free_tv::selected_countries(live_state_);const bool was_enabled=free_tv::enabled(live_state_);
 live_state_["free_tv_enabled"]=on;live_state_["free_tv_countries"]=regions;live_state_["free_tv_country"]=free_tv::normal_country(regions.front());
 if(on)live_state_["free_tv_disclaimer_version"]=1;
 if(on&&live_source_.empty())live_source_=free_tv::source_id;
 if(!on&&live_source_==free_tv::source_id) {
  const auto rows=free_tv::sources(live_state_);live_source_=rows.empty()?"":jstr(rows[0],"id");
 }
 if(!live_save()){live_state_=previous;live_source_=previous_source;refresh_settings();return;}
 if(free_tv_cancel_)*free_tv_cancel_=true;
 ++free_tv_generation_;free_tv_country_loading_=false;
 if(previous_source!=live_source_ || (live_source_==free_tv::source_id&&(old_countries!=free_tv::selected_countries(live_state_)||!was_enabled))) {
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
 free_tv_country_choices(free_tv::selected_countries(live_state_),4);
}
void Session::free_tv_country_choices(std::vector<std::string> draft,int focus) {
 std::vector<std::string> labels{i18n::tr("Apply"),i18n::tr("Select all"),i18n::tr("Clear selection"),i18n::tr("Cancel")};
 std::vector<std::string> codes;
 for(const auto& row:free_tv_countries_){labels.push_back(row.name);codes.push_back(row.code);}
 // Retain a previously selected code even if it disappears from the index.
 for(const auto& code:draft)if(std::find(codes.begin(),codes.end(),code)==codes.end()) {
  codes.push_back(code);auto label=code;for(auto& c:label)c=char(c-'a'+'A');labels.push_back(label);
 }
 const auto profile=profile_generation_,generation=free_tv_generation_;
 open_dropdown(i18n::tr("Free TV countries"),labels,-1,[this,profile,generation,codes,draft](int pick) mutable {
  if(profile!=profile_generation_||generation!=free_tv_generation_||!profile_ready_||backup_busy_||!free_tv::enabled(live_state_))return;
  if(pick==3||pick<0)return;
  if(pick==0) {
   if(draft.empty()){say(i18n::tr("Choose at least one country."),6);free_tv_country_choices(draft,0);return;}
   free_tv_apply(true,draft);return;
  }
  if(pick==1){draft.clear();for(const auto& row:free_tv_countries_)draft.push_back(row.code);}
  else if(pick==2)draft.clear();
  else if(size_t(pick-4)<codes.size()) {
   const auto& code=codes[size_t(pick-4)];const auto found=std::find(draft.begin(),draft.end(),code);
   if(found!=draft.end())draft.erase(found);
   else if(draft.size()<free_tv::selection_limit)draft.push_back(code);
  } else return;
  free_tv_country_choices(draft,pick);
 });
 dropdown.selected=std::clamp(focus,0,int(labels.size())-1);dropdown.wide=true;
 dropdown.purpose=DropdownState::Purpose::free_tv_countries;
 dropdown.message=i18n::tr("Cross: toggle countries. Square: apply. Circle: cancel.");
 dropdown.checked.resize(labels.size(),false);
 for(size_t i=0;i<codes.size();++i)dropdown.checked[i+4]=std::find(draft.begin(),draft.end(),codes[i])!=draft.end();
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
