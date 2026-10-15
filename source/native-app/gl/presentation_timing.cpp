#include "presentation_timing.hpp"
#include "util.h"
#include <ctime>

extern "C" {
uint32_t* __real_sceAgcDcbSetFlip(void*,uint32_t,int,uint32_t,int64_t);
int __real_sceVideoOutSubmitFlip(int32_t,int32_t,uint32_t,int64_t);
int sceVideoOutGetFlipStatus(int32_t,void*);
uint64_t sceKernelGetProcessTime();
}
namespace {
sx::PresentationTiming timing;
sx::PresentationTiming::Picture armed;
int32_t output_handle=-1;
int64_t logged_at=0;
int64_t monotonic_us() {
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC,&ts)!=0) return 0;
    return int64_t(ts.tv_sec)*1000000 + ts.tv_nsec/1000;
}
void submitted(int32_t handle,int64_t marker) {
    if (!armed.id) return;
    if (output_handle!=handle) { timing.reset(); output_handle=handle; }
    timing.submit(marker,armed);
}
}
extern "C" uint32_t* __wrap_sceAgcDcbSetFlip(void* command,uint32_t handle,int index,uint32_t mode,int64_t marker) {
    auto* result=__real_sceAgcDcbSetFlip(command,handle,index,mode,marker);
    if (result) submitted(int32_t(handle),marker);
    return result;
}
extern "C" int __wrap_sceVideoOutSubmitFlip(int32_t handle,int32_t index,uint32_t mode,int64_t marker) {
    const int result=__real_sceVideoOutSubmitFlip(handle,index,mode,marker);
    if (result>=0) submitted(handle,marker);
    return result;
}
namespace sx::presentation {
void reset() { timing.reset(); armed={}; output_handle=-1; logged_at=0; }
void arm(PresentationTiming::Picture picture) { armed=picture; }
void disarm() { armed={}; }
double poll() {
    const int64_t now=monotonic_us();
    if (output_handle<0 || !now) return 0;
    alignas(8) uint64_t status[16]{};
    if (sceVideoOutGetFlipStatus(output_handle,status)!=0) return 0;
    const uint64_t process_now=sceKernelGetProcessTime();
    const int64_t sampled_at=monotonic_us();
    const int64_t flip=flip_monotonic_us(status[1],process_now,sampled_at);
    if (timing.complete(int64_t(status[3]),flip,sampled_at) && sampled_at-logged_at>=30000000) {
        dlog("player: scanout timing selection-to-flip %.2f ms, video lead %.2f ms, samples %llu (console estimate; excludes TV/audio receiver)",
             timing.last_delay_us()/1000.0,timing.lead_seconds(sampled_at)*1000.0,
             static_cast<unsigned long long>(timing.samples()));
        logged_at=sampled_at;
    }
    return timing.lead_seconds(sampled_at);
}
}
