#include "../ui/i18n.hpp"
// Native source preferences, independent of any external UI or account service.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "source_policy.hpp"
namespace sx {
void Session::source_options() {
    if(watching||resuming_)return;
    if(!profile_ready_){say(sx::i18n::tr("Choose a profile before changing source preferences."),7);return;}
    cancel_autoplay();
    const auto& p=prefs_.source_preferences;
    std::vector<std::string> rows={
        std::string(sx::i18n::tr("Source checks: "))+(p.checks?"on":"off"),
        std::string(sx::i18n::tr("Hide cinema recordings: "))+(p.hide_cam?"on":"off"),
        sx::i18n::tr("Minimum resolution: ")+(p.min_height?std::to_string(p.min_height)+"p":"any"),
        sx::i18n::tr("Maximum file size: ")+(p.max_gb?std::to_string(p.max_gb)+" GB":"any"),
        "HDR: "+p.hdr,
        sx::i18n::tr("Preferred language: ")+source_policy::language_label(p.language)};
    const bool search=d_open_&&!d_video_id_.empty();
    if(search){rows.push_back(d_show_all_sources_?sx::i18n::tr("Apply source filters"):sx::i18n::tr("Show all sources for this search"));rows.push_back(sx::i18n::tr("Play top-ranked source"));}
    rows.push_back(sx::i18n::tr("Reset source preferences"));
    const auto generation=profile_generation_;const int detail_generation=d_gen_;
    const auto valid=[this,generation,detail_generation,search]{return generation==profile_generation_&&profile_ready_&&!watching&&!resuming_&&(!search||(d_open_&&detail_generation==d_gen_));};
    const auto changed=[this,valid]{
        if(!valid())return;
        save_prefs();refresh_settings();detail_preparation_cancel();
        if(d_open_)detail_refresh_streams();
        source_options();
    };
    open_dropdown(sx::i18n::tr("Source preferences"),rows,0,[this,valid,changed,search](int pick){
        if(!valid()||pick<0)return;
        auto& p=prefs_.source_preferences;
        if(pick==0){p.checks=!p.checks;changed();}
        else if(pick==1){p.hide_cam=!p.hide_cam;changed();}
        else if(pick==2)open_dropdown(sx::i18n::tr("Minimum advertised resolution"),{sx::i18n::tr("Any"),"720p","1080p","2160p / 4K"},0,[this,valid,changed](int i){
            if(!valid()||i<0||i>3)return;
            const int values[]={0,720,1080,2160};prefs_.source_preferences.min_height=values[i];changed();
        });
        else if(pick==3)open_dropdown(sx::i18n::tr("Maximum advertised file size"),{sx::i18n::tr("Any"),"2 GB","5 GB","10 GB","20 GB","40 GB"},0,[this,valid,changed](int i){
            if(!valid()||i<0||i>5)return;
            const int values[]={0,2,5,10,20,40};prefs_.source_preferences.max_gb=values[i];changed();
        });
        else if(pick==4)open_dropdown(sx::i18n::tr("HDR source preference"),{sx::i18n::tr("Any"),sx::i18n::tr("Prefer HDR"),sx::i18n::tr("Avoid HDR-labelled sources")},0,[this,valid,changed](int i){
            if(!valid()||i<0||i>2)return;
            const char* values[]={"any","prefer","avoid"};prefs_.source_preferences.hdr=values[i];changed();
        });
        else if(pick==5)open_dropdown(sx::i18n::tr("Preferred advertised language"),{sx::i18n::tr("Any"),"English",sx::i18n::tr("Spanish"),sx::i18n::tr("French"),sx::i18n::tr("German"),sx::i18n::tr("Italian"),sx::i18n::tr("Portuguese"),sx::i18n::tr("Japanese"),sx::i18n::tr("Korean"),sx::i18n::tr("Hindi")},0,[this,valid,changed](int i){
            if(!valid()||i<0||i>9)return;
            const char* values[]={"","eng","spa","fra","deu","ita","por","jpn","kor","hin"};prefs_.source_preferences.language=values[i];changed();
        });
        else if(search&&pick==6){d_show_all_sources_=!d_show_all_sources_;detail_preparation_cancel();detail_refresh_streams();}
        else if(search&&pick==7){
            detail_refresh_streams();
            if(d_streams_.empty()){say(sx::i18n::tr("No sources match. Adjust preferences or choose Show all sources."),8,true);return;}
            const auto check=source_policy::assess(d_streams_.front(),prefs_.source_preferences,source_policy::context(d_meta_,d_video_id_));
            if(check.hidden||!check.warning.empty()){say(sx::i18n::tr("The top source has a warning. Review the list and select it manually."),8,true);return;}
            // Explicit playback uses the current ranked selection, not an old menu index.
            detail_play(0,false);
        }
        else if(pick==(search?8:6)){p=source_policy::Preferences{};d_show_all_sources_=false;changed();}
    });
    dropdown.message=sx::i18n::tr("Ranks addon labels; playback may differ. Unknown metadata stays available. Show all overrides these filters.");
}
}
