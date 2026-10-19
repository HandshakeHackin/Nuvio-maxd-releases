// SPDX-License-Identifier: GPL-3.0-or-later
// Host-only syscall adapters for the unmodified PS5 helper's probe protocol.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <cstring>
extern "C" int __real_open(const char*,int,...);
extern "C" int __wrap_chdir(const char*) {return 0;}
extern "C" int __wrap_open(const char* path,int flags,...) {
    int mode=0;if(flags&O_CREAT){va_list args;va_start(args,flags);mode=va_arg(args,int);va_end(args);}
    if(std::strcmp(path,"/data/nuvio-maxd-updater.log")==0)path=std::getenv("NUVIO_HELPER_TEST_LOG");
    return __real_open(path,flags,mode);
}
extern "C" int sceKernelSendNotificationRequest(int,void* request,std::size_t,int) {
    std::fprintf(stderr,"NOTIFICATION: %s\n",static_cast<char*>(request)+45);return 0;
}
