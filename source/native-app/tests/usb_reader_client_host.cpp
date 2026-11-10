#include "../gl/usb_reader.hpp"
#include "../usb/protocol.hpp"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
const char* kUserAgent="USB-reader-fixture";
void http_setup_handle(void*){}
const std::string& http_ca_bundle(){static std::string s;return s;}
static std::string reply,sent;static size_t offset=0,fragment=3;static int opens=0,closes=0;
extern "C" {
int sceNetSocket(const char*,int,int,int){++opens;return 42;}
int sceNetConnect(int fd,const void* raw,unsigned size){const auto* p=static_cast<const unsigned char*>(raw);assert(fd==42&&size==16&&p[0]==16&&p[1]==2&&p[2]==(9021>>8)&&p[3]==(9021&255)&&p[4]==127&&p[5]==0&&p[6]==0&&p[7]==1);return 0;}
int sceNetSetsockopt(int,int,int,const void*,unsigned){return 0;}
int sceNetSend(int fd,const void* p,size_t n,int){assert(fd==42);n=std::min(n,fragment);sent.append(static_cast<const char*>(p),n);return int(n);}
int sceNetRecv(int fd,void* p,size_t n,int){assert(fd==42);n=std::min({n,fragment,reply.size()-offset});std::memcpy(p,reply.data()+offset,n);offset+=n;return int(n);}
int sceNetSocketClose(int fd){assert(fd==42);++closes;return 0;}
int __wrap_dup(int){std::abort();} DIR* __wrap_fdopendir(int){std::abort();} int __wrap_fstatat(int,const char*,struct stat*,int){std::abort();} int __wrap_openat(int,const char*,int,...){std::abort();}
}
using usb_reader::Json;
static std::string row(Json j){return j.dump()+"\n";}
static std::string hello(){return row({{"kind","ready"},{"protocol",1},{"title_id","PPSA99288"}});}
static void setup(std::string s,size_t f=3){reply=std::move(s);offset=0;sent.clear();fragment=f;}
int main(){
 namespace fs=std::filesystem;char tmp[]="/tmp/usb-reader-XXXXXX";assert(mkdtemp(tmp));fs::path root=tmp;std::ofstream(root/"reader.elf")<<"fixture-payload";setenv("NUVIO_USB_READER_TEST_HELPER",(root/"reader.elf").c_str(),1);fs::create_directory(root/"profile");fs::create_directory(root/"other");std::atomic<bool> cancel=false;
 auto manifest=hello();for(int i=0;i<8;++i){live_usb::MountDiagnostics d;d.directories=i==0?1:0;d.open_errno=i?ENOENT:0;d.metadata_errno=i?ENOENT:0;manifest+=row(usb_reader::diagnostic(d,i));}
 manifest+=row({{"kind","file"},{"mount",0},{"path","nested/TV.M3U8"}})+row({{"kind","done"},{"files",1}});
 setup(manifest);auto scan=sx::usb_reader_client::scan(cancel);assert(scan.error.empty()&&scan.files.size()==1&&scan.files[0].reader&&scan.mounts.size()==8);assert(scan.files[0].root=="/mnt/usb0");assert(sent.find("\"op\":\"scan\"")!=std::string::npos);assert(opens==closes);
 const std::string body="#EXTM3U\n#EXTINF:-1,News\nhttps://example.test/live.m3u8\n";
 setup(hello()+row({{"kind","body"},{"bytes",body.size()}})+body,4096);auto result=sx::usb_reader_client::import_playlist(scan.files[0],(root/"profile").string(),"123",cancel);assert(result.error.empty()&&result.channels.size()==1);
 auto saved=live_usb::cache_path((root/"profile").string(),"123");struct stat st{};assert(!stat(saved.c_str(),&st)&&(st.st_mode&0777)==0600);assert(live_usb::load((root/"profile").string(),"123",nullptr).channels.size()==1);assert(!live_usb::load((root/"other").string(),"123",nullptr).error.empty());
 for(auto packet:{hello()+row({{"kind","body"},{"bytes",body.size()}})+body.substr(0,5),hello()+row({{"kind","body"},{"bytes",(128u<<20)+1}}),hello()+row({{"kind","body"},{"bytes",0}}),hello()+row({{"kind","body"},{"bytes",3}})+"bad",row({{"kind","ready"},{"protocol",99},{"title_id","PPSA99288"}})}){
  setup(packet);assert(!sx::usb_reader_client::import_playlist(scan.files[0],(root/"profile").string(),"123",cancel).error.empty());assert(live_usb::load((root/"profile").string(),"123",nullptr).channels.size()==1);
 }
 setup(manifest);cancel=true;assert(!sx::usb_reader_client::import_playlist(scan.files[0],(root/"profile").string(),"123",cancel).error.empty());cancel=false;
 const int before=opens;assert(!sx::usb_reader_client::import_playlist({"/data","x.m3u",true},(root/"profile").string(),"123",cancel).error.empty());assert(opens==before);
 setup(hello()+std::string(8192,'x')+"\n");assert(!sx::usb_reader_client::scan(cancel).error.empty());
 setup(manifest.substr(0,manifest.size()-row({{"kind","done"},{"files",1}}).size()));assert(!sx::usb_reader_client::scan(cancel).error.empty());
 auto duplicate=manifest.substr(0,manifest.size()-row({{"kind","done"},{"files",1}}).size())+row({{"kind","file"},{"mount",0},{"path","nested/TV.M3U8"}})+row({{"kind","done"},{"files",2}});setup(duplicate);assert(!sx::usb_reader_client::scan(cancel).error.empty());

 auto prefix=hello();for(int i=0;i<8;++i)prefix+=row(usb_reader::diagnostic(live_usb::MountDiagnostics{},i));
 auto many=prefix;for(int i=0;i<512;++i)many+=row({{"kind","file"},{"mount",0},{"path",std::to_string(i)+".m3u"}});
 setup(many+row({{"kind","done"},{"files",512}}),4096);assert(sx::usb_reader_client::scan(cancel).files.size()==512);
 setup(many+row({{"kind","file"},{"mount",0},{"path","overflow.m3u"}})+row({{"kind","done"},{"files",513}}),4096);assert(!sx::usb_reader_client::scan(cancel).error.empty());
 setup(prefix+row({{"kind","file"},{"mount",0},{"path","../private.m3u"}})+row({{"kind","done"},{"files",1}}));assert(!sx::usb_reader_client::scan(cancel).error.empty());
 const std::string hls="#EXTM3U\n#EXT-X-TARGETDURATION:6\nsegment.ts\n";setup(hello()+row({{"kind","body"},{"bytes",hls.size()}})+hls);assert(!sx::usb_reader_client::import_playlist(scan.files[0],(root/"profile").string(),"123",cancel).error.empty());assert(live_usb::load((root/"profile").string(),"123",nullptr).channels.size()==1);
 assert(opens==closes);fs::remove_all(root);std::cout<<"USB reader: bounded protocol, loopback loader, fragmented/buffered transfers, invalid replies, cancellation, validation, private atomic copies and profile isolation passed\n";
}
