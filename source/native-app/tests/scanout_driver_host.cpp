// Exercise the actual extension included in the PS5 driver's source.
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cassert>
#define VIDEO_OUT_PIXEL_FORMAT UINT64_C(0x8000000000000000)
#define DISPLAY_WIDTH 3840
#define DISPLAY_HEIGHT 2160
#define FRAMEBUFFER_BYTES 0x2000000u
#define FRAMEBUFFER_POOL_BYTES 0x4000000u
struct video_attribute_t { uint64_t bytes[10]; };
struct video_buffer_t { void* p;void* r[3]; };
static uint8_t pool[FRAMEBUFFER_POOL_BYTES];
static uint8_t* runtime_video_framebuffer=pool;
static size_t runtime_video_framebuffer_size=sizeof(pool);
static int runtime_video_registered=1,runtime_video_handle=42;
static int change_rc,unregister_rc,register_rc,restore_rc,pending,changes,unregisters,registers;
static uint64_t target_format,last_registered;
void set_attr(video_attribute_t* a,uint64_t format,uint32_t tiling,uint32_t w,uint32_t h,uint64_t,uint32_t,uint64_t) {
 assert(!tiling && w==DISPLAY_WIDTH && h==DISPLAY_HEIGHT);a->bytes[0]=format;
}
int reg(int h,int set,int start,video_buffer_t* b,int n,video_attribute_t* a,int,void*) {
 assert(h==42 && !set && !start && n==2 && b[0].p==pool && b[1].p==pool+FRAMEBUFFER_BYTES);
 ++registers;last_registered=a->bytes[0];return registers==1?register_rc:restore_rc;
}
int unreg(int h,int set){assert(h==42&&!set);++unregisters;return unregister_rc;}
int is_pending(int){return pending;}
struct { decltype(&set_attr) set_attribute2;decltype(&reg) register_buffers2;decltype(&unreg) unregister_buffers;decltype(&is_pending) is_flip_pending; }
runtime_video_api{set_attr,reg,unreg,is_pending};
int sceVideoOutSubmitChangeBufferAttribute2(int h,int set,const void* a,const void*) {
 assert(h==42&&!set);++changes;target_format=static_cast<const video_attribute_t*>(a)->bytes[0];return change_rc;
}
#include "scanout_driver_extension.inc"
void reset(){runtime_video_registered=1;runtime_scanout_format=VIDEO_OUT_PIXEL_FORMAT;change_rc=unregister_rc=register_rc=restore_rc=pending=changes=unregisters=registers=0;}
int main(){
 int32_t results[4];const uint64_t hdr=UINT64_C(0x8100070422000000);
 reset();assert(!ps5_opengl_set_scanout_format(hdr,results)&&changes==1&&!unregisters&&runtime_scanout_format==hdr);
 reset();change_rc=int(0x80290003u);assert(!ps5_opengl_set_scanout_format(hdr,results)&&registers==1&&runtime_video_registered&&last_registered==hdr);
 reset();change_rc=int(0x80290003u);unregister_rc=int(0x80290009u);assert(ps5_opengl_set_scanout_format(hdr,results)==change_rc&&!registers&&runtime_video_registered);
 reset();change_rc=int(0x80290003u);register_rc=-11;assert(ps5_opengl_set_scanout_format(hdr,results)==-11&&registers==2&&runtime_video_registered&&last_registered==VIDEO_OUT_PIXEL_FORMAT);
 reset();change_rc=int(0x80290003u);register_rc=-11;restore_rc=-12;assert(ps5_opengl_set_scanout_format(hdr,results)==-12&&!runtime_video_registered);
 reset();runtime_scanout_format=hdr;change_rc=int(0x80290003u);register_rc=-11;assert(ps5_opengl_set_scanout_format(VIDEO_OUT_PIXEL_FORMAT,results)==-11&&last_registered==hdr&&runtime_scanout_format==hdr);
 reset();pending=1;assert(ps5_opengl_set_scanout_format(hdr,results)==-1&&!changes&&!unregisters);
 reset();change_rc=-17;assert(ps5_opengl_set_scanout_format(hdr,results)==-17&&!unregisters);
 std::puts("Driver HDR transactions: 8 registration, busy and rollback cases passed");
}
