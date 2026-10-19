// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_update.hpp"
#include "http.h"
#include "util.h"
#include <curl/curl.h>
#include <array>
#include <fcntl.h>
#include <unistd.h>
namespace sx {
namespace {
using nuvio_update::Json;
struct Download {
    int fd=-1;std::uint64_t done=0,limit=0;const std::atomic<bool>* cancel=nullptr;
    std::function<void(float)> progress;
};
std::size_t receive_image(char* p,std::size_t a,std::size_t b,void* v) {
    auto& d=*static_cast<Download*>(v);auto bytes=a*b;
    if(bytes>d.limit-d.done || *d.cancel)return 0;
    std::size_t pos=0;while(pos<bytes) {auto n=write(d.fd,p+pos,bytes-pos);if(n<=0)return 0;pos+=n;}
    d.done+=bytes;d.progress(float(double(d.done)/d.limit));return bytes;
}
int download_progress(void* v,curl_off_t,curl_off_t,curl_off_t,curl_off_t) {return static_cast<Download*>(v)->cancel->load()?1:0;}
bool download_image(const nuvio_update::Release& r,int fd,const std::atomic<bool>& cancel,std::function<void(float)> progress,std::string& error) {
    if(!nuvio_update::valid_release(r)){error="Invalid release";return false;}
    auto c=curl_easy_init();if(!c){error="Cannot start the download";return false;}
    Download d{fd,0,r.size,&cancel,std::move(progress)};char why[CURL_ERROR_SIZE]{};
    curl_easy_setopt(c,CURLOPT_URL,r.url.c_str());curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(c,CURLOPT_MAXREDIRS,5L);curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"https");curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"https");
    curl_easy_setopt(c,CURLOPT_USERAGENT,kUserAgent);curl_easy_setopt(c,CURLOPT_CAINFO,http_ca_bundle().c_str());
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,10L);curl_easy_setopt(c,CURLOPT_TIMEOUT,600L);
    curl_easy_setopt(c,CURLOPT_LOW_SPEED_LIMIT,1024L);curl_easy_setopt(c,CURLOPT_LOW_SPEED_TIME,30L);
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_IPRESOLVE,CURL_IPRESOLVE_V4);curl_easy_setopt(c,CURLOPT_ERRORBUFFER,why);
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,receive_image);curl_easy_setopt(c,CURLOPT_WRITEDATA,&d);
    curl_easy_setopt(c,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(c,CURLOPT_XFERINFOFUNCTION,download_progress);curl_easy_setopt(c,CURLOPT_XFERINFODATA,&d);
    http_setup_handle(c);auto rc=curl_easy_perform(c);long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);curl_easy_cleanup(c);
    if(cancel){error="Update cancelled";return false;}
    if(rc!=CURLE_OK || code!=200){error="Update download failed (HTTP "+std::to_string(code)+"): "+(why[0]?why:curl_easy_strerror(rc));return false;}
    if(d.done!=r.size || nuvio_update::sha256_file(fd)!=r.sha256 || fsync(fd)){error="Update size or SHA-256 does not match GitHub. Existing install retained.";return false;}
    return true;
}
bool response(UpdateChannel& channel,const std::atomic<bool>& cancel,std::string& state,std::string& error) {
    std::string line;if(!channel.line(line,cancel)){error="The update helper stopped responding";return false;}
    auto j=Json::parse(line,nullptr,false);if(!j.is_object()){error="Invalid update helper reply";return false;}
    state=j.value("state",std::string());if(state=="error"){error=j.value("message",std::string("The helper refused the update"));return false;}return true;
}
}
AppUpdate::~AppUpdate(){shutdown();}
AppUpdate::Status AppUpdate::status() const {std::lock_guard lock(mutex_);return state_;}
void AppUpdate::start_worker(std::function<void()> work) {
    try {worker_=std::thread(std::move(work));}
    catch(...) {running_=false;set(Phase::error,"Could not start an update worker. Close and reopen Nuvio Max'd, then retry.");}
}
void AppUpdate::set(Phase p,const std::string& m,float progress) {std::lock_guard lock(mutex_);if(state_.phase!=p)dlog("updater: phase %d: %s",int(p),m.c_str());state_.phase=p;state_.message=m;state_.progress=progress;}
void AppUpdate::cancel(){cancel_=true;cv_.notify_all();}
void AppUpdate::shutdown(){if(status().phase!=Phase::closing)cancel();if(worker_.joinable())worker_.join();if(status().phase!=Phase::closing)channel_.close();}
void AppUpdate::finish_exit(){if(status().phase!=Phase::closing)return;if(worker_.joinable())worker_.join();std::atomic<bool> no_cancel{false};const std::string exit="exit\n";channel_.send(exit.data(),exit.size(),no_cancel);channel_.close();}
void AppUpdate::check() {
    if(running_)return;if(worker_.joinable())worker_.join();cancel_=false;running_=true;set(Phase::checking,"Checking for updates...");
    start_worker([this] {
        try {
            const auto r=http_get_https(nuvio_update::feed,20,&cancel_,{"Accept: application/vnd.github+json"});
            if(cancel_)set(Phase::cancelled,"Update check cancelled");
            else if(!r.ok())set(Phase::error,r.status==404?"The public release feed is not available yet. Manual FFPFSC updates still work.":"Could not check for updates: "+r.describe());
            else {
                auto metadata=Json::parse(r.body,nullptr,false);
                if(!metadata.is_array())throw std::runtime_error("Invalid release response");
                auto release=nuvio_update::select_release(metadata,nuvio_update::current_version);
                {std::lock_guard lock(mutex_);state_.release=release;}
                set(release.version.empty()?Phase::current:Phase::available,release.version.empty()?"No newer verified native release is available":"Nuvio Max'd "+release.version+" is available");
            }
        }catch(...){set(Phase::error,"Could not read the release feed");}running_=false;
    });
}
void AppUpdate::download(const std::string& base,const std::string& data) {
    if(running_ || status().phase!=Phase::available)return;if(worker_.joinable())worker_.join();
    const auto r=status().release;cancel_=false;install_=false;running_=true;set(Phase::downloading,"Downloading Nuvio Max'd "+r.version,0);
    start_worker([this,base,data,r] {
        std::string error,state;const auto local=data+"/update-"+std::to_string(getpid())+".part";
        int fd=open(local.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);bool owned=fd>=0;
        try {
            bool ok=owned && download_image(r,fd,cancel_,[this](float p){set(Phase::downloading,"Downloading and verifying the FFPFSC",p);},error);
            if(!owned)error="Cannot stage the download in Nuvio Max'd storage. Remove a leftover update .part file and retry.";
            if(ok) {set(Phase::staging,"Preparing the update helper...");ok=channel_.open(base+"/nuvio-updater.elf",cancel_,error);}
            if(ok) {
                auto plan=Json{{"protocol",1},{"title_id",nuvio_update::title_id},{"version",r.version},{"tag",r.tag},{"url",r.url},{"sha256",r.sha256},{"size",r.size}}.dump()+"\n";
                ok=channel_.send(plan.data(),plan.size(),cancel_) && response(channel_,cancel_,state,error) && state=="receiving";
            }
            std::array<char,65536> bytes{};std::uint64_t moved=0;
            while(ok && moved<r.size) {auto n=pread(fd,bytes.data(),std::min<std::uint64_t>(bytes.size(),r.size-moved),moved);ok=n>0 && channel_.send(bytes.data(),n,cancel_);if(n>0)moved+=n;set(Phase::staging,"Staging the verified image on install storage",float(double(moved)/r.size));}
            if(ok)ok=response(channel_,cancel_,state,error) && state=="ready";
            if(ok) {
                set(Phase::ready,"Update verified. Install and close Nuvio Max'd when you are ready.",1);
                std::unique_lock lock(mutex_);cv_.wait_for(lock,std::chrono::seconds(240),[this]{return install_ || cancel_.load();});ok=install_ && !cancel_;lock.unlock();
                if(ok) {const std::string install="install\n";ok=channel_.send(install.data(),install.size(),cancel_) && response(channel_,cancel_,state,error) && state=="closing";}
            }
            if(ok)set(Phase::closing,"Closing Nuvio Max'd to install the update");
            else {channel_.close();set(cancel_?Phase::cancelled:Phase::error,cancel_?"Update cancelled; existing install retained":error.empty()?"Update expired or helper disconnected; existing install retained":error);}
        }catch(...){channel_.close();set(Phase::error,"Update failed; existing install retained");}
        if(fd>=0)close(fd);if(owned)unlink(local.c_str());running_=false;
    });
}
void AppUpdate::probe(const std::string& base) {
    if(running_)return;
    if(worker_.joinable())worker_.join();
    cancel_=false;install_=false;running_=true;set(Phase::staging,"Preparing updater handoff test...");
    start_worker([this,base] {
        std::string error,state;
        try {
            bool ok=channel_.open(base+"/nuvio-updater.elf",cancel_,error);
            const auto plan=Json{{"protocol",1},{"title_id",nuvio_update::title_id},{"mode","probe"}}.dump()+"\n";
            ok=ok && channel_.send(plan.data(),plan.size(),cancel_) && response(channel_,cancel_,state,error) && state=="ready";
            const std::string install="install\n";
            ok=ok && !cancel_ && channel_.send(install.data(),install.size(),cancel_) && response(channel_,cancel_,state,error) && state=="closing";
            if(ok)set(Phase::closing,"Closing for updater handoff test; no image will be replaced");
            else {channel_.close();set(cancel_?Phase::cancelled:Phase::error,error.empty()?"Updater handoff test cancelled":error);}
        }catch(...) {channel_.close();set(Phase::error,"Updater handoff test failed; no image was replaced");}
        running_=false;
    });
}
void AppUpdate::install(){if(status().phase!=Phase::ready)return;{std::lock_guard lock(mutex_);install_=true;}cv_.notify_all();}
}
