// Production curl transport against a local HTTPS server. Inject only the
// first resolver failure; successful transfers use the real curl/TLS stack.
#include "http.h"
#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

static std::string mode;
static int attempts=0;
static std::atomic<bool> cancelled{false};
extern "C" CURLcode __real_curl_easy_perform(CURL*);
extern "C" CURLcode __wrap_curl_easy_perform(CURL* handle) {
    ++attempts;
    if(mode=="cancel-retry") { cancelled=true; return CURLE_COULDNT_RESOLVE_HOST; }
    if(mode=="persistent-dns" || (mode=="retry-dns" && attempts==1)) return CURLE_COULDNT_RESOLVE_HOST;
    if(mode=="retry-connect" && attempts==1) return CURLE_COULDNT_CONNECT;
    return __real_curl_easy_perform(handle);
}
static void require(bool ok,const char* message) {
    if(!ok) throw std::runtime_error(message);
}
int main(int argc,char** argv) try {
    require(argc==4,"mode, endpoint and test CA required");
    mode=argv[1]; http_init(argv[3]);
    if(mode=="pre-cancel") cancelled=true;
    std::thread cancel_worker;
    if(mode=="cancel-transfer") cancel_worker=std::thread([]{
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); cancelled=true;
    });
    const auto begin=std::chrono::steady_clock::now();
    auto response=http_get_subtitle(argv[2],&cancelled);
    const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    if(cancel_worker.joinable()) cancel_worker.join();
    if(mode=="pre-cancel" || mode=="cancel-retry" || mode=="cancel-transfer") {
        require(response.error=="cancelled","subtitle cancellation failed");
        require(attempts==(mode=="pre-cancel"?0:1),"cancelled request was retried");
        require(elapsed<3,"subtitle cancellation was not prompt");
    } else if(mode=="oversized") {
        require(!response.ok() && response.body.size()<=8u*1024*1024,"decoded subtitle exceeded its byte limit");
        require(response.error=="Subtitle file is too large" && attempts==1,"oversized subtitle was retried or not identified");
    } else if(mode=="persistent-dns") {
        require(!response.ok() && attempts==2,"resolver retry was not bounded to one");
        require(response.error.find("private")==std::string::npos,"failure disclosed a private address");
    } else if(mode=="http-error") {
        require(response.status==503 && !response.ok() && attempts==1,"HTTP error was retried as a DNS error");
    } else if(mode=="bad-ca") {
        require(!response.ok() && attempts==1,"TLS verification was bypassed or retried");
    } else if(mode=="bad-redirect") {
        require(!response.ok() && response.body.empty() && attempts==1,"non-HTTP subtitle redirect was accepted or retried");
    } else {
        require(response.ok() && response.body=="1\n00:00:01,000 --> 00:00:03,000\nCaption\n","subtitle redirect/gzip/TLS transfer failed");
        require(attempts==((mode=="retry-dns" || mode=="retry-connect" || mode=="retry-redirect-connect")?2:1),"unexpected subtitle attempt count");
        if(mode=="slow-connect") require(elapsed>=10 && elapsed<25,"slow TLS connection was not exercised");
    }
    std::cout<<"Subtitle download: "<<mode<<" passed\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
