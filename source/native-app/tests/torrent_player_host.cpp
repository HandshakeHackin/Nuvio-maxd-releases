// Exercise production Player/FFmpeg through a deterministic file-backed torrent AVIO.
// The real torrent engine's waits, cancellation and deadlines have separate tests.
#define NUVIO_TEST_REAL_NETWORK 1
#define NUVIO_TEST_REAL_TORRENT 1
#define main legacy_lifecycle_main
#include "player_lifecycle_host.cpp"
#undef main
extern "C" {
#include <libavformat/avformat.h>
}
extern "C" int console_curl_nonblocking(int){return 0;}
static std::vector<std::string> media;
static std::atomic<int> opens{0}, refused{0};
static std::atomic<bool> stall_seek{false}, stall_read{false};
static int torrent_read(void* p,uint8_t* b,int n){return static_cast<TorrentStream*>(p)->read(b,n);}
static int64_t torrent_seek(void* p,int64_t o,int w){return static_cast<TorrentStream*>(p)->seek(o,w);}
AVIOContext* TorrentStream::open_avio(const std::string& url,const std::atomic<bool>* abort,std::string*){
    auto* ts=new TorrentStream();ts->file_=std::stoi(url.substr(url.find_last_of('/')+1));
    ts->size_=media.at(ts->file_).size();ts->abort_=abort;
    auto* pb=avio_alloc_context(static_cast<uint8_t*>(av_malloc(32768)),32768,0,ts,torrent_read,nullptr,torrent_seek);
    pb->seekable=AVIO_SEEKABLE_NORMAL;++opens;return pb;
}
bool TorrentStream::is_ours(AVIOContext* pb){return pb && pb->read_packet==torrent_read;}
TorrentStream* TorrentStream::of(AVIOContext* pb){return is_ours(pb)?static_cast<TorrentStream*>(pb->opaque):nullptr;}
void TorrentStream::close_avio(AVIOContext** pb){if(!pb || !*pb)return;delete of(*pb);av_freep(&(*pb)->buffer);avio_context_free(pb);}
std::string TorrentStream::peek(size_t n){return media.at(file_).substr(0,n);}
int TorrentStream::read(uint8_t* buf,int n){
    if(stall_read.exchange(false)){
        while(read_deadline_<=0 || now_seconds()<read_deadline_){
            if((abort_ && *abort_) || (read_interrupt_ && *read_interrupt_))return AVERROR_EXIT;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return AVERROR(ETIMEDOUT);
    }
    if(pos_>=size_)return AVERROR_EOF;
    const int count=static_cast<int>(std::min<int64_t>(n,size_-pos_));
    std::memcpy(buf,media.at(file_).data()+pos_,count);pos_+=count;return count;
}
int64_t TorrentStream::seek(int64_t o,int w){
    if(w&AVSEEK_SIZE)return size_;w&=~AVSEEK_FORCE;
    const auto next=w==SEEK_SET?o:w==SEEK_CUR?pos_+o:w==SEEK_END?size_+o:-1;
    if(next<0 || next>size_)return AVERROR(EINVAL);pos_=next;return next;
}
extern "C" int __real_avformat_seek_file(AVFormatContext*,int,int64_t,int64_t,int64_t,int);
extern "C" int __real_av_seek_frame(AVFormatContext*,int,int64_t,int);
extern "C" int __wrap_avformat_seek_file(AVFormatContext* f,int s,int64_t lo,int64_t t,int64_t hi,int flags){
    if(stall_seek.exchange(false)){
        stall_read=true;uint8_t byte;return TorrentStream::of(f->pb)->read(&byte,1);
    }
    if(refused>0){--refused;avio_seek(f->pb,avio_size(f->pb)/2,SEEK_SET);return AVERROR(EIO);}
    return __real_avformat_seek_file(f,s,lo,t,hi,flags);
}
extern "C" int __wrap_av_seek_frame(AVFormatContext* f,int s,int64_t t,int flags){
    if(refused>0){--refused;avio_seek(f->pb,avio_size(f->pb)/2,SEEK_SET);return AVERROR(EIO);}
    return __real_av_seek_frame(f,s,t,flags);
}
static void wait_failed(Player& p){
    const double deadline=now_seconds()+10;
    while(now_seconds()<deadline && p.state()!=Player::State::Failed){p.present();std::this_thread::sleep_for(std::chrono::milliseconds(3));}
    require(p.state()==Player::State::Failed,"failure was hidden or recovery looped");
}
int main(int argc,char** argv)try{
    require(argc==7,"expected MKV, TS, M2TS, front/tail MP4 and dual-audio fixtures");http_init("");
    for(int i=1;i<argc;++i){std::string bytes;require(read_file(argv[i],bytes),"missing media fixture");media.push_back(std::move(bytes));}
    for(int i=0;i<5;++i){
        for(double start:{0.0,3.0}){
            Player p;Player::Options o;o.url="torrent://"+std::string(40,'a')+"/"+std::to_string(i);o.start=start;
            p.open(o);run_until(p,[&]{return !p.buffering() && p.position()>start+.15;},15);
            p.seek(5);run_until(p,[&]{return !p.buffering() && p.position()>5.1;},10);p.close();
        }
    }
    std::cout<<"Torrent player: MKV/TS/M2TS/front- and tail-moov MP4 startup, resume and seek passed\n";
    Player p;Player::Options o;o.url="torrent://"+std::string(40,'a')+"/0";
    p.open(o);run_until(p,[&]{return !p.buffering() && p.position()>.2;});
    const auto opened=opens.load();refused=2;p.seek(7);
    run_until(p,[&]{return p.take_seek_rejected();},10);
    require(opens==opened && p.position()<3,"failed seek reopened or adopted unreachable target");
    run_until(p,[&]{return !p.buffering() && p.position()>.4;});
    stall_seek=true;refused=1;const auto before=now_seconds();p.seek(7);
    run_until(p,[&]{return p.take_seek_rejected();},12);
    require(now_seconds()-before<8,"torrent seek index I/O was unbounded");
    refused=4;p.seek(7);wait_failed(p);require(p.error().find("recover safely")!=std::string::npos,"unsafe seek error missing");p.close();
    refused=2;o.start=3;p.open(o);wait_failed(p);
    require(p.error().find("Play from beginning")!=std::string::npos,"startup resume failure hidden");p.close();
    refused=2;o.start=0;hardware=true;p.open(o);wait_failed(p);
    require(p.error().find("Decoder recovery")!=std::string::npos,"critical decoder failure swallowed");p.close();hardware=false;
    o.url="torrent://"+std::string(40,'a')+"/5";p.open(o);
    run_until(p,[&]{return !p.buffering() && p.position()>.2;});auto tracks=p.audio_tracks();
    require(tracks.size()==2,"dual-audio fixture missing");refused=2;p.select_audio(tracks.back().stream);wait_failed(p);
    require(p.error().find("resynchronize")!=std::string::npos,"critical audio failure swallowed");p.close();
    require(pcm_ports==0,"audio ports leaked");
    std::cout<<"Torrent player: seek rollback/deadline, startup failure, decoder/audio errors and no reopen loops passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
