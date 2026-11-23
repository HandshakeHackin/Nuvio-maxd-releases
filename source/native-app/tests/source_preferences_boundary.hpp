#pragma once
#include "source_policy.hpp"
#include "episode_playback.hpp"
namespace debrid { inline std::string provider_id(const std::string& id){return id;} }
#include <functional>
namespace sx {
class Session {
public:
    struct {source_policy::Preferences source_preferences;bool builtin_torrents=false;} prefs_;
    bool watching=false,resuming_=false,profile_ready_=true,d_open_=true,d_show_all_sources_=false;
    int profile_generation_=1,d_gen_=1,saves=0,refreshes=0,plays=0,cancellations=0;
    std::string d_video_id_="tt1:1:2",notice,dropdown_title;
    Meta d_meta_;
    std::vector<::Stream> d_streams_,d_found_;
    struct {bool can_resolve(const ::Stream& s)const{return s.debrid_resolvable;}std::string active_provider()const{return "torbox";}} services_;
    DetailContent detail;
    struct {std::string type="movie";} d_item_;
    bool d_cache_warm_=false,autoplay_pending_=false;
    int d_pending_=0,d_answered_=0,changed_=0;
    enum {kDetail=2};
    ::Stream autoplay_previous_,played;
    void detail_preparation_labels(){}
    void play_stream(const ::Stream& s,bool){++plays;played=s;}

    std::vector<std::string> choices;
    std::function<void(int)> choice;
    struct {std::string message;} dropdown;
    void source_options();
    void cancel_autoplay(){++cancellations;}
    void say(const std::string& value,double,bool=false){notice=value;}
    void save_prefs(){++saves;}
    void refresh_settings(){}
    void detail_preparation_cancel(){}
    void detail_refresh_streams();
    void detail_play(int i,bool){if(i>=0&&size_t(i)<d_streams_.size())play_stream(d_streams_[i],false);}
    void open_dropdown(const std::string& title,const std::vector<std::string>& rows,int,std::function<void(int)> cb){dropdown_title=title;choices=rows;choice=std::move(cb);}
    void pick(int i){auto cb=choice;cb(i);}
};
}
