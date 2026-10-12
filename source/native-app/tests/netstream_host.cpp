// Exercise the production stream reader and worker scheduler over local HTTP.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "netstream.h"
#include "http.h"
#include "util.h"
#include <curl/curl.h>
#include <atomic>
#include <iostream>

static std::atomic<int> faults{0};
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
    NetStream stream(argv[1], {}, &cancel, std::string(argv[2]) != "single");
    json result; result["started"] = stream.start();
    if (result["started"] == true) {
        uint8_t bytes[32]{}; int n = stream.read(bytes, sizeof bytes);
        result["read"] = n;
        if (n == 32) {
            bool correct = true; for (int i = 0; i < 32; ++i) correct &= bytes[i] == uint8_t(i);
            result["correct"] = correct;
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
