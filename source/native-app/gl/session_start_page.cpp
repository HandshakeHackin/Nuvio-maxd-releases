// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "start_page.hpp"
#include "i18n.hpp"
namespace sx {
void Session::start_page_apply() {
 if(!profile_ready_){goto_.reset();return;}
 if(profile_ready_)goto_=start_page::page(prefs_.start_page,sports_visible);
}
void Session::start_page_menu() {
 if(!profile_ready_){say(i18n::tr("Choose a profile first."),6);return;}
 const auto profile=profile_generation_;
 std::vector<std::string> labels;for(const auto& choice:start_page::choices)labels.emplace_back(i18n::tr(choice.label));
 open_dropdown(i18n::tr("Open Max'd on"),labels,int(start_page::index(prefs_.start_page)),[this,profile](int pick) {
  if(profile!=profile_generation_||!profile_ready_||pick<0||pick>=int(start_page::choices.size()))return;
  const auto previous=prefs_.start_page;
  prefs_.start_page=start_page::choices[size_t(pick)].id;
  save_profile_settings();json saved;
  if(!load_json(profile_dir_+"/profile-settings.json",saved)||jstr(saved,"start_page")!=prefs_.start_page) {
   prefs_.start_page=previous;say(i18n::tr("Could not save the start page. Try again."),8,true);return;
  }
  refresh_settings();cue(hui::audio::Cue::saved);
 });
 dropdown.message=i18n::tr("Choose the page to open after selecting this profile. Disabled Sports falls back to Home. Playback does not start automatically.");
}
}
