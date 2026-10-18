// Exercise production torrent recovery without contacting any swarm.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/torrent/engine.cpp"
#include "vlc_cancel.h"
#include <cassert>
#include <future>
#include <iostream>
int main() {
    const long first=nuvio_vlc_stream_begin();
    assert(!nuvio_vlc_stream_cancelled(first));
    nuvio_vlc_stream_end(first); assert(nuvio_vlc_stream_cancelled(first));
    const long second=nuvio_vlc_stream_begin();
    assert(second!=first && !nuvio_vlc_stream_cancelled(second));
    nuvio_vlc_stream_end(first); assert(!nuvio_vlc_stream_cancelled(second));
    assert(!nuvio_vlc_stream_cancelled(0)); nuvio_vlc_stream_end(second);
    using namespace bt;
    auto t=std::make_shared<Torrent>();
    t->has_meta=true; t->npieces=2; t->plen=16384; t->total=32768;
    t->have={0,1}; t->ever={0,1}; t->readers[1]=0;
    t->files.push_back({"fixture.mp4",32768,0});
    t->active[0].data="preserved partial piece";
    auto peer=std::make_unique<Peer>(); peer->state=Peer::Active; peer->since=1; peer->key=1;
    t->peers.push_back(std::move(peer));
    recover_stalled_torrent(t.get(),100); assert(!t->peers[0]->dead); // no blocked reader
    std::atomic<bool> cancel{false}; t->waiting_readers[1]={10,0,&cancel};
    recover_stalled_torrent(t.get(),39); assert(!t->peers[0]->dead);
    t->last_payload=35; recover_stalled_torrent(t.get(),50); assert(!t->peers[0]->dead);
    cancel=true; recover_stalled_torrent(t.get(),80); assert(!t->peers[0]->dead);
    cancel=false; t->idle=true; recover_stalled_torrent(t.get(),80); assert(!t->peers[0]->dead);
    t->idle=false; recover_stalled_torrent(t.get(),80);
    assert(t->peers[0]->dead && t->recovery_attempts==1 && t->next_recovery==110);
    assert(t->have[1] && t->active[0].data=="preserved partial piece" && t->readers.count(1));
    recover_stalled_torrent(t.get(),100); assert(t->recovery_attempts==1);
    recover_stalled_torrent(t.get(),110); assert(t->recovery_attempts==2 && t->next_recovery==170);
    t->have[0]=1; recover_stalled_torrent(t.get(),300); assert(t->recovery_attempts==2);
    t->waiting_readers.clear(); t->have[0]=0; cancel=false;
    uint8_t byte;
    auto read=std::async(std::launch::async,[&]{return Engine::get().read(t,1,0,0,&byte,1,&cancel);});
    bool registered=false;
    for (int i=0;i<100 && !registered;++i) {
        {std::lock_guard<std::mutex> lock(impl().mu);registered=t->waiting_readers.count(1);}
        if (!registered) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(registered); cancel=true;
    assert(read.get()==-1);
    assert(t->waiting_readers.empty());
    Torrent metadata;
    metadata.waiting_readers[-1]={1,-1,&cancel}; metadata.meta_asked={5,6};
    cancel=false; metadata.metadata_progress_at=30;
    recover_stalled_torrent(&metadata,50); assert(metadata.recovery_attempts==0);
    recover_stalled_torrent(&metadata,61);
    assert(metadata.recovery_attempts==1 && metadata.meta_asked[0]==0);
    std::cout<<"Playback recovery passed: cancellation generations, blocked-reader gating, useful-data timing, backoff, cache retention and aborted-read cleanup\n";
}
