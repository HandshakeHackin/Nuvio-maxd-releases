#include "ime.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <iostream>
struct Param {uint32_t user,type;uint64_t langs;int32_t label,method;void* filter;uint32_t option,max;uint16_t* buffer;};
static Param captured{};static int status=2,init_rc=0;
extern "C" {
int sceUserServiceInitialize(const void*){return 0;}
int sceUserServiceGetForegroundUser(int* user){*user=1;return 0;}
int sceImeDialogInit(const void* p,const void*){captured=*static_cast<const Param*>(p);return init_rc;}
int sceImeDialogGetStatus(){return status;}
int sceImeDialogGetResult(void* p){*static_cast<int32_t*>(p)=0;return 0;}
int sceImeDialogTerm(){return 0;}
}
int main(){sx::Ime ime;std::string text;assert(ime.open("Password",true));assert((captured.option&4)&&(captured.option&32)&&(captured.option&256));captured.buffer[0]='s';captured.buffer[1]='e';captured.buffer[2]=0;assert(ime.poll(&text)==sx::Ime::State::done&&text=="se");assert(captured.buffer[0]==0&&!ime.active());assert(ime.open("Username"));assert(captured.option==0);status=1;ime.poll(&text);captured.buffer[0]='x';status=0;assert(ime.poll(&text)==sx::Ime::State::cancelled&&!ime.active()&&captured.buffer[0]==0);std::cout<<"Password IME masks input, disables learning/resume and clears buffers\n";}
