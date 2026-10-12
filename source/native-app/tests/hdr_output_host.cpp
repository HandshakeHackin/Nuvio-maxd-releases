// Mock only the VideoOut boundary; exercise the production switch adapter.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hdr_output.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>

namespace {
int register_rc = 0, close_rc = 0, change_rc = 0, pending = 0, changes = 0, sleeps = 0;
int status_rc = 0, sleep_rc = 0;
bool bgr_only=false;
unsigned dynamic_range = 1;
std::uint64_t output_flags = 0, format = 0;
std::int64_t clock_us = 1000000;
int checks = 0;
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class F> void test(const char* name, F f) {
    f(); ++checks; std::printf("HDR output: %s passed\n",name);
}
}
extern "C" {
int __wrap_sceVideoOutRegisterBuffers2(int,int,int,void*,int,void*,int,void*);
int __wrap_sceVideoOutClose(int);
int __real_sceVideoOutRegisterBuffers2(int,int,int,void*,int,void*,int,void*) { return register_rc; }
int __real_sceVideoOutClose(int) { return close_rc; }
void sceVideoOutSetBufferAttribute2(void*,std::uint64_t f,unsigned t,unsigned w,unsigned h,
                                   std::uint64_t option,unsigned reserved,std::uint64_t other) {
    require(t==0&&w==3840&&h==2160&&!option&&!reserved&&!other,"wrong attribute layout");
    format=f;
}
int sceVideoOutSubmitChangeBufferAttribute2(int handle,int index,const void*,const void* option) {
    require(handle==42&&index==0&&!option,"borrowed registration was lost"); ++changes; return bgr_only && format==UINT64_C(0x8100070422000000) ? int(0x80290003u) : change_rc;
}
int sceVideoOutIsFlipPending(int) { return pending; }
int sceKernelUsleep(unsigned us) { ++sleeps; clock_us+=us; return sleep_rc; }
int sceVideoOutGetOutputStatus(int,void* ptr) {
    struct S { unsigned resolution,range; std::uint64_t refresh,flags,reserved[3]; };
    auto& s=*static_cast<S*>(ptr); s.range=dynamic_range; s.flags=output_flags; return status_rc;
}
int __wrap_clock_gettime(clockid_t,timespec* t) {
    t->tv_sec=clock_us/1000000; t->tv_nsec=clock_us%1000000*1000; return 0;
}
}
int main() try {
    using sx::HdrOutput;
    test("missing or failed registration cannot switch output",[] {
        require(!HdrOutput::set(true,3840,2160),"unregistered output switched");
        register_rc=-1;
        require(__wrap_sceVideoOutRegisterBuffers2(42,0,0,nullptr,2,nullptr,0,nullptr)==-1,"registration result changed");
        require(!HdrOutput::set(true,3840,2160)&&!HdrOutput::active(),"failed registration acquired ownership");
        register_rc=0;
        require(__wrap_sceVideoOutRegisterBuffers2(42,0,0,nullptr,2,nullptr,0,nullptr)==0,"register failed");
    });
    test("refused HDR leaves SDR layout intact",[] {
        change_rc=int(0x80290003u);
        require(!HdrOutput::set(true,3840,2160)&&!HdrOutput::active()&&!HdrOutput::confirmed(),"refusal changed state");
        require(format==UINT64_C(0x8100070400000000),"BGR alternative was not tried after RGB rejection");
        require(HdrOutput::result()==0x80290003u&&HdrOutput::error().find("pixel format (0x80290003)")!=std::string::npos,
                "invalid pixel format was hidden by a generic TV error");
        change_rc=0;
    });
    test("BGR-only outputs use the successful format and matching word order",[] {
        bgr_only=true;
        require(HdrOutput::set(true,3840,2160)&&HdrOutput::bgr(),"BGR candidate not selected");
        require(HdrOutput::format()==UINT64_C(0x8100070400000000),"accepted candidate lost");
        require(HdrOutput::set(false,3840,2160)&&!HdrOutput::bgr(),"BGR restore failed"); bgr_only=false;
    });
    test("busy or invalid flip queues cannot change the pixel layout",[] {
        const int before=changes; pending=1; sleeps=0;
        require(!HdrOutput::set(true,3840,2160)&&changes==before&&sleeps==500,"busy wait was unbounded or retyped buffers");
        pending=-1;
        require(!HdrOutput::set(true,3840,2160)&&changes==before,"failed pending query switched layout");
        require(HdrOutput::result()==0xffffffffu&&HdrOutput::error().find("flip queue query")!=std::string::npos,
                "pending query failure lost its result or stage");
        pending=0;
    });
    test("failed flip wait reports its native result without submitting an attribute",[] {
        const int before=changes; pending=1; sleep_rc=int(0x80020004u); sleeps=0;
        require(!HdrOutput::set(true,3840,2160)&&changes==before&&sleeps==1&&!HdrOutput::active(),
                "failed wait continued or switched the pixel layout");
        require(HdrOutput::result()==0x80020004u&&HdrOutput::error().find("flip queue wait (0x80020004)")!=std::string::npos,
                "wait failure lost its result or stage");
        sleep_rc=0; pending=0;
    });
    test("attribute acceptance requires a successful post-flip HDR readback",[] {
        require(HdrOutput::set(true,3840,2160)&&HdrOutput::active()&&!HdrOutput::confirmed(),"attribute was treated as HDMI confirmation");
        HdrOutput::presented(); require(!HdrOutput::confirmed(),"SDR status confirmed HDR");
        dynamic_range=2; output_flags=1; status_rc=-1;
        HdrOutput::presented(); require(!HdrOutput::confirmed(),"failed status query confirmed HDR");
        status_rc=0; output_flags=0; HdrOutput::presented(); require(HdrOutput::confirmed(),"HDR status not confirmed");
        const int before=changes;
        require(HdrOutput::set(true,3840,2160)&&changes==before,"repeated frame retyped the set");
    });
    test("refused SDR restore retains packed HDR presentation",[] {
        change_rc=-1;
        require(!HdrOutput::set(false,3840,2160)&&HdrOutput::active(),"failed restore mislabeled HDR bytes");
        change_rc=0;
        require(HdrOutput::set(false,3840,2160)&&!HdrOutput::active()&&!HdrOutput::confirmed(),"restore failed");
        require(format==UINT64_C(0x8000000000000000),"wrong SDR format");
    });
    test("missing HDR confirmation times out without silently tone mapping",[] {
        dynamic_range=1; output_flags=0;
        require(HdrOutput::set(true,3840,2160),"switch refused");
        clock_us+=5000001; HdrOutput::presented();
        require(HdrOutput::failed()&&HdrOutput::active()&&!HdrOutput::confirmed(),"timeout lost current layout");
        require(!HdrOutput::set(true,3840,2160),"failed output claimed ready");
        require(HdrOutput::set(false,3840,2160),"timeout could not restore SDR");
    });
    test("closing an unrelated or refused handle preserves ownership",[] {
        require(HdrOutput::set(true,3840,2160),"switch refused");
        require(__wrap_sceVideoOutClose(7)==0&&HdrOutput::active(),"unrelated close reset output");
        close_rc=-1;
        require(__wrap_sceVideoOutClose(42)==-1&&HdrOutput::active(),"refused close reset output");
        close_rc=0;
        require(__wrap_sceVideoOutClose(42)==0&&!HdrOutput::active(),"close failed to reset output");
        require(!HdrOutput::set(true,3840,2160),"closed handle reused");
    });
    std::printf("HDR output transitions: %d tests, 0 failures\n",checks); return 0;
} catch(const std::exception& e) { std::fprintf(stderr,"HDR output FAILED: %s\n",e.what()); return 1; }
