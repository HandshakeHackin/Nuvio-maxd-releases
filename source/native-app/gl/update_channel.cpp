// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 BlackBearReloaded (loader handoff)
// Loader handoff adapted from BlackBearReloaded's native app kit / ProsperoEden.
#include "update_channel.hpp"
#include <array>
#include <cstdint>
#include <fcntl.h>
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
void UpdateChannel::close(){if(socket_>=0)sceNetSocketClose(socket_);socket_=-1;}
bool UpdateChannel::send(const void* data,std::size_t size,const std::atomic<bool>& cancel) {
    auto p=static_cast<const char*>(data);
    while(size && !cancel) {auto n=sceNetSend(socket_,p,size,0);if(n<=0)return false;p+=n;size-=n;}
    return size==0;
}
bool UpdateChannel::line(std::string& out,const std::atomic<bool>& cancel) {
    out.clear();while(out.size()<8192 && !cancel) {char c;auto n=sceNetRecv(socket_,&c,1,0);if(n!=1)return false;if(c=='\n')return true;out+=c;}return false;
}
bool UpdateChannel::open(const std::string& helper,const std::atomic<bool>& cancel,std::string& error) {
    int fd=::open(helper.c_str(),O_RDONLY);if(fd<0){error="The bundled update helper is missing";return false;}
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
    ::close(fd);if(!ok){close();error="Could not start the automatic helper. Enable the PS5 payload loader on port 9021.";}return ok;
}
}
