// Link the exact pinned PS5 curl nonblocking object and production platform shim.
// Reproduce the custom-provider socket path with sandbox fcntl/readback refused.
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
static int console_errno=0,fcntl_calls=0,readbacks=0,writes=0,last_mode=-1;
static bool refuse_write=false;
extern "C" {
int curlx_nonblock(int,int);
int __real_curlx_nonblock(int,int);
int* __error() { return &console_errno; }
int fcntl(int fd,int command,...) {
    assert(fd==42&&command==3); ++fcntl_calls; console_errno=22; return -1;
}
int getsockopt(int fd,int level,int option,void*,unsigned*) {
    assert(fd==42&&level==0xffff&&option==0x1200); ++readbacks;
    console_errno=42; return -1; // SO_NBIO can be written without readable flags
}
int setsockopt(int fd,int level,int option,const void* value,unsigned length) {
    assert(fd==42&&level==0xffff&&option==0x1200&&length==sizeof(int));
    ++writes; last_mode=*static_cast<const int*>(value);
    if(refuse_write){console_errno=9;return -1;} return 0;
}
}
int main() {
    // Old SDK path: curl maps this failure to curl 1 in cf-socket.c.
    assert(__real_curlx_nonblock(42,1)==-1&&fcntl_calls==1&&readbacks==1);
    const auto old_fcntl=fcntl_calls,old_reads=readbacks;
    assert(curlx_nonblock(42,1)==0&&last_mode==1);
    assert(curlx_nonblock(42,0)==0&&last_mode==0);
    assert(curlx_nonblock(42,7)==0&&last_mode==1); // normalize boolean
    assert(fcntl_calls==old_fcntl&&readbacks==old_reads&&writes==3);
    refuse_write=true;
    assert(curlx_nonblock(42,1)==-1&&console_errno==9); // real socket failures stay failures
    std::puts("Pinned PS5 curl socket regression: old fcntl/readback fails; direct SO_NBIO enables/disables and preserves write errors");
}
