// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "age_ratings.hpp"
#include "i18n.hpp"
namespace sx {
void Session::age_rating_country_menu() {
 if(!profile_ready_){say(i18n::tr("Choose a profile first."),6);return;}
 const auto profile=profile_generation_;
 std::vector<std::string> choices;
 for(const auto& country:age_ratings::countries)
  choices.push_back(std::string(country.code)+" — "+i18n::tr(country.name));
 open_dropdown(i18n::tr("Age-rating country"),choices,int(age_ratings::country_index(prefs_.age_rating_country)),[this,profile](int pick) {
  if(profile!=profile_generation_||!profile_ready_||pick<0||pick>=int(age_ratings::countries.size()))return;
  const auto previous=prefs_.age_rating_country;
  prefs_.age_rating_country=age_ratings::countries[size_t(pick)].code;
  save_profile_settings();
  json stored;
  if(!load_json(profile_dir_+"/profile-settings.json",stored)||jstr(stored,"age_rating_country")!=prefs_.age_rating_country) {
   prefs_.age_rating_country=previous;say(i18n::tr("Could not save age-rating country. Try again."),8,true);return;
  }
  detail.title.certification=age_ratings::badge(detail.certifications,prefs_.age_rating_country);
  changed_|=kDetail;refresh_settings();cue(hui::audio::Cue::saved);
 });
}
}
