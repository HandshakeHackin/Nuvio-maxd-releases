#include "../ui/i18n.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 BlackBearReloaded (loader handoff)
// Loader handoff adapted from BlackBearReloaded's native app kit / ProsperoEden.
#include "update_channel.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
extern "C" {
int sceNetSocket(const char*,int,int,int);
int sceNetConnect(int,const void*,unsigned);
int sceNetSetsockopt(int,int,int,const void*,unsigned);
int sceNetSend(int,const void*,std::size_t,int);
int sceNetRecv(int,void*,std::size_t,int);
int sceNetSocketClose(int);
}
namespace sx {
UpdateChannel::~UpdateChannel(){close();}
void UpdateChannel::close(){if(socket_>=0)sceNetSocketClose(socket_);socket_=-1;at_=end_=0;}
bool UpdateChannel::send(const void* data,std::size_t size,const std::atomic<bool>& cancel) {
    auto p=static_cast<const char*>(data);
    while(size && active(cancel)) {auto n=sceNetSend(socket_,p,size,0);if(n<=0||std::size_t(n)>size)return false;p+=n;size-=n;}
    return size==0;
}
bool UpdateChannel::active(const std::atomic<bool>& cancel) const {return !cancel&&std::chrono::steady_clock::now()<deadline_;}
bool UpdateChannel::receive(void* data,std::size_t size,const std::atomic<bool>& cancel) {
    auto* p=static_cast<char*>(data);
    while(size&&active(cancel)){
        if(at_<end_){auto n=std::min(size,end_-at_);std::memcpy(p,input_.data()+at_,n);at_+=n;p+=n;size-=n;continue;}
        auto n=sceNetRecv(socket_,p,std::min<std::size_t>(size,65536),0);if(n<=0||std::size_t(n)>size)return false;p+=n;size-=n;
    }return size==0&&active(cancel);
}
bool UpdateChannel::line(std::string& out,const std::atomic<bool>& cancel) {
    out.clear();while(out.size()<8192&&active(cancel)){
        if(at_==end_){auto n=sceNetRecv(socket_,input_.data(),input_.size(),0);if(n<=0||std::size_t(n)>input_.size())return false;at_=0;end_=std::size_t(n);}
        char c=input_[at_++];if(c=='\n')return true;out+=c;
    }return false;
}
bool UpdateChannel::open(const std::string& helper,const std::atomic<bool>& cancel,std::string& error,unsigned deadline_seconds) {
    close();deadline_=deadline_seconds?std::chrono::steady_clock::now()+std::chrono::seconds(deadline_seconds):std::chrono::steady_clock::time_point::max();
    int fd=::open(helper.c_str(),O_RDONLY);if(fd<0){error=sx::i18n::tr("The bundled helper is missing");return false;}
    struct stat st{};if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||st.st_size>16*1024*1024){::close(fd);error=sx::i18n::tr("Invalid bundled helper");return false;}
    struct Address {std::uint8_t len,family;std::uint16_t port;std::uint32_t address;std::uint16_t vport;std::uint8_t pad[6];};
    static_assert(sizeof(Address)==16);
    const Address a{16,2,std::uint16_t((9021<<8)|(9021>>8)),0x0100007f,0,{}};
    const int io=30000000,connect=5000000;
    socket_=sceNetSocket("nuvio_update",2,1,6);
    bool ok=socket_>=0 && sceNetSetsockopt(socket_,0xffff,0x1105,&io,sizeof(io))>=0 &&
        sceNetSetsockopt(socket_,0xffff,0x1106,&io,sizeof(io))>=0 && sceNetSetsockopt(socket_,0xffff,0x1109,&connect,sizeof(connect))>=0 &&
        sceNetConnect(socket_,&a,sizeof(a))>=0;
    std::array<char,65536> buf{};
    while(ok) {auto n=read(fd,buf.data(),buf.size());if(n==0)break;if(n<0){ok=false;break;}ok=send(buf.data(),n,cancel);}
    ::close(fd);if(!ok){close();error=sx::i18n::tr("Could not start the automatic helper. Enable the PS5 payload loader on port 9021.");}return ok;
}
}
