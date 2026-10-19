// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 BlackBearReloaded (loader handoff and notification ABI)
// Bundled payload, launched by Nuvio Max'd through the local payload loader.
// The loader handoff follows BlackBearReloaded's ProsperoEden / native app kit.
#include "core.hpp"
#include "release_wait.hpp"
#include <fcntl.h>
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
constexpr const char* diagnostic_path="/data/nuvio-maxd-updater.log";
void trace(const std::string& message) {
    int fd=open(diagnostic_path,O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW,0600);
    if(fd<0)return;
    const auto line=std::string(current_version)+" "+message+"\n";
    (void)write(fd,line.data(),line.size());(void)fsync(fd);close(fd);
}
// Only status codes and known state fields are logged. API response bodies can
// include unrelated installed titles and are deliberately never recorded.

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
    auto result=rc==CURLE_OK?Json::parse(response,nullptr,false):Json();
    trace(std::string("api ")+route+" transport="+std::to_string(int(rc))+" status="+
          (result.is_object() && result.contains("status") && result["status"].is_number_integer() ? result["status"].dump() : "unknown"));
    return result;
}
void notify(const std::string& text) {
    struct Request {unsigned char reserved[45];char message[3075];} r{};
    std::snprintf(r.message,sizeof(r.message),"Nuvio Max'd: %s",text.c_str());sceKernelSendNotificationRequest(0,&r,sizeof(r),0);
}
bool capability(const Json& j,const char* value) {
    if(!j.is_object() || !j.contains("capabilities") || !j["capabilities"].is_array())return false;
    for(const auto& c:j["capabilities"])if(c==value)return true;return false;
}
bool wait_for_release(const Json& title,const std::string& path) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(120);
    while(std::chrono::steady_clock::now()<end) {
        const auto reply=api("games/unmount",title);
        const auto images=api("images",Json::object());
        if(images.is_object() && images.contains("images") && images["images"].is_array())
            for(const auto& image:images["images"]) if(image.is_object() && image.value("path",std::string())==path) {
                for(const char* key:{"source_available","mapped","mounted"})
                    trace(std::string("image ")+key+"="+(image.contains(key) && image[key].is_boolean()?image[key].dump():"unknown"));
            }
        const auto state=release_state(reply,images,path);
        if(state==ReleaseState::released) {trace("backing image released");return true;}
        if(state==ReleaseState::refused) {trace("unmount refused");return false;}
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    trace("backing image release timed out; original retained");return false;
}
int run() {
    // The helper is a loader payload, independent of /app0. Keep its working
    // directory out of the app mount while waiting for ShellCore to release it.
    if(chdir("/data")!=0)return 1;
    int log=open(diagnostic_path,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
    if(log>=0)close(log);
    trace("helper started");
    std::string line,error;
    if(!read_line(line,30))return 1;
    auto j=Json::parse(line,nullptr,false);
    if(!j.is_object() || j.value("protocol",0)!=1 || j.value("title_id",std::string())!=title_id)return 1;
    const bool probe=j.value("mode",std::string())=="probe";
    Release r{j.value("version",std::string()),j.value("tag",std::string()),j.value("url",std::string()),j.value("sha256",std::string()),"",j.value("size",std::uint64_t(0))};
    if(!probe && (!valid_release(r) || !newer(r.version,current_version))) {send({{"state","error"},{"message","Invalid release metadata or older version"}});return 1;}
    auto v=api("version",Json::object());
    if(!v.is_object() || v.value("api_version",0)!=1 || !capability(v,"game_info") || !capability(v,"unmount_game") || !capability(v,"list_images") || !capability(v,"rescan")) {
        send({{"state","error"},{"message","Automatic image updates need ShadowMount+ 1.7 with its local API enabled on port 10101"}});return 1;
    }
    const Json title={{"title_id",title_id}};
    auto path=installed_image(api("games/info",title));
    if(path.empty()) {send({{"state","error"},{"message","Nuvio Max'd must be installed as an FFPFSC in a managed /homebrew/ folder"}});return 1;}
    if(probe) {
        if(!send({{"state","ready"}}) || !read_line(line,300) || line!="install" ||
           !send({{"state","closing"}}) || !read_line(line,120) || line!="exit") {
            trace("handoff test cancelled");return 1;
        }
        trace("handoff test: no image replacement");
        const bool released=wait_for_release(title,path);
        notify(released ? "Updater handoff test passed. Reopen the app; no files were replaced."
            : "Updater handoff test failed. Original unchanged. Send /data/nuvio-maxd-updater.log and ShadowMount debug.log.");
        return released?0:1;
    }
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
    if(!wait_for_release(title,path)) {
        notify("Update not installed: backing image not released. Original retained. If persistent_image_mounts=1, disable it in ShadowMount config and reload ShadowMount. Log: /data/nuvio-maxd-updater.log");return 1;
    }
    auto guard=[&] {
        return installed_image(api("games/info",title))==path && mount_released(api("images",Json::object()),path);
    };
    if(!tx.commit(guard,error)) {trace("commit refused: "+error);notify(error);return 1;}
    trace("image committed; backup retained");
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
