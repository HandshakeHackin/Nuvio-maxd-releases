#include "content.hpp"
#include "core/input.hpp"
#include "track_restore.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>

namespace sx {
struct Session {
    struct WatchSub {
        std::string label,lang,url,rich_text;
        int embedded=-1,rich_id=-1;
        bool is_default=false,forced=false,hearing_impaired=false;
    };
    struct Track {int stream;std::string label,lang;bool is_default=false,forced=false,hearing_impaired=false;};
    struct PlayerBoundary {
        std::vector<Track> tracks;
        uint64_t generation=1;
        double target=-1,delay=0;
        bool paused=true;
        uint64_t revision()const{return generation;}
        std::vector<Track> subtitle_tracks()const{return tracks;}
        std::vector<Track> audio_tracks()const{return {};}
        void seek(double t){target=t;}
        void select_audio(int){}
        void set_audio_delay(double d){delay=d;}
        bool set_audio_gain(int){return true;}
    } player_;
    PlayerState playing;
    struct Prefs {int audio_delay_ms=0,subtitle_delay_ms=0,audio_gain_percent=100;}prefs_;
    std::vector<WatchSub> w_subs_;
    int w_sub_active_=-1,automatic=0;
    bool w_menu_built_=false,w_menu_dirty_=false;
    uint64_t w_player_revision_=0;
    double w_chapters_at_=10,w_sub_delay_=0;
    void watch_refresh_tracks();
    void menu_input(const hui::InputFrame&);
    void watch_auto_subtitles(){++automatic;}
    void watch_select_sub(int index){w_sub_active_=index;}
    void watch_show_info(){}
    void watch_build_menu(){}
    void save_prefs(){}
    void say(const char*,int){}
};
// Generated directly from the production Session; only player/UI boundaries
// above are mocked. This catches loss of explicit Off and stale stream IDs.
#include "track_recovery_production.inc"
}
int main() {
    sx::Session s;
    s.player_.tracks={{2,"English","eng"},{3,"English SDH","eng",false,false,true}};
    s.watch_refresh_tracks();assert(s.w_subs_.size()==2 && s.automatic==1);
    s.w_sub_active_=1;s.player_.tracks={{9,"English SDH","eng",false,false,true},{8,"English","eng"}};
    ++s.player_.generation;s.watch_refresh_tracks();
    assert(s.w_sub_active_==0 && s.w_subs_[0].embedded==9 && s.automatic==1 && s.w_player_revision_==2);
    s.w_sub_active_=-1;s.watch_refresh_tracks();assert(s.w_sub_active_==-1 && s.automatic==1);
    sx::Session::WatchSub external;external.label="Addon";external.url="https://example.test/sub";external.rich_id=-5;external.rich_text="captions";
    s.w_subs_.push_back(external);s.w_sub_active_=2;s.watch_refresh_tracks();
    assert(s.w_sub_active_==2 && s.w_subs_[2].rich_text=="captions" && s.w_subs_[2].rich_id==-5);
    s.w_sub_active_=0;s.player_.tracks={{11,"Spanish","spa"}};s.watch_refresh_tracks();assert(s.w_sub_active_==-1);
    sx::Session::WatchSub wanted;wanted.lang="eng";wanted.label="English (in file)";
    auto first=wanted,second=wanted;first.label="Commentary";second.label="Main";
    first.embedded=7;second.embedded=8;
    assert(sx::restored_subtitle(wanted,std::vector{first,second})==-1);
    s.playing.chapters={{0,"Opening"},{4,"Middle"},{8,"End"}};
    s.playing.menu=true;s.playing.menu_column=1;
    hui::InputFrame in;in.nav=hui::Direction::right;s.menu_input(in);assert(s.playing.menu_column==2);
    in={};in.nav=hui::Direction::down;s.menu_input(in);assert(s.playing.menu_row==1);
    in={};in.pressed=hui::action_bit(hui::Action::jump_next)|hui::action_bit(hui::Action::north);s.menu_input(in);
    assert(s.prefs_.subtitle_delay_ms==0 && s.prefs_.audio_delay_ms==0); // chapters cannot alter sync
    in={};in.pressed=hui::action_bit(hui::Action::confirm);s.menu_input(in);
    assert(s.player_.target==4 && s.player_.paused && !s.playing.menu);
    s.playing.chapters.clear();s.playing.menu=true;s.playing.menu_column=1;
    in={};in.nav=hui::Direction::right;s.menu_input(in);assert(s.playing.menu_column==1);
    std::cout<<"Production Session: renumbered subtitles, explicit Off, external cues, missing/ambiguous tracks and chapter controller navigation passed\n";
}
