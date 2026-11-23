// Detail menu mutations are bound to the verified profile, title and selected service.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "title_actions.hpp"
#include "nuvio_account.h"
#include "i18n.hpp"
namespace sx {
void Session::detail_actions() {
    if(!d_open_||watching||resuming_)return;
    if(title_action_busy_){say(i18n::tr("Saving title changes..."));return;}
    if(!d_meta_loaded_){say(i18n::tr("Loading..."));return;}
    const auto profile=profile_generation_;const int title=d_meta_gen_;
    const auto member=library_.find(d_item_.id);
    const bool added=member!=library_.end()&&!jbool(member->second,"removed")&&!jbool(member->second,"temp")&&jstr(member->second,"type")==d_item_.type;
    std::vector<std::string> rows{ i18n::tr(added?"Remove from library":"Add to library") };
    const bool series=d_item_.type=="series";
    if(series) {
        rows.push_back(i18n::tr("Mark series watched"));rows.push_back(i18n::tr("Mark series unwatched"));
        rows.push_back(i18n::tr("Mark season watched"));rows.push_back(i18n::tr("Mark season unwatched"));
    } else rows.push_back(i18n::tr(watched_.count(d_item_.id)?"Mark unwatched":"Mark watched"));
    rows.push_back(i18n::tr("Watch trailer"));
    if(series)rows.push_back(i18n::tr("Episode order (TheTVDB)"));
    open_dropdown(i18n::tr("Title actions"),rows,0,[this,profile,title,added,series](int pick){
        if(profile!=profile_generation_||title!=d_meta_gen_||!d_open_||watching||resuming_||title_action_busy_)return;
        if(series && pick==6){detail_tvdb_menu();return;}
        if(pick==0){detail_membership(!added);return;}
        if((series&&pick==5)||(!series&&pick==2)){detail_trailers();return;}
        if(!series&&pick==1){
            json row{{"content_id",d_item_.id},{"content_type","movie"},{"title",d_item_.name},{"season",nullptr},{"episode",nullptr}};
            detail_apply_marks(json::array({row}),!watched_.count(d_item_.id));return;
        }
        if(series&&pick>=1&&pick<=4) {
            const int season=pick<=2?-1:(d_season_idx_>=0&&d_season_idx_<int(d_seasons_.size())?d_seasons_[d_season_idx_]:-2);
            if(season==-2)return;
            const bool marking=pick==1||pick==3;
            const auto entries=title_actions::episodes(d_meta_,d_item_.id,season,marking,watched_,now_epoch_ms());
            if(!entries.is_array()){say(i18n::tr("Too many episodes. Choose a season instead."),8,true);return;}
            if(entries.empty()){say(i18n::tr("No released episodes with known dates are available for this action."),8);return;}
            open_dropdown(i18n::tr(marking?(season<0?"Mark series watched":"Mark season watched"):(season<0?"Mark series unwatched":"Mark season unwatched")),{i18n::tr("Cancel"),i18n::tr("Confirm")},0,
                [this,profile,title,entries,marking](int confirm){
                    if(confirm==1&&profile==profile_generation_&&title==d_meta_gen_&&d_open_&&!watching&&!resuming_)detail_apply_marks(entries,marking);
                });
            dropdown.dialog=true;dropdown.poster=detail.title.poster;
            dropdown.message=std::to_string(entries.size())+i18n::tr(marking?" released episodes. Resume positions stay unchanged.":" listed episodes. Resume positions stay unchanged.");
        }
    });
    dropdown.message=i18n::tr(jstr(tracking_status_,"libraryPreferred")=="simkl"?"Library: Nuvio only; SIMKL is read-only here. Watched: selected progress service.":"Library: Nuvio + Trakt/MDBList when selected. Watched: selected progress service.");
}
void Session::detail_membership(bool add) {
    if(!signed_in()||!profile_ready_){say(i18n::tr("Sign in to Nuvio to change your library."),7);return;}
    if(title_action_busy_||!d_open_||watching||resuming_)return;
    const auto key=prefs_.auth_key;const auto profile=profile_generation_;
    const auto source=jstr(tracking_status_,"libraryPreferred","nuvio");
    const json item{{"_id",d_item_.id},{"type",d_item_.type},{"name",d_item_.name},{"poster",d_item_.poster},
        {"background",d_item_.background},{"description",d_item_.description},{"releaseInfo",d_item_.release_info}};
    const auto tracking=tracking_generation_;
    title_action_busy_=true;++library_generation_;library_loading_=false;
    bg<ApiResult>([key,item,add,source]{return api_library_membership_update(key,item,add,source);},
        [this,key,profile,item,add,tracking](ApiResult& r){
            if(profile!=profile_generation_||key!=prefs_.auth_key)return;
            title_action_busy_=false;
            if(!r.ok){say(r.error,9,true);return;}
            if(tracking!=tracking_generation_){load_library();return;}
            auto& local=library_[jstr(item,"_id")];if(!local.is_object())local=item;
            local["removed"]=!add;local["temp"]=!add;
            say(i18n::tr(add?"Added to library":"Removed from library"));
            if(!jbool(r.result,"synced"))say(std::string(i18n::tr("Library change saved; some services still need to sync."))+" "+jstr(r.result,"trackerError"),10);
            refresh_library();refresh_board();load_library();
        });
}
void Session::detail_apply_marks(const json& entries,bool marking) {
    if(title_action_busy_||!d_open_||watching||resuming_||!entries.is_array()||entries.empty())return;
    const auto apply=[this,entries,marking]{
        for(const auto& row:entries){const auto canonical=title_actions::video_id(row),native=jstr(row,"video_id");
            if(marking)watched_.insert(canonical);
            else{watched_.erase(canonical);watched_.erase(native);}}
        save_progress();refresh_watched_views();
        say(i18n::tr(marking?"Marked as watched":"Marked as not watched"));
    };
    if(!signed_in()){apply();return;}
    if(!profile_ready_)return;
    const auto key=prefs_.auth_key,source=jstr(tracking_status_,"progressPreferred","nuvio");const auto profile=profile_generation_;
    const auto tracking=tracking_generation_;
    ++watched_revision_;title_action_busy_=true;
    bg<ApiResult>([key,entries,marking,source]{return api_watched_batch(key,entries,marking,source);},
        [this,key,profile,apply,tracking](ApiResult& r){
            if(profile!=profile_generation_||key!=prefs_.auth_key)return;
            title_action_busy_=false;
            if(!r.ok){say(r.error,9,true);return;}
            if(tracking!=tracking_generation_){load_watched();return;}
            apply();
            if(!jbool(r.result,"synced"))say(jstr(r.result,"notice").empty()?i18n::tr("Watched changes saved on this PS5; the selected service will retry."):i18n::tr("Saved on this PS5. ")+jstr(r.result,"notice"),9);
        });
}
}
