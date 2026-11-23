#include "free_tv_session_boundary.hpp"
#include "config_backup.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>

extern bool public_fails,public_fails_uk;
namespace {
void due(sx::Session& s) {
    const int hours=sx::live_refresh::hours(s.live_state_);
    s.live_refresh_timer_.finished(now_seconds()-hours*3600-1,sx::live_refresh::Outcome::success);
    assert(s.live_refresh_timer_.due(now_seconds()));
}
}
void live_refresh_cases(const std::filesystem::path& root) {
    using sx::live_refresh::Outcome;
    using sx::live_refresh::Timer;
    Timer timer;
    assert(!timer.due(1e10)); // No automatic download before this source has been used.
    for (const auto& choice:sx::live_refresh::choices) {
        timer.reset();timer.configure(choice.hours,1000);timer.finished(1000,Outcome::success);
        assert(!timer.due(1000));
        if (choice.hours) {
            assert(!timer.due(1000+choice.hours*3600-1));
            assert(timer.due(1000+choice.hours*3600));
        } else assert(!timer.due(1e10));
    }
    timer.reset();timer.finished(1000,Outcome::failure);assert(timer.next()==1300);
    timer.finished(1300,Outcome::failure);assert(timer.next()==1900);
    for (int n=0;n<10;++n) timer.finished(2000,Outcome::failure);
    assert(timer.next()==5600&&timer.failures()==5);
    timer.finished(2000,Outcome::deferred);assert(timer.next()==2030&&timer.failures()==5);
    timer.finished(2000,Outcome::success);assert(timer.failures()==0&&timer.next()==2000+6*3600);
    assert(!timer.due(std::numeric_limits<double>::quiet_NaN()));
    for (const auto& value:{json(-1),json(2),json(25),json("6"),json(true),json(6.0),json(nullptr),json(UINT64_MAX)}) {
        json settings={{sx::live_refresh::preference_key,value}};
        assert(!sx::live_refresh::valid(settings)&&sx::live_refresh::hours(settings)==6);
    }
    std::filesystem::create_directories(root/"a");
    std::filesystem::create_directories(root/"b");
    sx::Session s;s.profile_dir_=(root/"a").string();s.live_load();
    assert(sx::live_refresh::hours(s.live_state_)==6);
    s.free_tv_toggle();s.choose(1);g_tasks.drain();
    assert(s.live_channels_.size()==1);
    const auto original=s.live_channels_[0].id;
    due(s);s.watching=true;s.live_refresh_tick();assert(g_tasks.work.empty());s.watching=false;
    for (bool* blocked:{&s.resuming_,&s.multiview_active,&s.backup_busy_,&s.dropdown_open,&s.keyboard_open,&s.launch_open,&s.sign_in_open,&s.release_notes_open}) {
        *blocked=true;s.live_refresh_tick();assert(g_tasks.work.empty());*blocked=false;
    }
    s.profile_ready_=false;s.live_refresh_tick();assert(g_tasks.work.empty());s.profile_ready_=true;
    const auto before=s.live_state_;const auto toast=s.toast;
    s.live_refresh_tick();assert(g_tasks.work.size()==1&&s.live_refresh_inflight_);
    s.live_refresh_tick();assert(g_tasks.work.size()==1);g_tasks.drain();
    assert(!s.live_refresh_inflight_&&s.live_channels_[0].id==original&&s.live_state_==before&&s.toast==toast);
    // A failure keeps both channels and their guide; bounded retries do not spin.
    s.live_guide_auto_url_="https://epg.invalid/last-working.xml";
    public_fails=true;due(s);s.live_refresh_tick();g_tasks.drain();
    assert(s.live_channels_[0].id==original&&s.live_refresh_failed_&&s.live_refresh_timer_.failures()==1);
    assert(s.live_guide_auto_url_=="https://epg.invalid/last-working.xml"&&s.toast==toast);
    s.live_refresh_tick();assert(g_tasks.work.empty());public_fails=false;
    // A partially reachable multi-country catalogue may not erase working countries.
    s.free_tv_apply(true,{"us","uk"});g_tasks.drain();assert(s.live_channels_.size()==2);
    public_fails_uk=true;due(s);s.live_refresh_tick();g_tasks.drain();assert(s.live_channels_.size()==2&&s.live_refresh_failed_);
    public_fails_uk=false;
    s.live_unified_open();g_tasks.drain();assert(s.live_channels_.size()==2&&s.live_source_==unified_tv::source_id);
    public_fails_uk=true;due(s);s.live_refresh_tick();g_tasks.drain();
    assert(s.live_channels_.size()==2&&s.live_refresh_failed_);public_fails_uk=false;
    // Playback can start after the request or even after its network response.
    due(s);s.live_refresh_tick();g_tasks.work_one();s.watching=true;s.live_refresh_tick();
    assert(*s.live_cancel_);g_tasks.ready_one();assert(s.live_channels_.size()==2&&!s.live_loading_&&!s.live_refresh_inflight_);
    s.watching=false;
    // Off cancels only automatic work; an explicit reload still works.
    due(s);s.live_refresh_tick();s.live_refresh_menu();s.choose(0);g_tasks.drain();
    assert(sx::live_refresh::hours(s.live_state_)==0&&!s.live_loading_&&!s.live_refresh_inflight_);
    s.live_refresh_tick();assert(g_tasks.work.empty());s.live_reload();g_tasks.drain();assert(s.live_channels_.size()==2);
    // Saving is private, durable, backup-safe, isolated and rolls back on failure.
    s.live_refresh_menu();s.choose(5);json stored;assert(load_json(s.profile_dir_+"/live-tv.json",stored)&&stored["playlist_refresh_hours"]==24);
    auto capture=config_backup::capture(s.profile_dir_,std::string(64,'b'));assert(capture.error.empty());
    const auto encrypted=config_backup::encrypt(capture.snapshot,"refresh-fixture-passphrase");assert(encrypted.error.empty());
    const auto decrypted=config_backup::decrypt(encrypted.bytes,"refresh-fixture-passphrase",std::string(64,'b'));assert(decrypted.error.empty());
    assert(json::parse(decrypted.snapshot["files"]["live-tv.json"].get<std::string>())["playlist_refresh_hours"]==24);
    auto bad=decrypted.snapshot;auto live=json::parse(bad["files"]["live-tv.json"].get<std::string>());live["playlist_refresh_hours"]=2;
    bad["files"]["live-tv.json"]=live.dump();std::string error;assert(!config_backup::validate(bad,std::string(64,'b'),error));
    const auto state=s.live_state_;s.profile_dir_=(root/"missing").string();s.live_refresh_menu();s.choose(1);assert(s.live_state_==state);
    s.profile_dir_=(root/"a").string();s.live_load();assert(sx::live_refresh::hours(s.live_state_)==24);
    s.live_refresh_menu();auto stale=s.picked;++s.profile_generation_;s.profile_dir_=(root/"b").string();s.live_load();stale(0);
    assert(sx::live_refresh::hours(s.live_state_)==6&&!free_tv::enabled(s.live_state_));s.dropdown_open=false;
    // Exercise the personal M3U download with real HTTP instead of the public-catalogue mock.
    const char* url=std::getenv("TASK_PLAYLIST_FIXTURE_URL");assert(url);
    s.live_state_={{"sources",json::array({{{"id","personal"},{"type","m3u"},{"name","Fixture"},{"url",url}}})},{"playlist_refresh_hours",1}};
    s.live_source_="personal";s.live_refresh_load();s.live_reload();g_tasks.drain();assert(s.live_channels_.size()==1);
    const auto preferences=s.live_state_;due(s);s.live_refresh_tick();g_tasks.drain();assert(s.live_channels_.size()==2&&s.live_state_==preferences);
    const auto guide=s.live_guide_auto_url_;due(s);s.live_refresh_tick();g_tasks.drain();
    assert(s.live_channels_.size()==2&&s.live_guide_auto_url_==guide&&s.live_refresh_failed_);
    // No old-profile reply or timer may install a playlist into the next profile.
    due(s);s.live_refresh_tick();g_tasks.work_one();++s.profile_generation_;s.profile_dir_=(root/"b").string();s.live_load();g_tasks.drain();
    assert(s.live_channels_.empty()&&!s.live_refresh_inflight_&&!s.live_refresh_timer_.due(1e10));
    std::cout<<"Channel refresh: all intervals, real personal/free downloads, no overlap, active-playback/menu cancellation, failed/partial retention, guide preservation, retry bounds, private persistence/backup and profile isolation passed\n";
}
