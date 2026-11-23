// Exercise the production stream reader and worker scheduler over local HTTP.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "netstream.h"
#include "http_credentials.hpp"
#include "http.h"
#include "util.h"
#include <curl/curl.h>
#include <atomic>
#include <iostream>
#include <thread>
#include <chrono>
#include <dlfcn.h>
#include <pthread.h>
#include <cerrno>
#include <stdexcept>

static std::atomic<int> thread_budget{-1};
extern "C" int pthread_create(pthread_t* thread,const pthread_attr_t* attr,void* (*start)(void*),void* arg) noexcept {
    static auto real=reinterpret_cast<decltype(&pthread_create)>(dlsym(RTLD_NEXT,"pthread_create"));
    int left=thread_budget.load();
    while(left>=0) {
        if(!left)return EAGAIN;
        if(thread_budget.compare_exchange_weak(left,left-1))break;
    }
    return real?real(thread,attr,start,arg):EAGAIN;
}

static std::atomic<int> faults{0};
static bool shared_dns=false;
static std::atomic<bool> seed_dns{true};
static std::atomic<bool> rapid{false}, release_request{false};
static std::atomic<bool> hold_next_init{false}, request_waiting{false};
extern "C" CURL* __real_curl_easy_init();
extern "C" CURL* __wrap_curl_easy_init() {
    // Newer static libcurl also creates private admin handles during the first
    // perform. Arm after the first read, so only the requested reconnect pauses.
    if(hold_next_init.exchange(false)) {
        request_waiting=true;
        while(!release_request)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return __real_curl_easy_init();
}
extern "C" CURLcode __real_curl_easy_perform(CURL*);
extern "C" CURLcode __wrap_curl_easy_perform(CURL* c) {
    char* url = nullptr; curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &url);
    if (url && std::string(url).find("/dns-single") != std::string::npos) {
        ++faults; return CURLE_COULDNT_RESOLVE_HOST;
    }
    if (url && std::string(url).find("/dns-parallel") != std::string::npos && ++faults > 1)
        return CURLE_COULDNT_RESOLVE_HOST;
    curl_slist* seed=nullptr;
    if(shared_dns) {
        curl_easy_setopt(c,CURLOPT_PROXY,"");
        if(seed_dns.exchange(false)) {
            CURLU* parsed=curl_url();char* port=nullptr;
            curl_url_set(parsed,CURLUPART_URL,url,0);curl_url_get(parsed,CURLUPART_PORT,&port,0);
            if(!port)throw std::runtime_error("DNS fixture port missing");
            for(const char* host:{"stream.fixture.invalid","subs.fixture.invalid"})
                seed=curl_slist_append(seed,("+"+std::string(host)+":"+port+":127.0.0.1").c_str());
            curl_free(port);curl_url_cleanup(parsed);
            curl_easy_setopt(c,CURLOPT_RESOLVE,seed);
        }
    }
    const auto result=__real_curl_easy_perform(c);
    if(seed){curl_easy_setopt(c,CURLOPT_RESOLVE,nullptr);curl_slist_free_all(seed);}
    return result;
}
int main(int argc, char** argv) try {
    if (argc != 4) return 2;
    http_init(""); NetStream::set_cache_dir(std::string(argv[2]) == "seek" ? "" : argv[3]);
    std::atomic<bool> cancel{false};
    const std::string test_mode=argv[2];shared_dns=test_mode=="shared-dns";
    if(test_mode=="thread-no-reader")thread_budget=0;
    if(test_mode=="thread-no-workers")thread_budget=1;
    if(test_mode=="thread-two-workers")thread_budget=3;
    std::vector<std::string> headers;
    if(std::string(argv[2]).starts_with("media-"))headers={"X-Emby-Authorization: MediaBrowser Token=\"fixture-only\"", "X-Maxd-Credential-Origin: "+(std::string(argv[2])=="media-foreign"?std::string("http://foreign.invalid"):http_credentials::origin(argv[1]))};
    NetStream stream(argv[1], headers, &cancel, !std::string(argv[2]).starts_with("media-") && std::string(argv[2]) != "single" && std::string(argv[2]) != "rapid-single");
    rapid=std::string(argv[2])=="rapid-single";
    json result; result["started"] = stream.start();
    if(shared_dns && result["started"]==true) {
        std::string endpoint=argv[1];endpoint.replace(endpoint.find("stream.fixture.invalid"),22,"subs.fixture.invalid");
        endpoint=endpoint.substr(0,endpoint.find('/',7))+"/captions";
        auto response=http_get_subtitle(endpoint,&cancel);
        result["subtitle_ok"]=response.ok() && response.body=="Caption fixture";
    }
    if (result["started"] == true) {
        uint8_t bytes[32]{}; int n = stream.read(bytes, sizeof bytes);
        result["read"] = n;
        if (n == 32) {
            bool correct = true; for (int i = 0; i < 32; ++i) correct &= bytes[i] == uint8_t(i);
            result["correct"] = correct;
            if(rapid) {
                hold_next_init=true;
                stream.seek(0,SEEK_SET);
                const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
                while(!request_waiting && std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
                const bool pending=request_waiting;
                stream.seek(53,SEEK_SET);release_request=true;
                n=stream.read(bytes,sizeof bytes);correct=pending && n==32;
                for(int i=0;i<32;++i)correct &= bytes[i]==uint8_t(53+i);
                result["seek_correct"]=correct;
            }
            if (std::string(argv[2]) == "seek" || test_mode=="thread-no-workers" || test_mode=="thread-two-workers") {
                const int64_t position = (180ll << 20) + 19;
                result["seek"] = stream.seek(position, SEEK_SET);
                n = stream.read(bytes, sizeof bytes);
                correct = n == 32;
                for (int i = 0; i < 32; ++i) correct &= bytes[i] == uint8_t(position + i);
                result["seek_correct"] = correct;
            }
        }
    }
    result["error"] = stream.failure(); result["fault_calls"] = faults.load();
    std::cout << result.dump() << std::endl;
} catch(const std::exception& error) {std::cerr<<"Uncaught stream exception: "<<error.what()<<'\n';return 1;}
