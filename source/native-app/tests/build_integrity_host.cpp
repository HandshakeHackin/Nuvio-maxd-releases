#include "build_integrity.h"
#include "release_public_key.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc,char** argv) {
 if(argc==2&&std::string(argv[1])=="embedded-key") {
  return std::fwrite(build_integrity::official_key.data(),1,32,stdout)==32?0:2;
 }
 if(argc<4)return 2;
 std::array<unsigned char,32> key{};
 FILE* file=std::fopen(argv[3],"rb");if(!file)return 2;
 auto count=std::fread(key.data(),1,key.size(),file);std::fclose(file);if(count!=key.size())return 2;
 const auto mode=argc>4?std::string(argv[4]):"";
 std::atomic<bool> cancel=mode=="cancel";
 auto result=build_integrity::verify(argv[1],argv[2],key,&cancel,mode=="deadline"?0:30000);
 std::printf("{\"status\":%d,\"reason\":%d,\"files\":%zu,\"credits\":%s}\n",int(result.status),int(result.reason),result.files_checked,result.credits_checked?"true":"false");
}
