#include "console_lights.hpp"
#include "util.h"
#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
using State=std::array<uint8_t,130>;
using Levels=std::array<uint8_t,3>;
// Public protocol reviewed in PS5-Suite/payload/main.c (ICC INDICATOR LED).
constexpr unsigned long kGet=0x40829505ul, kSet=0x80829504ul;
constexpr uint8_t ids[3]={0x01,0x11,0x21}; // blue, white, amber
bool layout(const State& state,std::array<int,3>& offsets) {
    for(int c=0;c<3;++c) {
        offsets[c]=-1;
        for(int i=2;i+5<int(state.size());++i) if(state[i]==ids[c] && state[i+1]==1) {
            if(offsets[c]!=-1) return false; // ambiguous blob: never guess
            offsets[c]=i;
        }
        if(offsets[c]<0) return false;
        for(int other=0;other<c;++other) if(std::abs(offsets[c]-offsets[other])<6) return false;
    }
    return true;
}
bool read(int fd,State& state,std::array<int,3>& offsets) {
    return ioctl(fd,kGet,state.data())==0 && layout(state,offsets);
}
Levels levels(const State& state,const std::array<int,3>& offsets) {
    return {state[offsets[0]+3],state[offsets[1]+3],state[offsets[2]+3]};
}
int write(int fd,State& state,const std::array<int,3>& offsets,const Levels& value) {
    for(int c=0;c<3;++c) state[offsets[c]+3]=value[c];
    return ioctl(fd,kSet,state.data());
}
}
namespace sx {
ConsoleLights::ConsoleLights() {
    try { worker_=std::thread([this]{run();}); }
    catch(...) { status_=Status::unavailable; dlog("console lights: worker unavailable; leaving system lighting unchanged"); }
}
ConsoleLights::~ConsoleLights() { stop(); }
void ConsoleLights::stop() {
    { std::lock_guard<std::mutex> lock(mutex_); quit_=true; }
    wake_.notify_one(); if(worker_.joinable()) worker_.join();
}
void ConsoleLights::update(bool enabled,uint8_t r,uint8_t g,uint8_t b) {
    // Extract white, then approximate the remaining cool/warm components.
    const uint8_t white=std::min({r,g,b});
    const Levels wanted={uint8_t(b-white),white,uint8_t(r-white)};
    { std::lock_guard<std::mutex> lock(mutex_);
      const bool active=enabled && (wanted[0] || wanted[1] || wanted[2]);
      if(active!=enabled_) ++generation_;
      enabled_=active; requested_=wanted; }
    wake_.notify_one();
}
void ConsoleLights::run() {
    int fd=-1; bool owned=false, blocked=false;
    Levels saved{},last{};
    const auto release=[&] {
        if(fd>=0 && owned) {
            State current{}; std::array<int,3> offsets{};
            // Restore only if the system has not already taken ownership.
            if(read(fd,current,offsets) && levels(current,offsets)==last) {
                if(write(fd,current,offsets,saved)!=0) dlog("console lights: previous state restore failed");
            }
        }
        if(fd>=0) close(fd);
        fd=-1; owned=false;
    };
    uint64_t observed_generation=0;
    std::unique_lock<std::mutex> lock(mutex_);
    for(;;) {
        const bool quit=quit_,enabled=enabled_; const auto wanted=requested_;
        const auto generation=generation_;
        lock.unlock();
        if(observed_generation!=generation) { release(); blocked=false; observed_generation=generation; }
        if(quit || !enabled) {
            release(); blocked=false; status_=Status::idle;
        } else if(!blocked) {
            if(fd<0) {
                fd=open("/dev/icc_indicator",O_RDWR);
                State state{}; std::array<int,3> offsets{};
                if(fd<0 || !read(fd,state,offsets)) {
                    release(); blocked=true; status_=Status::unavailable;
                    dlog("console lights: control unavailable; leaving system lighting unchanged");
                } else {
                    saved=levels(state,offsets);
                    if(saved==Levels{}) { release(); blocked=true; status_=Status::unavailable; }
                }
            }
            if(fd>=0) {
                State current{}; std::array<int,3> offsets{};
                if(!read(fd,current,offsets)) {
                    release(); blocked=true; status_=Status::unavailable;
                } else if(owned && levels(current,offsets)!=last) {
                    owned=false; release(); blocked=true; status_=Status::system_owned;
                    dlog("console lights: system reclaimed lighting; sync suspended");
                } else if(!owned || wanted!=last) {
                    // Restore only changes still matching our last request.
                    last=wanted; owned=true;
                    if(write(fd,current,offsets,wanted)!=0) {
                        release(); blocked=true; status_=Status::unavailable;
                        dlog("console lights: control refused; sync suspended");
                    } else status_=Status::active;
                }
            }
        }
        lock.lock();
        if(quit) break;
        // Color updates coalesce at 5 Hz. Disable/exit wakes immediately.
        wake_.wait_for(lock,std::chrono::milliseconds(200),[&]{return quit_ || generation_!=generation;});
    }
}
}
