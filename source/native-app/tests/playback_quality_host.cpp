#include "pcm_out.h"
#include "packet_storage.h"
#include "subtitle_renderer.h"
#include "subtitles.h"
#include <functional>
#include <stdexcept>
void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
template<class F> void test(const char* name,F f) { f(); std::printf("Playback quality: %s passed\n",name); }
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstring>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <fstream>
static std::atomic<int> mode{0},format_seen{-1},submitted{0};
static std::atomic<bool> nonzero{false};
extern "C" int sceAudioOutInit(){return 0;}
extern "C" int sceAudioOutOpen(int,int,int,unsigned frames,unsigned rate,unsigned format) {
    require(frames==256 && rate==48000,"audio grain/rate changed");
    format_seen=format; return mode==1 && format!=1 ? -1 : 42;
}
extern "C" int sceAudioOutOutput(int,const void* data) {
    if(data) { const auto* p=static_cast<const unsigned char*>(data); for(int i=0;i<256*4;++i) if(p[i]) nonzero=true; ++submitted; }
    std::this_thread::sleep_for(std::chrono::milliseconds(5)); return 0;
}
extern "C" int sceAudioOutClose(int){return 0;}
static void wait_for(const std::function<bool()>& f) {
    for(int i=0;i<200 && !f();++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    require(f(),"audio worker did not advance");
}
int main(int argc,char** argv) try {
    require(argc==2,"font dir missing");
    test("surround negotiates S16 PCM, consumes media samples, and holds during pause",[]{
        PcmOut out; require(out.open(true),"audio open failed");
        require(out.channels()==8 && !out.floating_point() && format_seen==2,"S16 surround not selected");
        std::vector<int16_t> pcm(2048*8,8192); require(out.queue(pcm.data(),pcm.size()*2),"queue rejected PCM");
        require(out.played_frames()==0,"paused audio clock advanced"); out.pause(false);
        wait_for([&]{return out.played_frames()>=1024;}); out.pause(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); auto held=out.played_frames();
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); require(out.played_frames()==held,"pause clock advanced");
        out.clear(); require(out.played_frames()==0 && out.queued_bytes()==0,"seek did not reset PCM epoch");
        require(nonzero,"audio submitted silence instead of PCM"); out.close();
    });
    test("refused surround/float output falls back to stereo S16",[]{
        mode=1; PcmOut out; require(out.open(true),"fallback failed");
        require(out.channels()==2 && !out.floating_point() && format_seen==1,"fallback format incorrect"); out.close(); mode=0;
    });
    test("fixed PCM ring wraps and rejects overflow without discarding samples",[]{
        PcmOut out; require(out.open(false),"stereo open failed");
        std::vector<uint8_t> pcm(PcmOut::kRate*out.bytes_per_frame()*2,7);
        require(out.queue(pcm.data(),pcm.size()),"capacity refused"); require(!out.queue(pcm.data(),8),"overflow grew ring");
        out.pause(false); wait_for([&]{return out.played_frames()>=1024;}); out.pause(true);
        require(out.queue(pcm.data(),512*out.bytes_per_frame()),"wrapped ring refused available space"); out.close();
    });
    test("packet slab preserves codec references across flush and destruction",[]{
        AVPacket* p=av_packet_alloc(); av_new_packet(p,800); memset(p->data,0x4a,800); p->pts=123; p->flags=AV_PKT_FLAG_KEY;
        AVPacket* held=av_packet_alloc();
        {
            PacketStorage pool(1024); require(pool.store(p),"packet not pooled");
            require(p->pts==123 && p->flags==AV_PKT_FLAG_KEY,"packet metadata changed");
            for(int i=0;i<AV_INPUT_BUFFER_PADDING_SIZE;++i) require(p->data[800+i]==0,"decoder padding missing");
            av_packet_ref(held,p); av_packet_unref(p); av_new_packet(p,800); memset(p->data,1,800);
            require(!pool.store(p),"live codec reference overwritten"); require(held->data[0]==0x4a,"live packet changed");
        }
        require(held->data[799]==0x4a,"slab freed before codec reference"); av_packet_free(&held); av_packet_free(&p);
    });
    test("packet slab reuses coalesced free blocks and preserves fallback data",[]{
        PacketStorage pool(4096);
        AVPacket *a=av_packet_alloc(),*b=av_packet_alloc(); av_new_packet(a,1000); av_new_packet(b,1000);
        require(pool.store(a)&&pool.store(b),"initial slab packets refused"); av_packet_unref(a); av_packet_unref(b);
        av_new_packet(a,3000); require(pool.store(a),"released regions were not coalesced"); av_packet_free(&a); av_packet_free(&b);
    });
    test("automatic subtitles wait for embedded metadata and preserve manual choices",[]{
        std::vector<SubtitleCandidate> tracks={{false,"eng",false,false,false},{true,"eng",true,false,false}};
        require(default_subtitle(tracks,{"eng"},false,false)==-1,"addon won before file metadata");
        require(default_subtitle(tracks,{"eng"},true,false)==1,"matching embedded track did not win");
        require(default_subtitle(tracks,{"eng"},true,true)==-1,"manual choice or Off was overwritten");
    });
    test("embedded source priority respects preferred languages and addon fallback",[]{
        std::vector<SubtitleCandidate> tracks={{false,"eng",false,false,false},{true,"ell",false,false,false}};
        require(default_subtitle(tracks,{"eng","ell"},true,false)==1,"embedded preferred-language fallback lost to addon");
        require(default_subtitle(tracks,{"eng"},true,false)==0,"matching addon fallback ignored");
        require(default_subtitle(tracks,{"fra"},true,false)==-1,"random foreign subtitle selected");
        require(default_subtitle(tracks,{},true,false)==-1,"no-language preference selected a random track");
    });
    test("embedded caption roles prefer full default tracks and handle untagged defaults",[]{
        std::vector<SubtitleCandidate> tracks={{true,"eng",true,true,false},{true,"eng",false,false,false},{true,"eng",true,false,false}};
        require(default_subtitle(tracks,{"eng"},true,false)==2,"full default caption track not selected");
        tracks={{true,"spa",true,false,false},{true,"und",true,false,false}};
        require(default_subtitle(tracks,{"eng"},true,false)==1,"file-declared untagged default ignored");
        tracks.push_back({false,"eng",false,false,false});
        require(default_subtitle(tracks,{"eng"},true,false)==1,"declared file default lost to addon");
        tracks.pop_back();
        tracks[1].is_default=false;
        require(default_subtitle(tracks,{"eng"},true,false)==-1,"untagged non-default track guessed");
    });
    test("styled ASS preserves positioned events and external cues survive a seek",[&]{
        SubtitleRenderer renderer; renderer.configure(argv[1]);
        const std::string ass="[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\nStyle: Default,Inter,40,&H000000FF,&H00FFFFFF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,2,0,2,10,10,10,1\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\nDialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,{\\pos(400,200)}Styled text\n";
        require(renderer.external(-2,ass,"eng"),"ASS failed to load");
        auto o=renderer.render(-2,2,3840,2160,1,0); require(o.supported && o.w>0 && o.y<250,"ASS positioning lost");
        renderer.seek(); o=renderer.render(-2,2,3840,2160,1,0); require(o.w>0,"seek erased external track");
        renderer.clear(); require(renderer.external(-2,ass,"eng"),"cached external track did not restore after input reopen");
        o=renderer.render(-2,2,3840,2160,1,0); require(o.w>0 && o.y<250,"external reopen lost styling/position");
        o=renderer.render(-2,4,3840,2160,1,0); require(o.changed && o.rgba.empty(),"expired ASS remained visible");
    });
    test("Arabic, CJK and Thai subtitles render with bundled font shaping",[&]{
        SubtitleRenderer renderer; renderer.configure(argv[1]);
        const char* texts[]={"مرحبا بالعالم","日本語 中文 한국어","ภาษาไทย"}; const char* langs[]={"ara","jpn","tha"};
        for(int i=0;i<3;++i) {
            const std::string srt="1\n00:00:01,000 --> 00:00:03,000\n"+std::string(texts[i])+"\n";
            require(renderer.external(-2-i,srt,langs[i]),"language track failed");
            auto o=renderer.render(-2-i,2,1920,1080,1,0); require(o.w>50 && o.h>10,"language subtitle empty");
        }
    });
    test("PGS bitmap fits video and clear packets end the display",[&]{
        SubtitleRenderer renderer; renderer.configure(argv[1]);
        auto* par=avcodec_parameters_alloc(); par->codec_id=AV_CODEC_ID_HDMV_PGS_SUBTITLE; par->width=3840; par->height=2160;
        renderer.add_track(3,par,"eng"); avcodec_parameters_free(&par);
        uint8_t indexes[4]={1,1,1,1}; uint32_t palette[2]={0,0xffff8000};
        AVSubtitleRect rect{}; rect.type=SUBTITLE_BITMAP; rect.x=1800; rect.y=1800; rect.w=2;rect.h=2;rect.linesize[0]=2;rect.data[0]=indexes;rect.data[1]=reinterpret_cast<uint8_t*>(palette);rect.nb_colors=2;
        AVSubtitleRect* rects[]={&rect}; AVSubtitle sub{}; sub.num_rects=1;sub.rects=rects;
        AVPacket p{}; renderer.packet(3,&p,1,0,&sub); auto o=renderer.render(3,1.5,3840,2160,1,0);
        require(o.supported && o.x==900 && o.y==900 && o.rgba.size()==4,"bitmap scaling incorrect");
        require(o.rgba[0]==255 && o.rgba[1]==128 && o.rgba[3]==255,"PGS palette/alpha lost");
        AVSubtitle clear{}; renderer.packet(3,&p,2,0,&clear); o=renderer.render(3,2.1,3840,2160,1,0);
        require(o.changed && o.rgba.empty(),"PGS clear ignored");
    });
    return 0;
} catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
