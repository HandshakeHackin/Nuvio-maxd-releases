// Exercise the production stream reader and worker scheduler over local HTTP.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "netstream.h"
#include "http.h"
#include "util.h"
#include <curl/curl.h>
#include <atomic>
#include <iostream>
#include <thread>
#include <chrono>

static std::atomic<int> faults{0};
static std::atomic<bool> rapid{false}, release_request{false};
static std::atomic<int> opens{0};
extern "C" CURL* __real_curl_easy_init();
extern "C" CURL* __wrap_curl_easy_init() {
    if(rapid && ++opens==2)while(!release_request)std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
    return __real_curl_easy_perform(c);
}
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    http_init(""); NetStream::set_cache_dir(std::string(argv[2]) == "seek" ? "" : argv[3]);
    std::atomic<bool> cancel{false};
    NetStream stream(argv[1], {}, &cancel, std::string(argv[2]) != "single" && std::string(argv[2]) != "rapid-single");
    rapid=std::string(argv[2])=="rapid-single";
    json result; result["started"] = stream.start();
    if (result["started"] == true) {
        uint8_t bytes[32]{}; int n = stream.read(bytes, sizeof bytes);
        result["read"] = n;
        if (n == 32) {
            bool correct = true; for (int i = 0; i < 32; ++i) correct &= bytes[i] == uint8_t(i);
            result["correct"] = correct;
            if(rapid) {
                stream.seek(0,SEEK_SET);
                const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
                while(opens<2 && std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
                const bool pending=opens>=2;
                stream.seek(53,SEEK_SET);release_request=true;
                n=stream.read(bytes,sizeof bytes);correct=pending && n==32;
                for(int i=0;i<32;++i)correct &= bytes[i]==uint8_t(53+i);
                result["seek_correct"]=correct;
            }
            if (std::string(argv[2]) == "seek") {
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
}
