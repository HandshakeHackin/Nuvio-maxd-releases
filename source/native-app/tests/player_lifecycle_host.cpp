// Production demux/audio/seek/recovery with a deliberately failing hardware boundary.
#include "player.h"
#include "evo_backend.h"
#include "torrent/torrent_stream.h"
#include "http.h"
#include "util.h"
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <array>
#include <chrono>
#include <cstring>
#include <thread>
#include <iostream>
#include <stdexcept>
static bool hardware=false;
static std::atomic<int> pcm_ports{0},max_pcm_ports{0},hw_opens{0},pcm_peak{0};
static std::atomic<bool> fail_bitstream{false},fail_drain{false};
static std::atomic<bool> failed_hardware{false};
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
extern "C" int sceAudioOutInit(){return 0;}
extern "C" int sceAudioOutOpen(int,int,int,unsigned,unsigned,unsigned){
    // Widen the failed-bitstream -> closed -> PCM-open transition. The main
    // thread must wait for that transaction, not fail on its intermediate kind.
    if(fail_bitstream) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const int ports=++pcm_ports;max_pcm_ports=std::max(max_pcm_ports.load(),ports);
    return 42;
}
extern "C" int sceAudioOutClose(int){--pcm_ports;return 0;}
extern "C" int sceAudioOutOutput(int h,const void* data) {
    if(h==42 && data) {const auto* s=static_cast<const int16_t*>(data);for(int i=0;i<512;++i)pcm_peak=std::max(pcm_peak.load(),std::abs(int(s[i])));}
    if(h==47)return data ? (fail_bitstream?-1:0) : (fail_drain?-1:0);
    std::this_thread::sleep_for(std::chrono::nanoseconds(5333333));return 0;
}
extern "C" int sceAudioOutExOpen(int,int){return 47;}
extern "C" int sceAudioOutExClose(int){return 0;}
extern "C" int sceAudioOutExConfigureOutput(int,unsigned,int,int,uint64_t){return 0;}
extern "C" int sceAudioOutSysGetHdmiMonitorInfo(int,void* ptr,unsigned n){std::memset(ptr,0,n);static_cast<uint8_t*>(ptr)[0x98]=2;return 0;}
#ifndef NUVIO_TEST_REAL_NETWORK
const std::string& http_ca_bundle(){static const std::string empty;return empty;}
const char* kUserAgent="NuvioTest";
#endif
std::string language_name(const std::string& s){return s;}
std::string language_to_iso639_2(const std::string& s){return s;}
#ifndef NUVIO_TEST_REAL_NETWORK
void NetStream::set_read_interrupt(AVIOContext*,const std::atomic<bool>*){}
void NetStream::install(AVFormatContext*,IoHooks*){}
NetStream* NetStream::of(AVIOContext*){return nullptr;}
AVIOContext* NetStream::open_avio(const std::string&,const std::vector<std::string>&,const std::atomic<bool>*,std::string*,bool,const std::atomic<bool>*){return nullptr;}
void NetStream::close_avio(AVIOContext**){}
std::string NetStream::peek(size_t){return "";}
std::string NetStream::failure(){return "";}
std::string NetStream::effective_url(){return "";}
#endif
AVIOContext* TorrentStream::open_avio(const std::string&,const std::atomic<bool>*,std::string*){return nullptr;}
bool TorrentStream::is_ours(AVIOContext*){return false;}
TorrentStream* TorrentStream::of(AVIOContext*){return nullptr;}
void TorrentStream::close_avio(AVIOContext**){}
std::string TorrentStream::peek(size_t){return "";}
std::unique_ptr<EvoDecoder> EvoDecoder::open(const AVCodecParameters*,std::string&){return nullptr;}
EvoDecoder::~EvoDecoder()=default;
int EvoDecoder::send(const uint8_t*,int,int64_t){return -1;}
int EvoDecoder::receive(pp_frame&){return -1;}
bool EvoDecoder::pack(const pp_frame&,VideoFrame&){return false;}
void EvoDecoder::flush(){}
bool EvoDecoder::hardware()const{return false;}
EvoPresenter::EvoPresenter()=default;
EvoPresenter::~EvoPresenter()=default;
void EvoPresenter::reset(){}
int EvoPresenter::publish(const VideoPicture&,double,double){return 0;}
struct HwDecoder::Impl { int w=0,h=0,sent=0; bool ready=false; int64_t pts=0; std::vector<uint8_t> pixels; };
void HwDecoder::load_module(){}
HwDecoder* HwDecoder::open(const AVCodecParameters* p,std::string* why) {
    ++hw_opens;
    if(!hardware){*why="host software test"; return nullptr;}
    auto* d=new HwDecoder; d->d_=new Impl; d->d_->w=p->width;d->d_->h=p->height;
    d->d_->pixels.assign(size_t(p->width)*p->height*3/2,128); return d;
}
HwDecoder::~HwDecoder(){delete d_;}
int HwDecoder::send(const uint8_t* data,int,int64_t pts) {
    if(!data) return 0;
    if(++d_->sent==12){failed_hardware=true;return -1;}
    d_->pts=pts;d_->ready=true;return 0;
}
int HwDecoder::receive(Picture* p) {
    if(!d_->ready)return 0;d_->ready=false;
    p->y=d_->pixels.data();p->uv=p->y+size_t(d_->w)*d_->h;p->pitch=d_->w;p->width=d_->w;p->height=d_->h;p->pts_us=d_->pts;return 1;
}
void HwDecoder::flush(){d_->ready=false;d_->sent=0;}
const char* HwDecoder::name()const{return "test hardware";}
void run_until(Player& p,const std::function<bool()>& ready,double timeout=8) {
    const double deadline=now_seconds()+timeout;
    while(now_seconds()<deadline) {
        p.present();
        if(p.state()==Player::State::Failed) throw std::runtime_error(p.error());
        if(ready())return;
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    throw std::runtime_error("player timed out: "+p.stats()+" diagnostics="+p.diagnostic_snapshot().dump());
}
int main(int argc,char** argv) try {
    require(argc>=3,"fixtures missing");
    Player p; Player::Options o;o.url=argv[1];o.surround=true;
    p.open(o); run_until(p,[&]{return p.position()>.25 && !p.buffering();});
    const auto subs=p.subtitle_tracks();
    require(subs.size()==1 && subs[0].lang=="eng" && subs[0].is_default && !subs[0].forced,"embedded track metadata/default disposition lost");
    require(p.audio_output().find("7.1")!=std::string::npos,"surround output missing");
    p.set_paused(true); std::this_thread::sleep_for(std::chrono::milliseconds(15));const double paused=p.position();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));require(std::abs(paused-p.position())<.006,"paused clock moved");
    p.seek(6);run_until(p,[&]{return !p.buffering();});require(p.position()>=6 && p.position()<6.03,"seek sample trim/gate failed");
    p.set_paused(false);run_until(p,[&]{return p.position()>6.15;});
    p.seek(2);run_until(p,[&]{return !p.buffering()&&p.position()>2.10 && p.position()<3;});p.close();
    std::cout<<"Playback lifecycle: open, pause, sample-trimmed forward/back seek passed\n";
    hardware=true;p.open(o);
    run_until(p,[&]{return failed_hardware && p.codec_summary().find("(software)")!=std::string::npos && !p.buffering() && p.position()>.8;});
    p.close();std::cout<<"Playback lifecycle: hardware failure reseek to software without an A/V wait cycle passed\n";
    hardware=false;o.url=argv[2];o.audio_mode="auto";o.surround=false;p.open(o);
    run_until(p,[&]{return !p.buffering() && p.position()>.25;});
    require(p.audio_output()=="stereo PCM 16-bit","Auto ignored stereo route and selected passthrough");
    p.close();std::cout<<"Playback lifecycle: Auto stereo policy avoids HDMI passthrough passed\n";
    o.surround=true;p.open(o);
    run_until(p,[&]{return !p.buffering() && p.position()>.25;});
    require(p.audio_output().find("Dolby Digital bitstream")!=std::string::npos,"AC3 did not use compressed output");
    p.set_paused(true);std::this_thread::sleep_for(std::chrono::milliseconds(15));const double bs_paused=p.position();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));require(std::abs(bs_paused-p.position())<.006,"bitstream pause moved the clock");
    p.seek(6);run_until(p,[&]{return !p.buffering();});require(p.position()>=6 && p.position()<6.10,"bitstream seek boundary failed");
    p.set_paused(false);p.seek(2);run_until(p,[&]{return !p.buffering() && p.position()>2.12;});
    fail_bitstream=true;
    run_until(p,[&]{return p.audio_output().find("PCM")!=std::string::npos && !p.buffering() && p.position()>2.35;});
    p.close();fail_bitstream=false;
    std::cout<<"Playback lifecycle: bitstream pause/seek and failed-port recovery to PCM passed\n";
#ifdef NUVIO_HAVE_VLC
    o.url=std::string("file://")+argv[1];o.vlc=true;o.surround=false;o.audio_mode="pcm";
    p.open(o);run_until(p,[&]{return p.state()==Player::State::Playing && p.position()>.3;},15);
    require(p.codec_summary().find("VLC")==0,"VLC selection did not dispatch to libVLC");
    p.close();o.vlc=false;o.url=argv[1];p.open(o);
    run_until(p,[&]{return !p.buffering()&&p.position()>.25;});
    require(p.codec_summary().find("VLC")!=0,"switching back left VLC active");p.close();
    std::cout<<"Playback lifecycle: Native -> VLC -> Native dispatch and port cleanup passed\n";
#endif
    o.url=argv[1];o.vlc=false;o.surround=false;o.audio_mode="pcm";
    p.open(o);run_until(p,[&]{return !p.buffering() && p.position()>.4;});
    pcm_peak=0;std::this_thread::sleep_for(std::chrono::milliseconds(180));const int normal=pcm_peak;
    require(p.set_audio_gain(200),"PCM boost rejected");pcm_peak=0;
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    require(normal>100 && pcm_peak>normal*1.7 && pcm_peak<normal*2.3,"PCM output gain did not double samples");
    const double clock=p.position();p.set_audio_delay(.5);
    run_until(p,[&]{return p.picture_pts()-p.position()>.25;});
    require(p.position()>=clock && p.picture_pts()-p.position()<.8,"sync offset altered progress or exceeded bounds");
    p.set_audio_delay(0);p.close();
    std::cout<<"PCM gain reaches real output boundary; video sync changes without resetting progress passed\n";
    for(int fixture=3;fixture<argc;++fixture) {
        Player limited;auto rejected=o;rejected.url=argv[fixture];rejected.preview=true;rejected.video_only=true;
        const int opens=hw_opens;limited.open(rejected);const double bound=now_seconds()+4;
        while(now_seconds()<bound && limited.state()!=Player::State::Failed)std::this_thread::sleep_for(std::chrono::milliseconds(3));
        require(limited.state()==Player::State::Failed && limited.error().find("1080p")!=std::string::npos,"preview did not reject oversized/HDR source");
        limited.close();require(hw_opens==opens && pcm_ports==0,"rejected preview acquired decoder/audio ownership");
    }
    std::array<std::unique_ptr<Player>,4> screens;
    const int before_hw=hw_opens;max_pcm_ports=0;
    for(int i=0;i<4;++i) {screens[i]=std::make_unique<Player>();auto preview=o;preview.preview=true;preview.live_tv=false;preview.video_only=i!=0;screens[i]->open(preview);}
    const double until=now_seconds()+12;bool ready=false;
    while(now_seconds()<until && !ready) {
        ready=true;for(auto& screen:screens) {
            if(const auto* picture=screen->present())require(picture->w<=640 && picture->h<=360,"preview exceeded bounded surface");require(screen->state()!=Player::State::Failed,"preview playback failed");
            if(screen->picture_serial()!=screen->video_serial() || screen->buffering())ready=false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    require(ready && hw_opens==before_hw && max_pcm_ports==1,"four previews did not play with software video and one PCM owner");
    require(screens[0]->subtitle_tracks().empty(),"preview unnecessarily decoded subtitles");
    screens[0]->close();auto muted=o;muted.preview=true;muted.live_tv=false;muted.video_only=true;screens[0]->open(muted);
    screens[1]->close();muted.video_only=false;screens[1]->open(muted);
    for(auto& screen:screens)screen->close();
    require(pcm_ports==0 && max_pcm_ports==1,"audio focus/stop leaked or duplicated PCM ports");
    std::cout<<"Four independent previews, software-only decoding, single audio owner, focus transfer and cleanup passed\n";
    o.url=argv[2];o.vlc=false;o.audio_mode="bitstream";o.surround=true;
    p.open(o);run_until(p,[&]{return !p.buffering()&&p.position()>.25;});
    fail_drain=true;fail_bitstream=true;
    const double deadline=now_seconds()+4;
    while(now_seconds()<deadline && p.state()!=Player::State::Failed) {p.present();std::this_thread::sleep_for(std::chrono::milliseconds(3));}
    require(p.state()==Player::State::Failed && p.error().find("safely")!=std::string::npos,"unsafe audio shutdown did not stop playback");
    p.close();
    std::cout<<"Playback lifecycle: failed audio drain stops without PCM retry loop passed\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
