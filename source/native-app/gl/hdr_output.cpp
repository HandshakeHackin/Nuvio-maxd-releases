// Switching is owned by the experimental extension inside the OpenGL runtime.
// HDR uses ProsperoLight's hardware-tested RGB10A2 BT.2100 PQ scanout format.
// Buffer registration and GPU rendering stay owned by the pinned OpenGL SDK.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hdr_output.hpp"
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <string>

extern "C" {
int __real_sceVideoOutRegisterBuffers2(int, int, int, void*, int, void*, int, void*);
int __real_sceVideoOutClose(int);
void sceVideoOutSetBufferAttribute2(void*, std::uint64_t, unsigned, unsigned, unsigned,
                                   std::uint64_t, unsigned, std::uint64_t);
int sceVideoOutSubmitChangeBufferAttribute2(int, int, const void*, const void*);
int ps5_opengl_set_scanout_format(std::uint64_t, std::int32_t*);
int sceVideoOutIsFlipPending(int);
int sceVideoOutGetOutputStatus(int, void*);
int sceKernelUsleep(unsigned);
}

namespace {
constexpr std::uint64_t kSdr = UINT64_C(0x8000000000000000);
constexpr std::uint64_t kHdrBgr = UINT64_C(0x8100070400000000);
constexpr std::uint64_t kHdr = UINT64_C(0x8100070422000000); // Rgb10A2Bt2100Pq
struct alignas(8) Attribute { std::uint8_t bytes[80]; };
// SharpProspero/EVO's 48-byte VideoOut status ABI. No sys/privileged calls.
struct OutputStatus {
    std::uint32_t resolution, dynamic_range;
    std::uint64_t refresh, flags, reserved[3];
};
static_assert(sizeof(OutputStatus) == 48 && offsetof(OutputStatus, flags) == 16);
int handle = -1, set_index = -1;
bool hdr_bgr=false;
bool switching_driver=false;
std::int32_t driver_results[4]{};
std::uint64_t attempted_format=0;
OutputStatus readback{};
int readback_rc=0;
bool hdr = false, hdr_confirmed = false, hdr_failed = false;
std::uint32_t last_result = 0;
const char* failure_stage = "none";
std::int64_t requested_at = 0;
int pending_frames = 0;
std::int64_t now_us() {
    timespec t{}; clock_gettime(CLOCK_MONOTONIC, &t);
    return std::int64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}
bool fail(const char* stage, std::uint32_t result) {
    failure_stage = stage; last_result = result;
    std::printf("[HDR] failed stage=%s rc=%08x\n",stage,unsigned(result));
    return false;
}
}

extern "C" int __wrap_sceVideoOutRegisterBuffers2(int h, int index, int start, void* buffers,
                                                   int count, void* attr, int category, void* option) {
    const int rc = __real_sceVideoOutRegisterBuffers2(h,index,start,buffers,count,attr,category,option);
    if (rc == 0 && !switching_driver) {
        handle = h; set_index = index; hdr = hdr_confirmed = hdr_failed = false;
        hdr_bgr=false; attempted_format=0;
        pending_frames = 0; last_result = 0; failure_stage = "none";
    }
    return rc;
}
extern "C" int __wrap_sceVideoOutClose(int h) {
    const int rc = __real_sceVideoOutClose(h);
    if (rc == 0 && h == handle) {
        handle = set_index = -1; hdr = hdr_confirmed = hdr_failed = false;
        hdr_bgr=false; attempted_format=0;
        pending_frames = 0; last_result = 0; failure_stage = "none";
    }
    return rc;
}

namespace sx {
bool HdrOutput::set(bool enable, int width, int height) {
    if (enable == hdr && handle >= 0) return !hdr_failed;
    if (handle < 0 || set_index < 0 || width <= 0 || height <= 0) {
        return fail("buffer registration",UINT32_C(0xffffffff));
    }
    // Drain the flip queue too: a completed GL batch can still be waiting for
    // VideoOut. A busy display leaves the current format untouched.
    int pending = 0;
    for (int i = 0; i < 500; ++i) {
        pending = sceVideoOutIsFlipPending(handle);
        if (pending <= 0) break;
        const int rc = sceKernelUsleep(1000);
        if (rc != 0) return fail("flip queue wait",std::uint32_t(rc));
    }
    if (pending < 0) return fail("flip queue query",std::uint32_t(pending));
    if (pending > 0) return fail("flip queue drain",UINT32_C(0xfffffffe));
    // Probe only a rejected format. An accepted attribute owns the scanout
    // until it is restored, even while HDMI readback is pending.
    readback={}; readback_rc=sceVideoOutGetOutputStatus(handle,&readback);
    std::printf("[HDR] before switch status_rc=%08x range=%u refresh=%llu flags=%016llx\n",
        unsigned(readback_rc),readback.dynamic_range,(unsigned long long)readback.refresh,(unsigned long long)readback.flags);
    int rc=0;
    const std::uint64_t candidates[]={enable?kHdr:kSdr,kHdrBgr};
    for(int candidate=0;candidate<(enable?2:1);++candidate) {
        attempted_format=candidates[candidate];
        switching_driver=true;
        rc=ps5_opengl_set_scanout_format(attempted_format,driver_results);
        switching_driver=false;
        std::printf("[HDR] driver change=%08x unregister=%08x register=%08x restore=%08x\n",
            unsigned(driver_results[0]),unsigned(driver_results[1]),unsigned(driver_results[2]),unsigned(driver_results[3]));
        last_result=std::uint32_t(rc);
        std::printf("[HDR] attribute %s render=%dx%d format=%016llx rc=%08x handle=%08x set=%d candidate=%d\n",
            enable?"HDR10":"SDR",width,height,(unsigned long long)attempted_format,unsigned(rc),unsigned(handle),set_index,candidate);
        if(rc==0) { hdr_bgr=enable && attempted_format==kHdrBgr; break; }
        if(last_result!=UINT32_C(0x80290003)) break;
    }
    if(rc!=0) return fail("buffer attribute",std::uint32_t(rc));
    failure_stage = "none";
    hdr = enable; hdr_confirmed = hdr_failed = false; pending_frames = 0;
    requested_at = now_us();
    return true;
}
void HdrOutput::presented() {
    if (!hdr || hdr_confirmed || hdr_failed || handle < 0) return;
    OutputStatus status{};
    const int rc = sceVideoOutGetOutputStatus(handle, &status);
    readback=status; readback_rc=rc;
    ++pending_frames;
    hdr_confirmed = rc == 0 && status.dynamic_range == 2;
    if (pending_frames == 1 || hdr_confirmed)
        std::printf("[HDR] after flip rc=%08x range=%u flags=%016llx confirmed=%d\n",
                    unsigned(rc), status.dynamic_range,
                    static_cast<unsigned long long>(status.flags), hdr_confirmed);
    if (!hdr_confirmed && now_us() - requested_at > 5000000) {
        hdr_failed = true;
        fail("HDR confirmation",rc ? std::uint32_t(rc) : UINT32_C(0xfffffffd));
        std::printf("[HDR] output did not confirm HDR within 5 seconds; stop playback and restore SDR\n");
    }
}
bool HdrOutput::bgr() { return hdr_bgr; }
std::uint64_t HdrOutput::format() { return attempted_format; }
std::string HdrOutput::status() {
    char s[256]; std::snprintf(s,sizeof(s),"stage=%s format=%016llx accepted=%d confirmed=%d result=%08x status_rc=%08x range=%u refresh=%llu flags=%016llx",
        failure_stage,(unsigned long long)attempted_format,hdr,hdr_confirmed,unsigned(last_result),unsigned(readback_rc),readback.dynamic_range,
        (unsigned long long)readback.refresh,(unsigned long long)readback.flags); return s;
}
bool HdrOutput::active() { return hdr; }
bool HdrOutput::confirmed() { return hdr_confirmed; }
bool HdrOutput::failed() { return hdr_failed; }
std::uint32_t HdrOutput::result() { return last_result; }
std::string HdrOutput::error() {
    char message[256];
    if (last_result == UINT32_C(0x80290003) && std::string(failure_stage) == "buffer attribute")
        std::snprintf(message,sizeof(message),"The PS5 rejected the HDR10 pixel format (0x%08x). RGB and BGR HDR10 formats were refused. Use Convert HDR to SDR for this console/output.",unsigned(last_result));
    else
        std::snprintf(message,sizeof(message),"HDR10 output failed at %s (0x%08x). Check the PS5 HDR setting and TV input.",failure_stage,unsigned(last_result));
    return message;
}
}
