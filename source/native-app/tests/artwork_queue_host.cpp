#include "art.hpp"
#include "http.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

static std::string image;
static std::atomic<int> reads{0};
static int deleted_textures=0;
HttpResponse http_get(const std::string&,long,const std::atomic<bool>* cancel,const std::vector<std::string>&) {
    ++reads;return {cancel && cancel->load()?0:200,image,"","" ,0};
}
std::string HttpResponse::describe() const {return error;}
extern "C" void glDeleteTextures(GLsizei n,const GLuint*) {deleted_textures+=n;}
namespace sx {
struct TexCacheProbe {
    static void check() {
        TexCache cache;
        assert(cache.init("/tmp/maxd-artwork-queue-test",nullptr,2));
        for(int i=0;i<300;++i)cache.get("https://example.test/poster/"+std::to_string(i),ArtKind::poster);
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        bool full=false;
        while(std::chrono::steady_clock::now()<end) {
            {std::lock_guard<std::mutex> lock(cache.mutex_);
             assert(cache.queue_.size()<=artwork::kJobs && cache.ready_.size()<=artwork::kReadyCount && cache.ready_bytes_<=artwork::kReadyBytes);
             full=cache.ready_.size()==artwork::kReadyCount;}
            if(full)break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        assert(full);
        const int before=reads;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        assert(reads==before); // workers backpressure instead of decoding the rest
        cache.entries_["uploaded-test"].id=77;
        cache.entries_["uploaded-test"].bytes=4;cache.bytes_=4;
        const auto start=std::chrono::steady_clock::now();
        cache.shutdown(); // wakes blocked producers and releases their pixels
        assert(std::chrono::steady_clock::now()-start<std::chrono::seconds(1));
        assert(cache.ready_bytes_==0 && cache.ready_.empty() && cache.entries_.empty());
        assert(cache.bytes_==0 && deleted_textures==1);
        // Failed/evicted-request metadata must not grow forever either.
        for(size_t i=0;i<artwork::kEntries;++i)cache.entries_[std::to_string(i)]={};
        assert(cache.get("https://example.test/new",ArtKind::poster)==0 && cache.entries_.size()==artwork::kEntries);
        cache.clock_=61;cache.trim(1);assert(cache.entries_.empty());
    }
};
}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);image.assign(std::istreambuf_iterator<char>(f),{});
    sx::TexCacheProbe::check();std::filesystem::remove_all("/tmp/maxd-artwork-queue-test");
    std::cout<<"Production artwork workers: bounded request/decoded queues, backpressure and shutdown cancellation passed\n";
}
