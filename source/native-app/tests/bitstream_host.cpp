#include "bitstream.h"
#include "pcm_out.h"
#include <cassert>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <cstdarg>
#include <sys/wait.h>
#include <unistd.h>
extern "C" {
#include <libavformat/avformat.h>
}
static int probe_rc,open_rc=47,configure_rc,opens,closes,restores;
static std::atomic<int> output_rc{0},drain_rc{0};
static int close_rc=0,ex_close_rc=0;
static const void* last_buffer=nullptr;
static std::atomic<unsigned> buffer_switches{0};
static uint8_t monitor[0x180]{};
extern "C" {
int sceAudioOutInit(){return 0;}
int sceAudioOutOpen(int,int,int,unsigned,unsigned,unsigned){return 50;}
int sceAudioOutClose(int){return close_rc;}
int sceAudioOutOutput(int,const void* data){
 if(data) {if(last_buffer){assert(last_buffer!=data);++buffer_switches;}last_buffer=data;}
 else last_buffer=nullptr;
 return data ? output_rc.load() : drain_rc.load();
}
int sceAudioOutExOpen(int user,int mode){assert(user==0xff&&mode>=0&&mode<=3);++opens;return open_rc;}
int sceAudioOutExClose(int h){assert(h==47);++closes;return ex_close_rc;}
int sceAudioOutExConfigureOutput(int zero,unsigned flags,int mode,int target,uint64_t opts) {
 assert(!zero&&!flags&&!opts);if(mode==0xff){assert(target==0xff);++restores;return 0;}assert(target==1);return configure_rc;
}
int sceAudioOutSysGetHdmiMonitorInfo(int target,void* p,unsigned n){assert(target==1&&n==sizeof(monitor));memcpy(p,monitor,n);return probe_rc;}
}
void dlog(const char*,...){ }
extern double now_seconds(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void fixture(const char* path,int expected){
 AVFormatContext* input=nullptr;assert(avformat_open_input(&input,path,nullptr,nullptr)==0);assert(avformat_find_stream_info(input,nullptr)>=0);
 auto* params=input->streams[0]->codecpar;assert(bitstream::kind(params)==expected);
 bitstream::Packer pack;assert(pack.start(params));auto* pkt=av_packet_alloc();int bursts=0;std::vector<uint8_t> bytes;
 while(av_read_frame(input,pkt)>=0){assert(pack.pack(pkt,bytes));if(!bytes.empty()) {
  assert(bytes.size()==(expected==bitstream::Ac3?6144u:24576u));assert(bytes[0]==0x72&&bytes[1]==0xf8&&bytes[2]==0x1f&&bytes[3]==0x4e);++bursts;
 }av_packet_unref(pkt);}
 assert(bursts>=2);av_packet_free(&pkt);avformat_close_input(&input);
}
int main(int argc,char** argv){
 assert(argc==3);fixture(argv[1],bitstream::Ac3);fixture(argv[2],bitstream::Eac3);
 AVCodecParameters* p=avcodec_parameters_alloc();p->sample_rate=48000;p->codec_id=AV_CODEC_ID_TRUEHD;assert(!bitstream::kind(p));
 p->codec_id=AV_CODEC_ID_DTS;p->profile=60;assert(!bitstream::kind(p));p->profile=20;p->sample_rate=44100;assert(!bitstream::kind(p));avcodec_parameters_free(&p);
 assert(!bitstream::receiver_supports(monitor,0x98,bitstream::Ac3));
 assert(bitstream::open_port(bitstream::Ac3,true)<0&&!opens);
 monitor[0x98]=10;assert(bitstream::receiver_supports(monitor,sizeof(monitor),bitstream::Eac3));
 configure_rc=-1;assert(bitstream::open_port(bitstream::Eac3,true)<0&&closes==1&&restores==1);configure_rc=0;
 probe_rc=-1;assert(bitstream::open_port(bitstream::Eac3,true)<0);assert(bitstream::open_port(bitstream::Eac3,false)==47);bitstream::close_port(47);probe_rc=0;
 PcmOut out;assert(out.open_bitstream(bitstream::Eac3,true));std::vector<uint8_t> burst(24576,0);assert(out.queue(burst.data(),burst.size()));out.pause(false);
 std::this_thread::sleep_for(std::chrono::milliseconds(10));assert(out.played_seconds()>=0&&out.played_seconds()<0.020);
 for(int i=0;i<100 && out.played_seconds()<0.030;++i) {std::this_thread::sleep_for(std::chrono::milliseconds(5));}
 assert(out.played_seconds()>0.030&&out.played_seconds()<0.034);
 out.pause(true);out.clear();std::this_thread::sleep_for(std::chrono::milliseconds(8));assert(out.played_seconds()==0);
 output_rc=-1;for(int i=0;i<100 && !out.failed();++i) {std::this_thread::sleep_for(std::chrono::milliseconds(5));}
 assert(out.failed());output_rc=0;out.close();
 assert(buffer_switches>1);
 assert(!out.queue(burst.data(),burst.size()));
 assert(!AudioPort::player_wants);
 // Isolate process-wide unsafe-shutdown latches, just as restarting the app does.
 for(int scenario=0;scenario<3;++scenario) {
  const pid_t child=fork();assert(child>=0);
  if(child==0) {
   PcmOut pcm;assert(pcm.open(false));
   std::this_thread::sleep_for(std::chrono::milliseconds(8));
   if(scenario==0)close_rc=-1;
   if(scenario==1)drain_rc=-1;
   if(scenario==2){ex_close_rc=-1;assert(!bitstream::close_port(47));assert(!bitstream::ensure_pcm());}
   pcm.close();
   if(scenario<2) {assert(AudioPort::shutdown_failed && AudioPort::player_wants);assert(!pcm.open(false));assert(!pcm.open_bitstream(bitstream::Ac3,false));}
   else {assert(!pcm.open(false));assert(bitstream::open_port(bitstream::Ac3,false)<0);}
   _exit(0);
  }
  int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
 }
 std::puts("Bitstream audio: framing, capabilities, lossless fallback, pacing, seek epoch and port recovery passed");
}
