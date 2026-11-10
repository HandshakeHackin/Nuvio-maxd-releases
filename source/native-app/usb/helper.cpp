// SPDX-License-Identifier: GPL-3.0-or-later
// One request over the payload loader's inherited stdin/stdout connection.
// Reads only USB playlists. No listener, network requests, filesystem writes,
// title credential or mount changes. Standard SDK startup prepares the helper
// process itself; see USB_READER_HELPER.md for the runtime privilege boundary.
#include "protocol.hpp"
#include <chrono>
#include <csignal>
#include <poll.h>
#include <thread>
namespace {
using namespace usb_reader;
bool output(const void* p,size_t n){auto* s=static_cast<const char*>(p);while(n){auto r=write(STDOUT_FILENO,s,n);if(r<0&&errno==EINTR)continue;if(r<=0||size_t(r)>n)return false;s+=r;n-=r;}return true;}
bool json(const Json& j){auto s=j.dump()+"\n";return output(s.data(),s.size());}
bool line(std::string& s){s.clear();auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(s.size()<4096&&std::chrono::steady_clock::now()<end){pollfd p{STDIN_FILENO,POLLIN,0};if(poll(&p,1,250)<0){if(errno==EINTR)continue;return false;}if(p.revents&(POLLERR|POLLNVAL))return false;if(!(p.revents&(POLLIN|POLLHUP)))continue;char c;if(read(STDIN_FILENO,&c,1)!=1)return false;if(c=='\n')return true;s+=c;}return false;
}
int run(){
    std::string request;if(!line(request))return 1;auto j=Json::parse(request,nullptr,false);
    if(!request_ok(j)){json({{"kind","error"},{"code","invalid-request"}});return 1;}
    std::atomic<bool> cancel{false},finished{false};
    auto start=std::chrono::steady_clock::now();
    std::thread monitor([&]{while(!finished){pollfd p{STDIN_FILENO,POLLIN,0};auto r=poll(&p,1,100);
        if((r<0&&errno!=EINTR)||(r>0&&(p.revents&(POLLERR|POLLHUP|POLLNVAL)))){cancel=true;break;}
        if(r>0&&(p.revents&POLLIN)){char c;(void)read(STDIN_FILENO,&c,1);cancel=true;break;}
        if(std::chrono::steady_clock::now()-start>std::chrono::seconds(30)){cancel=true;break;}}});
    struct Join {std::atomic<bool>& done;std::thread& t;~Join(){done=true;t.join();}} join{finished,monitor};
    auto roots=usb_reader::roots();
#ifdef NUVIO_USB_READER_HOST_TEST
    if(const char* base=std::getenv("NUVIO_USB_READER_TEST_ROOT"))for(int i=0;i<8;++i)roots[i]=std::string(base)+"/usb"+std::to_string(i);
#endif
    if(!json({{"kind","ready"},{"protocol",protocol},{"title_id",title}}))return 1;
    if(j["op"]=="scan"){
        std::vector<live_usb::MountDiagnostics> diagnostics;auto files=live_usb::scan(roots,&diagnostics,&cancel);
        if(cancel){json({{"kind","error"},{"code","cancelled"}});return 1;}
        for(size_t i=0;i<diagnostics.size();++i)if(!json(diagnostic(diagnostics[i],i)))return 1;
        size_t emitted=0;for(const auto& file:files){auto it=std::find(roots.begin(),roots.end(),file.root);if(it==roots.end()||!path_ok(file.path))continue;
            if(!json({{"kind","file"},{"mount",it-roots.begin()},{"path",file.path}}))return 1;++emitted;}
        return json({{"kind","done"},{"files",emitted}})?0:1;
    }
    const int mount=j["mount"].get<int>();const auto path=j["path"].get<std::string>();std::string error;
    auto body=live_usb::read_file({roots[size_t(mount)],path},&cancel,error);
    if(!error.empty()||body.empty()||cancel){json({{"kind","error"},{"code",cancel?"cancelled":"read-rejected"}});return 1;}
    if(!json({{"kind","body"},{"bytes",body.size()}}))return 1;
    return output(body.data(),body.size())?0:1;
}
}
int main(){std::signal(SIGPIPE,SIG_IGN);try{return run();}catch(...){json({{"kind","error"},{"code","reader-failed"}});return 1;}}
