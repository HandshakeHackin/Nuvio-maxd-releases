// SPDX-License-Identifier: GPL-3.0-or-later
#include "packet_storage.h"
extern "C" {
#include <libavcodec/packet.h>
#include <libavcodec/defs.h>
#include <libavutil/buffer.h>
}
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <vector>
struct PacketStorage::State {
    explicit State(size_t n):capacity(n) {}
    size_t capacity,live=0;
    std::vector<uint8_t> backing;
    uint8_t* base=nullptr;
    std::map<size_t,size_t> free;
    std::mutex mutex;
    std::atomic<uint64_t> pooled{0},fallbacks{0};
    struct Lease { std::shared_ptr<State> state; size_t start,length; };
    static void release(void* opaque,uint8_t*) {
        std::unique_ptr<Lease> lease(static_cast<Lease*>(opaque));
        auto& s=*lease->state;
        std::lock_guard<std::mutex> lock(s.mutex);
        size_t start=lease->start,n=lease->length;
        s.live-=n;
        auto next=s.free.lower_bound(start);
        if(next!=s.free.end() && start+n==next->first) { n+=next->second; s.free.erase(next); }
        auto after=s.free.lower_bound(start);
        if(after!=s.free.begin()) {
            auto before=std::prev(after);
            if(before->first+before->second==start) { start=before->first; n+=before->second; s.free.erase(before); }
        }
        s.free[start]=n;
    }
};
PacketStorage::PacketStorage(size_t capacity):state_(std::make_shared<State>(capacity)) {}
PacketStorage::~PacketStorage()=default;
bool PacketStorage::store(AVPacket* packet) {
    if(!packet || packet->size<=0 || !packet->data) return false;
    auto& s=*state_;
    const size_t n=(size_t(packet->size)+AV_INPUT_BUFFER_PADDING_SIZE+63)&~size_t(63);
    size_t start=0;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if(!s.base) {
            try { s.backing.resize(s.capacity+63); } catch(const std::bad_alloc&) { ++s.fallbacks; return false; }
            s.base=reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(s.backing.data())+63)&~uintptr_t(63));
            s.free[0]=s.capacity;
        }
        auto found=s.free.end();
        for(auto it=s.free.begin();it!=s.free.end();++it) if(it->second>=n) { found=it; break; }
        if(found==s.free.end()) { ++s.fallbacks; return false; }
        start=found->first; const size_t remaining=found->second-n;
        s.free.erase(found); if(remaining) s.free[start+n]=remaining; s.live+=n;
    }
    auto* lease=new State::Lease{state_,start,n};
    auto* data=s.base+start;
    AVBufferRef* buffer=av_buffer_create(data,n,State::release,lease,0);
    if(!buffer) { State::release(lease,nullptr); ++s.fallbacks; return false; }
    std::memcpy(data,packet->data,packet->size);
    std::memset(data+packet->size,0,n-size_t(packet->size));
    // Keep timestamps, flags and side data on the same AVPacket. Only payload ownership changes.
    av_buffer_unref(&packet->buf); packet->buf=buffer; packet->data=data; ++s.pooled; return true;
}
uint64_t PacketStorage::pooled() const { return state_->pooled; }
uint64_t PacketStorage::fallbacks() const { return state_->fallbacks; }
size_t PacketStorage::live_bytes() const { std::lock_guard<std::mutex> lock(state_->mutex); return state_->live; }
