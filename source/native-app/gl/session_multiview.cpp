#include "../ui/i18n.hpp"
// Experimental four-screen Live TV. Independent software previews, one PCM port.
#include "session.hpp"
#include "stalker.h"
#include <algorithm>
namespace sx {
void Session::multi_choose(std::vector<live_tv::Channel> chosen,int replace) {
    if(!profile_ready_)return;
    if(live_loading_){say(sx::i18n::tr("Wait for the channel import to finish before opening four screens."),6);return;}
    std::vector<live_tv::Channel> candidates;
    std::vector<std::string> names;
    for(auto index:live_indices_) {
        if(index>=live_channels_.size())continue;
        const auto& c=live_channels_[index];
        if(std::any_of(chosen.begin(),chosen.end(),[&](const auto& a){return a.url==c.url;}))continue;
        if(replace>=0 && std::any_of(multi_channels_.begin(),multi_channels_.end(),[&](const auto& a){return a.url==c.url;}))continue;
        candidates.push_back(c);names.push_back(c.name);
    }
    if(names.empty()){say(sx::i18n::tr("Choose a source/filter with at least four different live channels."),7);return;}
    const auto generation=profile_generation_;
    const auto title=replace>=0?sx::i18n::tr("Replace focused screen"):sx::i18n::tr("Four-screen TV: choose screen ")+std::to_string(chosen.size()+1)+" of 4";
    open_dropdown(title,names,0,
        [this,generation,candidates=std::move(candidates),chosen=std::move(chosen),replace](int i) mutable {
        if(generation!=profile_generation_ || i<0 || size_t(i)>=candidates.size())return;
        if(replace>=0) {
            if(!multiview_active)return;
            multi_channels_[replace]=candidates[size_t(i)];multi_open(replace,replace==multiview_audio);return;
        }
        chosen.push_back(candidates[size_t(i)]);
        if(chosen.size()==4)multi_start(chosen);else multi_choose(std::move(chosen));
    });
    dropdown.message=sx::i18n::tr("Requires four simultaneous provider connections. Use SDR channels up to 1080p; SD is recommended. Channel choices follow the current Live TV page/filter.");
}
void Session::multi_open(int tile,bool audio,const std::string& resolved,const std::vector<std::string>& headers) {
    if(multi_players_[tile])multi_players_[tile]->close();
    multi_surfaces_[tile].reset();multiview_tiles[tile]=MultiTileView{};
    auto& view=multiview_tiles[tile];const auto& channel=multi_channels_[tile];
    view.name=channel.name;view.status=sx::i18n::tr("Connecting...");
    ++multi_resolve_generation_[tile];
    if(multi_resolve_cancel_[tile])*multi_resolve_cancel_[tile]=true;
    if(resolved.empty() && channel.url.rfind(stalker::marker,0)==0) {
        json source;for(const auto& s:live_state_["sources"])if(jstr(s,"id")==live_source_)source=s;
        if(jstr(source,"type")!="stalker"){view.status="Portal source unavailable";return;}
        multi_resolve_cancel_[tile]=std::make_shared<std::atomic<bool>>(false);
        const auto cancel=multi_resolve_cancel_[tile];const auto gen=multi_resolve_generation_[tile],profile=profile_generation_;
        bg<stalker::Link>([source,channel,cancel]{return stalker::resolve(stalker::source(source),channel.url.substr(std::char_traits<char>::length(stalker::marker)),false,0,cancel.get());},
          [this,tile,audio,gen,profile](auto& r){
            if(!multiview_active || profile!=profile_generation_ || gen!=multi_resolve_generation_[tile])return;
            if(!r.error.empty()){multiview_tiles[tile].status=r.error;return;}
            multi_open(tile,audio,r.url,r.headers);
          });return;
    }
    multi_players_[tile]=std::make_unique<Player>();
    Player::Options o;o.url=resolved.empty()?channel.url:resolved;o.headers=resolved.empty()?channel.headers:headers;
    o.live_tv=true;o.preview=true;o.video_only=!audio;o.read_ahead_seconds=2;
    o.audio_mode="pcm";o.night_mode=prefs_.night_mode;o.lfe_mix_percent=prefs_.lfe_mix_percent;
    o.gain_percent=prefs_.audio_gain_percent;o.audio_delay=prefs_.audio_delay_ms/1000.0;
    multi_players_[tile]->open(o);
}
void Session::multi_start(const std::vector<live_tv::Channel>& channels) {
    if(channels.size()!=4 || watching || !profile_ready_)return;
    cancel_autoplay();next_preparation_cancel();++w_gen_;
    resume_play_=false;resuming_=false;w_trailer_=false;
    multiview_focus=multiview_audio=0;multiview_active=true;
    watching=true;launch_open=false;video_hdr=false;video_texture=0;
    w_item_=Item{};w_item_.type="tv";playing=PlayerState{};
    for(int i=0;i<4;++i) {multi_channels_[i]=channels[size_t(i)];multi_open(i,i==0);}
}
void Session::multi_stop() {
    for(int i=0;i<4;++i){++multi_resolve_generation_[i];if(multi_resolve_cancel_[i])*multi_resolve_cancel_[i]=true;}
    for(auto& p:multi_players_) {if(p)p->close();p.reset();}
    for(auto& surface:multi_surfaces_)surface.reset();
    multiview_tiles={};multi_channels_={};multiview_active=false;
    watching=false;launch_open=false;w_started_=false;playing=PlayerState{};
    video_texture=0;video_hdr=false;video_w=video_h=0;
}
void Session::multi_update() {
    for(int i=0;i<4;++i) {
        auto& p=multi_players_[i];auto& v=multiview_tiles[i];if(!p)continue;
        const auto state=p->state();
        if(state==Player::State::Failed) {v.status=p->error();p->close();p.reset();continue;}
        if(const auto* pic=p->present()) {
            std::string error;
            if(!multi_surfaces_[i].upload(*pic,&error,false)) {v.status=error;p->close();p.reset();continue;}
            v.texture=multi_surfaces_[i].texture();v.w=pic->w;v.h=pic->h;v.sar=pic->sar;
        }
        int percent=0;
        if(p->buffering(&percent))v.status=sx::i18n::tr("Buffering ")+std::to_string(percent)+"%";
        else if(state==Player::State::Ended)v.status=sx::i18n::tr("Channel ended");
        else if(p->too_heavy())v.status=sx::i18n::tr("High load: choose an SD channel");
        else v.status=i==multiview_audio?sx::i18n::tr("Audio on"):sx::i18n::tr("Muted");
    }
}
void Session::multi_input(const hui::InputFrame& in) {
    using hui::Action;using hui::Direction;
    if(in.is_pressed(Action::back)){multi_stop();return;}
    if(in.nav==Direction::left && multiview_focus%2) --multiview_focus;
    if(in.nav==Direction::right && !(multiview_focus%2)) ++multiview_focus;
    if(in.nav==Direction::up && multiview_focus>=2)multiview_focus-=2;
    if(in.nav==Direction::down && multiview_focus<2)multiview_focus+=2;
    if(in.is_pressed(Action::confirm) && multiview_focus!=multiview_audio) {
        const int old=multiview_audio;
        // Release the old audio owner before opening the new one.
        multi_open(old,false);multiview_audio=multiview_focus;multi_open(multiview_audio,true);
    }
    if(in.is_pressed(Action::west))multi_choose({},multiview_focus);
    if(in.is_pressed(Action::north)) {
        const auto channel=multi_channels_[multiview_focus];multi_stop();live_play(channel);
    }
}
}
