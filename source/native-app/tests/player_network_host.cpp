// Production Player and NetStream with local HTTP, real FFmpeg and fake console ports.
#define NUVIO_TEST_REAL_NETWORK 1
#define main lifecycle_test_main
#include "player_lifecycle_host.cpp"
#undef main
#include <future>
extern "C" {
#include <libavformat/avformat.h>
}
extern "C" int console_curl_nonblocking(int){return 0;}
static std::atomic<int> refused_seeks{0};
extern "C" int __real_avformat_seek_file(AVFormatContext*,int,int64_t,int64_t,int64_t,int);
extern "C" int __real_av_seek_frame(AVFormatContext*,int,int64_t,int);
extern "C" int __wrap_avformat_seek_file(AVFormatContext* f,int s,int64_t lo,int64_t t,int64_t hi,int flags) {
    if(refused_seeks>0){--refused_seeks;return AVERROR(ENOSYS);}
    return __real_avformat_seek_file(f,s,lo,t,hi,flags);
}
extern "C" int __wrap_av_seek_frame(AVFormatContext* f,int s,int64_t t,int flags) {
    if(refused_seeks>0){--refused_seeks;return AVERROR(ENOSYS);}
    return __real_av_seek_frame(f,s,t,flags);
}
int main(int argc,char** argv) try {
    require(argc==2,"local HTTP URL missing");
    http_init("");
    const std::string base=argv[1];
    for (const auto* name : {"fixture.m3u8", "fixture.mp4", "fixture.mkv"}) {
        Player p; Player::Options o; o.url=base+"/"+name;
        p.open(o); run_until(p,[&]{return !p.buffering() && p.position()>.2;});
        const auto tracks=p.audio_tracks();
        require(!tracks.empty(),"audio tracks missing");
        if(tracks.size()>1) {
            p.select_audio(tracks.back().stream);
            run_until(p,[&]{return !p.buffering() && p.audio_stream()==tracks.back().stream;});
        }
        const int audio=p.audio_stream();
        const auto subs=p.subtitle_tracks();
        if(std::string(name)!="fixture.m3u8") require(subs.size()==1 && subs[0].is_default,"subtitle fixture metadata missing");
        p.set_paused(true);
        const int serial=p.picture_serial();
        p.seek(6);
        run_until(p,[&]{return !p.buffering() && p.picture_serial()!=serial;});
        require(p.paused(),"paused seek resumed playback");
        require(p.position()>=6 && p.position()<6.04,"paused seek target incorrect");
        
        require(p.audio_stream()==audio,"seek changed selected audio");
        require(p.subtitle_tracks().size()==subs.size(),"seek lost subtitle tracks");
        const int preview=p.picture_serial();
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        require(p.present()==nullptr && p.picture_serial()==preview,"paused preview moved");
        p.seek(2);p.seek(8);p.seek(4);
        run_until(p,[&]{return !p.buffering() && p.picture_serial()!=preview;});
        require(p.position()>=4 && p.position()<4.04,"rapid seeks did not land at newest target");
        require(p.audio_stream()==audio,"rapid seek changed audio selection");
        p.set_paused(false);run_until(p,[&]{return p.position()>4.15;});
        p.seek(0);run_until(p,[&]{return !p.buffering() && p.position()<1;});
        p.seek(10);run_until(p,[&]{return !p.buffering() && p.position()>10.15;});
        run_until(p,[&]{return p.state()==Player::State::Ended;});
        p.seek(2);run_until(p,[&]{return !p.buffering() && p.position()>2.15 && p.position()<3;});
        p.close();
        std::cout<<"Network playback: "<<name<<" segment playback, paused/rapid/backward seeks and tracks passed\n";
    }
    {
        Player p;Player::Options o;o.url=base+"/live.m3u8";p.open(o);
        run_until(p,[&]{return !p.buffering() && p.position()>2.2;});p.close();
        std::cout<<"Live HLS: custom-AVIO segment playback and clean close passed\n";
    }
    {
        Player p;Player::Options o;o.url=base+"/fixture.mkv";p.open(o);
        run_until(p,[&]{return !p.buffering() && p.position()>.2;});
        const auto tracks=p.audio_tracks();require(tracks.size()==2,"reopen audio fixture missing");
        p.select_audio(tracks.back().stream);
        run_until(p,[&]{return p.audio_stream()==tracks.back().stream;});
        p.set_paused(true);refused_seeks=2;p.seek(6);
        run_until(p,[&]{return !p.buffering() && p.position()>=6;});
        require(p.paused() && p.audio_stream()==tracks.back().stream,"index-reopen fallback lost pause/audio selection");
        require(p.subtitle_tracks().size()==1,"index-reopen fallback lost embedded subtitle metadata");
        p.close();
        std::cout<<"Matroska seek: refused inline seek safely reopens with pause/audio preserved\n";
    }
    {
        Player p;Player::Options o;o.url=base+"/watch";o.headers={"X-Playback-Test: required"};p.open(o);
        run_until(p,[&]{return !p.buffering() && p.position()>2.2;});p.close();
        std::cout<<"Redirected HLS: extensionless entry, relative segments and request headers passed\n";
    }
    {
        Player p;Player::Options o;o.url=base+"/html-error";p.open(o);
        const double deadline=now_seconds()+5;
        while(p.state()!=Player::State::Failed && now_seconds()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(5));
        require(p.state()==Player::State::Failed && p.error().find("web page")!=std::string::npos,"HTML response was not identified");p.close();
        std::cout<<"Non-media HTML: actionable error instead of demux failure passed\n";
    }
    std::atomic<bool> stop{false}, pending{false};std::string error;
    auto* pb=NetStream::open_avio(base+"/stall-read",{},&stop,&error);
    require(pb!=nullptr,"stalled stream could not open");
    NetStream::set_read_interrupt(pb,&pending);
    auto* ns=NetStream::of(pb);uint8_t bytes[32];
    require(ns->read(bytes,32)==32,"initial stalled data missing");
    auto reader=std::async(std::launch::async,[&]{return ns->read(bytes,32);});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));pending=true;
    require(reader.wait_for(std::chrono::milliseconds(400))==std::future_status::ready,"seek did not interrupt blocked network read");
    require(reader.get()==AVERROR_EXIT && ns->failure().empty(),"seek interruption became a terminal download failure");
    pending=false;require(ns->seek(1000000,SEEK_SET)==1000000,"network reposition failed");
    require(ns->read(bytes,32)==32,"reader could not resume after seek interruption");
    stop=true;NetStream::close_avio(&pb);
    stop=false;pending=false;
    auto opening=std::async(std::launch::async,[&]{
        return NetStream::open_avio(base+"/stall-headers",{},&stop,&error,false,&pending);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));pending=true;
    require(opening.wait_for(std::chrono::seconds(3))==std::future_status::ready,"seek did not cancel segment header wait");
    auto* cancelled=opening.get();
    if(cancelled) NetStream::close_avio(&cancelled);
    require(cancelled==nullptr && error=="cancelled","segment header cancellation did not propagate");
    std::cout<<"Network seeking: blocked read interruption and reusable downloader passed\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
