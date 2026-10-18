#include "presentation_timing.hpp"
#include "tube_frame_pacing.h"
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <ctime>
#include <cstdio>

static int64_t mono=2000000;
static uint64_t process=1500000, status[16]{};
static int status_rc=0, submit_rc=0;
static bool encode_ok=true;
static int calls=0;
extern "C" int __wrap_clock_gettime(clockid_t clock,timespec* out) {
    assert(clock==CLOCK_MONOTONIC); out->tv_sec=mono/1000000; out->tv_nsec=mono%1000000*1000; return 0;
}
extern "C" uint64_t sceKernelGetProcessTime() { return process; }
extern "C" int sceVideoOutGetFlipStatus(int32_t handle,void* out) {
    assert(handle==7); for(int i=0;i<16;++i) static_cast<uint64_t*>(out)[i]=status[i]; return status_rc;
}
extern "C" uint32_t* __real_sceAgcDcbSetFlip(void* cmd,uint32_t h,int index,uint32_t mode,int64_t marker) {
    assert(cmd==reinterpret_cast<void*>(1234) && h==7 && index==1 && mode==1 && marker>0); ++calls;
    return encode_ok ? reinterpret_cast<uint32_t*>(5678) : nullptr;
}
extern "C" int __real_sceVideoOutSubmitFlip(int32_t h,int32_t index,uint32_t mode,int64_t marker) {
    assert(h==7 && index==1 && mode==1 && marker>0); ++calls; return submit_rc;
}
extern "C" int sceKernelUsleep(unsigned us) { mono+=us; process+=us; return 0; }
void dlog(const char*,...) {}
extern "C" uint32_t* __wrap_sceAgcDcbSetFlip(void*,uint32_t,int,uint32_t,int64_t);
extern "C" int __wrap_sceVideoOutSubmitFlip(int32_t,int32_t,uint32_t,int64_t);
static void near(double a,double b) { assert(std::abs(a-b)<0.000001); }
int main() {
    using sx::PresentationTiming;
    assert(sx::flip_monotonic_us(1490000,1500000,2000000)==1990000);
    assert(!sx::flip_monotonic_us(0,1500000,2000000));
    assert(!sx::flip_monotonic_us(1600000,1500000,2000000));
    assert(!sx::flip_monotonic_us(1,1500000,2000000));
    assert(!sx::flip_monotonic_us(1,100,50));
    for (const int64_t delay : {8333,16667,60000}) {
        PresentationTiming t;
        for(uint64_t i=1;i<=3;++i) {
            t.submit(int64_t(i),{i,1000000+int64_t(i)*42000});
            assert(t.complete(int64_t(i),1000000+int64_t(i)*42000+delay,1300000));
        }
        near(t.lead_seconds(1300000),std::min(delay/1e6,0.04));
        assert(!t.complete(3,1126000+delay,1300000));
        t.submit(99,{3,1200000}); // same picture redrawn
        assert(!t.complete(99,1210000,1300000));
        assert(t.samples()==3);
        near(t.lead_seconds(2400000),0);
        t.reset(); // paused, buffering, seek or new movie
        assert(!t.complete(2,1100000,1300000));
        near(t.lead_seconds(1300000),0);
    }
    PresentationTiming t;
    t.submit(1,{1,1000000});
    assert(!t.complete(1,900000,1100000)); // impossible negative latency
    t.submit(2,{2,1000000});
    assert(!t.complete(2,1200000,1300000)); // hitch, not a steady scanout estimate
    t.submit(3,{3,1000000});
    assert(!t.complete(3,1100000,1000000)); // future
    assert(!t.complete(3,1100000,2200000)); // stale
    assert(!t.samples());
    for(uint64_t i=4;i<=70;++i) t.submit(int64_t(i),{i,1000000});
    assert(!t.complete(4,1010000,1020000)); // overwritten pending entry

    using namespace sx::presentation;
    reset();
    for(uint64_t i=1;i<=3;++i) {
        arm({i,mono-20000});
        if(i%2) assert(__wrap_sceAgcDcbSetFlip(reinterpret_cast<void*>(1234),7,1,1,int64_t(i))==reinterpret_cast<uint32_t*>(5678));
        else assert(__wrap_sceVideoOutSubmitFlip(7,1,1,int64_t(i))==0);
        disarm(); status[1]=process-10000; status[3]=i;
        near(poll(),i==3?0.01:0);
        mono+=42000;process+=42000;
    }
    near(poll(),0.01); // repeat status cannot skew average
    status_rc=-1;near(poll(),0);status_rc=0;
    reset();near(poll(),0);
    // Failures pass through unchanged and are never recorded.
    arm({10,mono-20000}); encode_ok=false; submit_rc=-42;
    assert(!__wrap_sceAgcDcbSetFlip(reinterpret_cast<void*>(1234),7,1,1,10));
    assert(__wrap_sceVideoOutSubmitFlip(7,1,1,10)==-42);
    status[3]=10;near(poll(),0);
    assert(calls==5);
    // The production scheduler waits once for a new low-rate picture at 120 Hz.
    reset();
    arm({1,mono,0,24,1},120); assert(needs_wait());
    const auto before=mono; wait_before_swap();
    assert(mono>before && mono-before<=25025); assert(!needs_wait());
    arm({1,before,0,24,1},120); assert(!needs_wait()); // repeated picture
    arm({2,mono,41667,24,1},60); assert(!needs_wait());
    arm({3,mono,83333,60,1},120); assert(!needs_wait()); // high-rate path unchanged
    arm({4,mono,125000,24,2},120); assert(needs_wait()); // new seek epoch
    reset(); assert(!needs_wait());
    // Exact source timestamps, jitter, discontinuities and late GPU completion.
    tube_frame_pacing p{};
    assert(tube_frame_pacing_begin(&p,1,1,0,1000000,1,24,1001000.0/120));
    assert(tube_frame_pacing_wait_until(&p,1001000)>1001000);
    tube_frame_pacing_observe(&p,1025025,1); assert(!p.pending);
    assert(tube_frame_pacing_begin(&p,1,2,41667,1041667,1,24,1001000.0/120));
    assert(!tube_frame_pacing_wait_until(&p,1100000)); assert(p.misses==1);
    tube_frame_pacing_observe(&p,1100000,0); assert(!p.active);
    assert(!tube_frame_pacing_begin(&p,1,0,0,1000000,1,24,8333));
    assert(!tube_frame_pacing_begin(&p,1,1,INT64_MIN,1000000,1,24,8333));

    std::puts("Presentation timing: GPU/CPU flips, timestamp guards, duplicate/stale rejection, reset and bounded lead passed");
}
