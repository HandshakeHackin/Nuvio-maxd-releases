#include "subtitle_sync_session_boundary.hpp"
#include <cassert>
#include <iostream>
sx::Session session() {
    sx::Session s;s.prefs_.subtitle_auto_sync=true;
    sx::Session::WatchSub addon,file;
    addon.url="https://fixture/sub.srt";addon.lang="eng";file.embedded=2;file.lang="eng";
    double at=10;
    for(int i=0;i<32;++i){at+=4+(i*7%23)*.27;addon.cues.push_back({at-3,at-1.5,"dialogue"});s.player_.timelines[2].push_back({at,at+1.5});}
    s.w_subs_={addon,file};return s;
}
int main(){
    auto s=session();s.prefs_.subtitle_auto_sync=false;s.watch_sync_subtitles();assert(sync_tasks.work.empty());
    s.prefs_.subtitle_auto_sync=true;s.watch_sync_subtitles();assert(sync_tasks.work.size()==1 && s.w_sub_sync_busy_);
    s.w_subs_.push_back(s.w_subs_[0]);s.w_subs_.back().url="https://fixture/other.srt";s.w_sub_active_=2;
    s.watch_sync_subtitles();assert(sync_tasks.work.size()==1 && s.player_.reads==1);
    sync_tasks.drain();assert(s.w_subs_[0].synced && !s.w_subs_[2].synced && !s.w_sub_sync_busy_ && std::abs(s.w_subs_[0].sync_offset-3)<.1);
    auto stale=session();stale.watch_sync_subtitles();sync_tasks.work_one();++stale.w_gen_;sync_tasks.ready_one();assert(!stale.w_subs_[0].synced);
    auto profile=session();profile.watch_sync_subtitles();sync_tasks.work_one();++profile.profile_generation_;sync_tasks.ready_one();assert(!profile.w_subs_[0].synced);
    auto disabled=session();disabled.watch_sync_subtitles();disabled.prefs_.subtitle_auto_sync=false;sync_tasks.drain();assert(!disabled.w_subs_[0].synced && !disabled.w_sub_sync_busy_);
    auto manual=session();manual.watch_sync_subtitles();manual.w_subs_[0].sync_manual=true;sync_tasks.drain();assert(!manual.w_subs_[0].synced);manual.w_subs_[0].sync_after=0;manual.watch_sync_subtitles();assert(sync_tasks.work.empty());
    auto vlc=session();vlc.prefs_.vlc_player=true;vlc.watch_sync_subtitles();assert(sync_tasks.work.empty());
    auto embedded=session();embedded.w_sub_active_=1;embedded.watch_sync_subtitles();assert(sync_tasks.work.empty());
    auto sparse=session();sparse.player_.timelines[2].resize(4);sparse.watch_sync_subtitles();assert(sync_tasks.work.empty() && !sparse.w_subs_[0].synced);
    auto live=session();live.w_item_.type="tv";live.watch_sync_subtitles();assert(sync_tasks.work.empty());
    auto trailer=session();trailer.w_trailer_=true;trailer.watch_sync_subtitles();assert(sync_tasks.work.empty());
    auto cancelled=session();cancelled.watch_sync_subtitles();*cancelled.w_episode_cancel_=true;sync_tasks.drain();assert(!cancelled.w_subs_[0].synced);
    std::cout<<"Production subtitle sync: bounded worker, selected-file identity, generation/profile/cancellation guards, settings and unsupported-source fallback passed\n";
}
