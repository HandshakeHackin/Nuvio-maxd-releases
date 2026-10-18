// Exercise the exact runtime extension with a mock VideoOut boundary.
#include <cstdint>
#include <cassert>
#include <cstdio>
#define VIDEO_OUT_PIXEL_FORMAT UINT64_C(0x8000000000000000)
#define DISPLAY_WIDTH 3840
#define DISPLAY_HEIGHT 2160
struct video_attribute_t { uint64_t words[10]; };
static int pending=0, changes=0, result=0;
static bool runtime_video_registered=true;
static int runtime_video_handle=42;
static unsigned char storage;
static void* runtime_video_framebuffer=&storage;
static int idle(int h) { assert(h==42); return pending; }
static void attribute(void* out,uint64_t fmt,unsigned,unsigned w,unsigned h,uint64_t,unsigned,uint64_t) {
    assert(w==3840&&h==2160); static_cast<video_attribute_t*>(out)->words[0]=fmt;
}
static struct { decltype(&attribute) set_attribute2=attribute; decltype(&idle) is_flip_pending=idle; } runtime_video_api;
extern "C" int sceVideoOutSubmitChangeBufferAttribute2(int h,int index,const void* attr,const void* option) {
    assert(h==42&&index==0&&attr&&!option); ++changes; return result;
}
#include "scanout_driver_extension.inc"
int main() {
    int32_t results[4]; const uint64_t hdr=UINT64_C(0x8100070400000000);
    result=int(0x80290003u);
    assert(ps5_opengl_set_scanout_format(hdr,results)==result);
    assert(changes==1&&runtime_video_registered&&runtime_scanout_format==VIDEO_OUT_PIXEL_FORMAT);
    // No unregister/re-register entry points are provided: a refused set survives.
    assert(results[1]==INT32_MAX&&results[2]==INT32_MAX&&results[3]==INT32_MAX);
    pending=1; assert(ps5_opengl_set_scanout_format(hdr,results)==-1&&changes==1);
    pending=0; result=0; assert(ps5_opengl_set_scanout_format(hdr,results)==0);
    assert(runtime_scanout_format==hdr&&runtime_video_registered);
    assert(ps5_opengl_set_scanout_format(VIDEO_OUT_PIXEL_FORMAT,results)==0);
    assert(runtime_scanout_format==VIDEO_OUT_PIXEL_FORMAT);
    std::puts("Scanout driver: in-place HDR/SDR, refusal preserves registration, busy queue passed");
}
