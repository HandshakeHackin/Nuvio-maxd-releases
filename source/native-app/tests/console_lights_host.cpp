#include "console_lights.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <thread>
#include <cstdio>
#include <pthread.h>
#include <dlfcn.h>
static std::mutex mock_mutex;
static std::array<uint8_t,130> hardware{};
static bool deny=false, refuse_write=false;
static bool deny_thread=false;
// Interpose the real exported symbol too: host libstdc++ starts threads inside
// its shared library, where a link-only --wrap cannot intercept the call.
extern "C" int pthread_create(pthread_t* t,const pthread_attr_t* a,void*(*f)(void*),void* p) noexcept {
    using Create=int(*)(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);
    static auto real=reinterpret_cast<Create>(dlsym(RTLD_NEXT,"pthread_create"));
    return deny_thread ? EAGAIN : real(t,a,f,p);
}
static int opens=0,writes=0,closes=0;
extern "C" int __wrap_open(const char* path,int flags,...) {
    std::lock_guard<std::mutex> lock(mock_mutex);
    assert(std::strcmp(path,"/dev/icc_indicator")==0 && flags==2); ++opens;
    if(deny) {errno=EACCES;return -1;} return 42;
}
extern "C" int __wrap_close(int fd) {std::lock_guard<std::mutex> lock(mock_mutex);assert(fd==42);++closes;return 0;}
extern "C" int __wrap_ioctl(int fd,unsigned long cmd,...) {
    assert(fd==42); va_list args;va_start(args,cmd);auto* state=va_arg(args,uint8_t*);va_end(args);
    std::lock_guard<std::mutex> lock(mock_mutex);
    if(cmd==0x40829505ul) {std::memcpy(state,hardware.data(),130);return 0;}
    assert(cmd==0x80829504ul);++writes;
    if(refuse_write) {errno=EPERM;return -1;}
    std::memcpy(hardware.data(),state,130);return 0;
}
void dlog(const char*,...) {}
template<class F> void until(F ready) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(std::chrono::steady_clock::now()<deadline) {
        if(ready())return; std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(false && "console lighting worker timed out");
}
static void fixture() {
    std::lock_guard<std::mutex> lock(mock_mutex); hardware={};
    hardware[4]=1;hardware[5]=1;hardware[7]=20;
    hardware[10]=0x11;hardware[11]=1;hardware[13]=80;
    hardware[16]=0x21;hardware[17]=1;hardware[19]=10;
    deny=refuse_write=false;opens=writes=closes=0;
}
int main() {
    using Status=sx::ConsoleLights::Status;
    fixture();
    std::array<uint8_t,130> original;{std::lock_guard<std::mutex> lock(mock_mutex);original=hardware;}
    { sx::ConsoleLights lights;
      lights.update(true,0,74,89);until([&]{return lights.status()==Status::active;});
      {std::lock_guard<std::mutex> lock(mock_mutex);assert(hardware[7]==89 && hardware[13]==0 && hardware[19]==0);}
      lights.update(false,0,0,0);until([&]{return lights.status()==Status::idle;});
      {std::lock_guard<std::mutex> lock(mock_mutex);assert(hardware==original);}
      lights.update(true,89,89,89);until([&]{return lights.status()==Status::active;});
      // Emulate ShellUI taking ownership. Nuvio must stop, never overwrite it.
      {std::lock_guard<std::mutex> lock(mock_mutex);hardware[13]=17;}
      until([&]{return lights.status()==Status::system_owned;});
      lights.stop();
      {std::lock_guard<std::mutex> lock(mock_mutex);assert(hardware[13]==17);}
    }
    fixture();{sx::ConsoleLights lights;lights.update(true,51,15,89);until([&]{return lights.status()==Status::active;});}
    {std::lock_guard<std::mutex> lock(mock_mutex);assert(hardware==original && opens==closes);}
    fixture();deny=true;
    {sx::ConsoleLights lights;lights.update(true,0,74,89);until([&]{return lights.status()==Status::unavailable;});
     for(int i=0;i<100;++i) lights.update(true,0,74,89);
     std::this_thread::sleep_for(std::chrono::milliseconds(250));
     {std::lock_guard<std::mutex> lock(mock_mutex);assert(opens==1 && writes==0);deny=false;}
     lights.update(false,0,0,0);lights.update(true,0,74,89);until([&]{return lights.status()==Status::active;});}
    fixture();refuse_write=true;
    {sx::ConsoleLights lights;lights.update(true,0,74,89);until([&]{return lights.status()==Status::unavailable;});}
    {std::lock_guard<std::mutex> lock(mock_mutex);assert(hardware==original);}
    fixture();hardware[28]=1;hardware[29]=1; // ambiguous duplicate channel record
    {sx::ConsoleLights lights;lights.update(true,0,74,89);until([&]{return lights.status()==Status::unavailable;});}
    {std::lock_guard<std::mutex> lock(mock_mutex);assert(writes==0);}
    fixture();hardware[7]=hardware[13]=hardware[19]=0;
    {sx::ConsoleLights lights;lights.update(true,0,74,89);until([&]{return lights.status()==Status::unavailable;});}
    {std::lock_guard<std::mutex> lock(mock_mutex);assert(writes==0);}
    fixture();deny_thread=true;
    {sx::ConsoleLights lights;assert(lights.status()==Status::unavailable);lights.update(true,0,74,89);lights.stop();}
    assert(opens==0);deny_thread=false;
    std::puts("Console lighting: saved state, disable/exit restoration, system priority, denied access, retry, rejected writes and malformed layouts passed");
}
