// Hold real demux seeking while the UI continues presenting old queued frames.
#define main lifecycle_fixture_main
#include "player_lifecycle_host.cpp"
#undef main
extern "C" {
#include <libavformat/avformat.h>
int __real_avformat_seek_file(AVFormatContext*,int,int64_t,int64_t,int64_t,int);
int __real_av_seek_frame(AVFormatContext*,int,int64_t,int);
}
static std::atomic<bool> hold_next_seek{false},seek_entered{false},release_seek{false};
extern "C" int __wrap_avformat_seek_file(AVFormatContext* f,int s,int64_t lo,int64_t t,int64_t hi,int flags) {
    if(hold_next_seek.exchange(false)) {
        seek_entered=true;
        while(!release_seek)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return __real_avformat_seek_file(f,s,lo,t,hi,flags);
}
extern "C" int __wrap_av_seek_frame(AVFormatContext* f,int s,int64_t t,int flags) {
    return __real_av_seek_frame(f,s,t,flags);
}
int main(int argc,char** argv) try {
    require(argc==2,"fixture missing");
    Player p;Player::Options options;options.url=argv[1];p.open(options);
    run_until(p,[&]{return !p.buffering() && p.position()>.25;});
    p.set_paused(true);
    for(const auto& targets:{std::pair{6.0,-1.0},std::pair{2.0,3.0}}) {
        seek_entered=false;release_seek=false;hold_next_seek=true;
        p.seek(targets.first);
        const auto deadline=now_seconds()+3;
        while(!seek_entered && now_seconds()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const bool entered=seek_entered;
        bool buffered=true;
        for(int n=0;n<20;++n) {p.present();buffered &= p.buffering();std::this_thread::sleep_for(std::chrono::milliseconds(3));}
        if(targets.second>=0)p.seek(targets.second);
        release_seek=true; // Always release the worker before a failing assertion.
        require(entered,"demux seek boundary was not reached");
        if(!buffered)std::cerr<<"Premature seek release: "<<p.diagnostic_snapshot().dump()<<'\n';
        require(buffered,"old queued frames released buffering during an in-progress seek");
        run_until(p,[&]{return !p.buffering();});
        const double target=targets.second>=0?targets.second:targets.first;
        require(p.position()>=target && p.position()<target+.03,"seek completed on the wrong timeline");
        const double held=p.position();std::this_thread::sleep_for(std::chrono::milliseconds(30));
        require(std::abs(p.position()-held)<.006,"paused seek advanced its clock");
    }
    p.close();std::cout<<"Seek gate: blocked demux keeps buffering, paused target and newer seek override passed\n";
    return 0;
} catch(const std::exception& error) {release_seek=true;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
