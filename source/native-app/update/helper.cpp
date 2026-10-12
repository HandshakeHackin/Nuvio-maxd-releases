// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 BlackBearReloaded (loader handoff and notification ABI)
// Bundled payload, launched by Nuvio Max'd through the local payload loader.
// The loader handoff follows BlackBearReloaded's ProsperoEden / native app kit.
#include "core.hpp"
#include <curl/curl.h>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <poll.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <thread>
#include <unistd.h>
extern "C" int sceKernelSendNotificationRequest(int,void*,std::size_t,int);
namespace {
using namespace nuvio_update;
bool send(const Json& j) {
    auto s=j.dump()+"\n"; std::size_t p=0;
    while(p<s.size()) {auto n=write(STDOUT_FILENO,s.data()+p,s.size()-p);if(n<=0)return false;p+=n;} return true;
}
bool read_line(std::string& s,int seconds) {
    s.clear(); const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    while(s.size()<8192 && std::chrono::steady_clock::now()<end) {
        pollfd p{STDIN_FILENO,POLLIN,0}; if(poll(&p,1,1000)<0)return false; if(!(p.revents&(POLLIN|POLLHUP)))continue;
        char c; if(read(STDIN_FILENO,&c,1)!=1)return false; if(c=='\n')return true;s+=c;
    } return false;
}
std::size_t write_json(char* p,std::size_t a,std::size_t b,void* v) {
    auto& s=*static_cast<std::string*>(v); if(a*b>1048576-s.size())return 0;s.append(p,a*b);return a*b;
}
Json api(const char* route,const Json& body) {
    CURL* c=curl_easy_init(); if(!c)return Json();
    auto request=body.dump();std::string response,url="http://127.0.0.1:10101/api/v1/"+std::string(route);
    auto headers=curl_slist_append(nullptr,"Content-Type: application/json");
    curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_PROXY,"");
    curl_easy_setopt(c,CURLOPT_POSTFIELDS,request.c_str());curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,2L);curl_easy_setopt(c,CURLOPT_TIMEOUT,5L);
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http");
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,write_json);curl_easy_setopt(c,CURLOPT_WRITEDATA,&response);
    auto rc=curl_easy_perform(c);curl_slist_free_all(headers);curl_easy_cleanup(c);
    return rc==CURLE_OK?Json::parse(response,nullptr,false):Json();
}
void notify(const std::string& text) {
    struct Request {unsigned char reserved[45];char message[3075];} r{};
    std::snprintf(r.message,sizeof(r.message),"Nuvio Max'd: %s",text.c_str());sceKernelSendNotificationRequest(0,&r,sizeof(r),0);
}
bool capability(const Json& j,const char* value) {
    if(!j.is_object() || !j.contains("capabilities") || !j["capabilities"].is_array())return false;
    for(const auto& c:j["capabilities"])if(c==value)return true;return false;
}
int run() {
    std::string line,error;
    if(!read_line(line,30))return 1;
    auto j=Json::parse(line,nullptr,false);
    if(!j.is_object() || j.value("protocol",0)!=1 || j.value("title_id",std::string())!=title_id)return 1;
    Release r{j.value("version",std::string()),j.value("tag",std::string()),j.value("url",std::string()),j.value("sha256",std::string()),"",j.value("size",std::uint64_t(0))};
    if(!valid_release(r) || !newer(r.version,current_version)) {send({{"state","error"},{"message","Invalid release metadata or older version"}});return 1;}
    auto v=api("version",Json::object());
    if(!v.is_object() || v.value("api_version",0)!=1 || !capability(v,"game_info") || !capability(v,"unmount_game") || !capability(v,"list_images") || !capability(v,"rescan")) {
        send({{"state","error"},{"message","Automatic image updates need ShadowMount+ 1.7 with its local API enabled on port 10101"}});return 1;
    }
    const Json title={{"title_id",title_id}};
    auto path=installed_image(api("games/info",title));
    if(path.empty()) {send({{"state","error"},{"message","Nuvio Max'd must be installed as an FFPFSC in a managed /homebrew/ folder"}});return 1;}
    Transaction tx;
    if(!tx.begin(path,error)) {send({{"state","error"},{"message",error}});return 1;}
    struct statfs space{};
    if(statfs(path.c_str(),&space) || space.f_bavail<0 || std::uint64_t(space.f_bavail)*space.f_bsize < r.size+tx.original_size()+(16ull<<20)) {
        send({{"state","error"},{"message","Not enough free space for the new image and an original-image backup"}});return 1;
    }
    if(!send({{"state","receiving"}}))return 1;
    std::array<char,65536> data{};
    while(tx.received()<r.size) {
        pollfd p{STDIN_FILENO,POLLIN,0};if(poll(&p,1,30000)<=0)return 1;
        auto n=read(STDIN_FILENO,data.data(),std::min<std::uint64_t>(data.size(),r.size-tx.received()));
        if(n<=0 || !tx.append(data.data(),n,error))return 1;
    }
    if(!tx.verify(r,error)) {send({{"state","error"},{"message",error}});return 1;}
    if(!send({{"state","ready"}}) || !read_line(line,300) || line!="install")return 1;
    if(!send({{"state","closing"}}))return 1;
    // Closing the channel alone is insufficient proof that Nuvio Max'd has exited.
    // ShadowMount's guarded unmount refuses while a title is active.
    if(!read_line(line,120) || line!="exit") {notify("Update cancelled before installation; original retained.");return 1;}
    bool released=false;
    for(int attempt=0;attempt<120;++attempt) {
        auto reply=api("games/unmount",title);
        if(reply.is_object() && reply.value("status",-1)==0) {released=mount_released(api("images",Json::object()),path);break;}
        if(!reply.is_object() || reply.value("status",-1)!=16)break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if(!released) {notify("Update not installed: the image is still mounted or ShadowMount is unavailable. Original retained; manual replacement is still available.");return 1;}
    auto guard=[&] {
        return installed_image(api("games/info",title))==path && mount_released(api("images",Json::object()),path);
    };
    if(!tx.commit(guard,error)) {notify(error);return 1;}
    const auto scan=api("scan",{{"reset_attempts",false}});
    if(!error.empty()) notify(error);
    else if(!scan.is_object() || scan.value("status",-1)!=0 || !scan.value("scan_queued",false))notify("Update installed and backup saved. Restart ShadowMount+ to refresh the app.");
    else notify("Update installed and backup saved. ShadowMount+ rescan requested; reopen Nuvio Max'd after it finishes.");
    return 0;
}
}
int main() {
    std::signal(SIGPIPE,SIG_IGN);curl_global_init(CURL_GLOBAL_ALL);
    int result=1;try {result=run();}catch(...) {send({{"state","error"},{"message","Unexpected update response; original retained"}});}
    curl_global_cleanup();return result;
}
