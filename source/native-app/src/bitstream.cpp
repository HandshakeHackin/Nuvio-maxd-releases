#include "bitstream.h"
#include "util.h"
#include <atomic>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#if (defined(PLATFORM_PS5) || defined(NUVIO_HOST_BITSTREAM_API)) && defined(PLATFORM_PS5_NATIVE)
int sceAudioOutInit(void);
int sceAudioOutExOpen(int,int);
int sceAudioOutExClose(int);
int sceAudioOutExConfigureOutput(int,unsigned,int,int,uint64_t);
int sceAudioOutSysGetHdmiMonitorInfo(int,void*,unsigned);
#endif
}
namespace bitstream {
static std::atomic<bool> pcm_normal{true};
int kind(const AVCodecParameters* p) {
    if(!p || p->sample_rate!=48000) return None;
    if(p->codec_id==AV_CODEC_ID_AC3) return Ac3;
    if(p->codec_id==AV_CODEC_ID_EAC3) return Eac3;
    if(p->codec_id==AV_CODEC_ID_DTS && p->profile>=0 && p->profile<50) return Dts;
    return None; // Preserve lossless TrueHD/DTS-HD as PCM.
}
const char* name(int k) {return k==Ac3?"Dolby Digital":k==Eac3?"Dolby Digital Plus":k==Dts?"DTS core":"PCM";}
int rate(int k) {return k==Eac3?192000:48000;}
bool receiver_supports(const uint8_t* p,size_t n,int k) {
    if(!p || k<Ac3 || k>Dts) return false;
    const unsigned format=k==Ac3?2:k==Eac3?10:7;
    for(size_t i=0x98;i+8<=n && i<0xf0;i+=8) {if(!p[i]) break;if(p[i]==format)return true;}
    return false;
}
int open_port(int k,bool check) {
#if (defined(PLATFORM_PS5) || defined(NUVIO_HOST_BITSTREAM_API)) && defined(PLATFORM_PS5_NATIVE)
    if(k<Ac3 || k>Dts)return -1;
    const int init=sceAudioOutInit();if(init && uint32_t(init)!=0x8026000eu)return -1;
    alignas(16) uint8_t info[0x180]{};
    const int probe=sceAudioOutSysGetHdmiMonitorInfo(1,info,sizeof(info));
    const bool supported=probe==0 && receiver_supports(info,sizeof(info),k);
    dlog("audio: HDMI %s supported=%d probe=0x%08x",name(k),supported,unsigned(probe));
    if(check && !supported)return -1;
    const int mode=k==Ac3?0:k==Dts?2:3;
    const int h=sceAudioOutExOpen(0xff,mode);
    if(h<0)return h;
    const int rc=sceAudioOutExConfigureOutput(0,0,mode,1,0);
    if(rc) {sceAudioOutExClose(h);pcm_normal=sceAudioOutExConfigureOutput(0,0,0xff,0xff,0)==0;return -1;}
    pcm_normal=false;
    return h;
#else
    (void)k;(void)check;return -1;
#endif
}
bool close_port(int h) {
#if (defined(PLATFORM_PS5) || defined(NUVIO_HOST_BITSTREAM_API)) && defined(PLATFORM_PS5_NATIVE)
    if(h>=0)sceAudioOutExClose(h);
    const int rc=sceAudioOutExConfigureOutput(0,0,0xff,0xff,0);
    pcm_normal=rc==0;
    dlog("audio: restored PCM output rc=0x%08x",unsigned(rc));
    return pcm_normal;
#else
    (void)h;return true;
#endif
}
bool ensure_pcm() {
#if (defined(PLATFORM_PS5) || defined(NUVIO_HOST_BITSTREAM_API)) && defined(PLATFORM_PS5_NATIVE)
    if(!pcm_normal)pcm_normal=sceAudioOutExConfigureOutput(0,0,0xff,0xff,0)==0;
#endif
    return pcm_normal;
}
Packer::~Packer(){clear();}
void Packer::clear() {
    if(ctx_) {ctx_->pb=nullptr;avformat_free_context(ctx_);ctx_=nullptr;}
    if(io_) {av_freep(&io_->buffer);avio_context_free(&io_);}
    bytes_.clear();
}
int Packer::write(void* self,const uint8_t* p,int n) {
    auto& b=static_cast<Packer*>(self)->bytes_;
    if(n<0 || size_t(n)>65536-b.size())return AVERROR(ENOSPC);
    try {b.insert(b.end(),p,p+n);}catch(...){return AVERROR(ENOMEM);}
    return n;
}
bool Packer::start(const AVCodecParameters* p) {
    clear();if(kind(p)==None)return false;
    if(avformat_alloc_output_context2(&ctx_,nullptr,"spdif",nullptr)<0 || !ctx_)return false;
    auto* st=avformat_new_stream(ctx_,nullptr);
    if(!st || avcodec_parameters_copy(st->codecpar,p)<0){clear();return false;}
    auto* b=static_cast<unsigned char*>(av_malloc(32768));
    if(!b){clear();return false;}
    io_=avio_alloc_context(b,32768,1,this,nullptr,&Packer::write,nullptr);
    if(!io_){av_free(b);clear();return false;}
    ctx_->pb=io_;ctx_->flags|=AVFMT_FLAG_CUSTOM_IO;
    if(avformat_write_header(ctx_,nullptr)<0){clear();return false;}return true;
}
bool Packer::pack(const AVPacket* p,std::vector<uint8_t>& out) {
    out.clear();bytes_.clear();if(!ctx_ || !p)return false;
    AVPacket copy=*p;copy.stream_index=0;
    const int rc=av_write_frame(ctx_,&copy);avio_flush(io_);
    if(rc<0 || io_->error || bytes_.size()%4)return false;
    out=std::move(bytes_);return true;
}
}
