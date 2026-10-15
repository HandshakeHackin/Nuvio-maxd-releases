// Exercise the real pad lifecycle without PS5 services.
#include "../third_party/hui/src/platform/ps5/pad.cpp"
#include "logo_animation.hpp"
#include <cassert>
#include <cstdio>
static std::int64_t clock_us;
static int calls, resets, errors, result, generation=1;
static bool connected=true;
namespace hui::sys {
std::int64_t monotonic_us() { return clock_us; }
void sleep_us(std::uint32_t) {}
void log(const char*,...) { ++errors; }
}
extern "C" {
int sceUserServiceInitialize(const void*) { return 0; }
int sceUserServiceTerminate() { return 0; }
int sceUserServiceGetInitialUser(int* u) { *u=1; return 0; }
int scePadInit() { return 0; }
int scePadOpen(int,int,int,const void*) { return 1; }
int scePadRead(int,RawPadSample* p,int) { *p={}; p->connected=connected; p->connected_count=generation; return 1; }
int scePadClose(int) { return 0; }
int scePadSetVibrationMode(int,int) { return 0; }
int scePadSetVibration(int,const PadVibration*) { return 0; }
int scePadSetLightBar(int,const PadColor*) { ++calls; return result; }
int scePadResetLightBar(int) { ++resets; return 0; }
}
int main() {
    hui::ps5::Pad pad; assert(pad.open()); errors=0;
    pad.set_light_bar(10,20,30); assert(calls==1);
    for(int i=0;i<120;++i) pad.set_light_bar(10,20,30);
    assert(calls==1);
    pad.set_light_bar(20,30,40); assert(calls==1);
    clock_us=33333; pad.set_light_bar(20,30,40); assert(calls==2);
    clock_us=66666; result=-1; pad.set_light_bar(30,40,50);
    for(int i=0;i<120;++i) {clock_us+=33333;pad.set_light_bar(i,40,50);}
    assert(calls==3 && errors==1);
    hui::PadSample sample; pad.read(std::span(&sample,1)); result=0;
    pad.set_light_bar(30,40,50); assert(calls==4);
    connected=false; pad.read(std::span(&sample,1));
    connected=true; ++generation;pad.read(std::span(&sample,1));
    pad.set_light_bar(30,40,50); assert(calls==5);
    pad.close(); assert(resets==1); pad.set_light_bar(1,2,3); assert(calls==5);
    auto dark=sx::logo_animation(0,0); assert(dark.light.r==0&&dark.light.g==0&&dark.light.b==0);
    auto a=sx::logo_animation(0,1),b=sx::logo_animation(18,1);
    assert(a.light.r==0 && a.light.g==74 && a.light.b==89);
    assert(a.light.r==b.light.r&&a.light.g==b.light.g&&a.light.b==b.light.b);
    const auto white=sx::logo_animation(6,1).light, purple=sx::logo_animation(12,1).light;
    assert(white.r==89 && white.g==89 && white.b==89);
    assert(purple.r==51 && purple.g==15 && purple.b==89);
    // No abrupt jumps at the color stops, loop boundary or normal frame cadence.
    for(int i=1;i<=18000;++i) {
        const auto prev=sx::logo_animation((i-1)/1000.0f,1).light;
        const auto next=sx::logo_animation(i/1000.0f,1).light;
        assert(std::abs(int(next.r)-prev.r)<=1 && std::abs(int(next.g)-prev.g)<=1);
    }
    hui::tween::reduced_motion=true; a=sx::logo_animation(2,0); b=sx::logo_animation(20,0);
    assert(a.light.r==b.light.r&&a.light.g==b.light.g&&a.glow==b.glow);
    a=sx::logo_animation(2,1); b=sx::logo_animation(20,1);
    assert(a.light.r==0 && a.light.g==b.light.g && a.light.b==b.light.b);
    std::puts("Controller lightbar: throttling, failure suppression, reconnect, reset and logo/reduced-motion envelopes passed; slow teal-white-purple loop and continuity passed");
}
