#include "live_usb.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
const char* kUserAgent="USB-Playlist-Fixture";
void http_setup_handle(void*){}
const std::string& http_ca_bundle(){static std::string empty;return empty;}
// Direct use of these SDK syscall stubs would crash before a file list appears.
extern "C" int __wrap_dup(int){std::abort();}
extern "C" DIR* __wrap_fdopendir(int){std::abort();}
extern "C" int __wrap_fstatat(int,const char*,struct stat*,int){std::abort();}
extern "C" int __wrap_openat(int,const char*,int,...){std::abort();}
int main(){
  namespace fs=std::filesystem;char dir[]="/tmp/maxd-usb-XXXXXX";assert(mkdtemp(dir));const fs::path root=dir;
  fs::create_directories(root/"usb/nested");fs::create_directories(root/"profile");fs::create_directories(root/"other");
  const std::string body="#EXTM3U\n#EXTINF:-1 tvg-id=\"news\",News\nhttps://example.test/live.m3u8\n";
  std::ofstream(root/"usb/nested/TV.M3U8")<<body;
  fs::create_symlink(root/"usb/nested/TV.M3U8",root/"usb/link.m3u");
  fs::create_directory_symlink(root/"usb/nested",root/"usb/linked");
  auto files=live_usb::scan({(root/"usb").string()});assert(files.size()==1&&files[0].path=="nested/TV.M3U8");
  auto result=live_usb::import_playlist(files[0],(root/"profile").string(),"123",nullptr);assert(result.error.empty()&&result.channels.size()==1);
  struct stat st{};assert(!stat(live_usb::cache_path((root/"profile").string(),"123").c_str(),&st));assert((st.st_mode&0777)==0600);
  fs::rename(root/"usb",root/"removed");assert(live_usb::load((root/"profile").string(),"123",nullptr).channels.size()==1);
  assert(!live_usb::load((root/"other").string(),"123",nullptr).error.empty());
  assert(!live_usb::import_playlist(files[0],(root/"profile").string(),"123",nullptr).error.empty());
  fs::rename(root/"removed",root/"usb");std::ofstream(root/"usb/nested/TV.M3U8")<<"#EXTM3U\n#EXT-X-TARGETDURATION:6\nsegment.ts\n";
  assert(!live_usb::import_playlist(files[0],(root/"profile").string(),"123",nullptr).error.empty());
  assert(live_usb::load((root/"profile").string(),"123",nullptr).channels.size()==1);
  std::ofstream(root/"usb/nested/TV.M3U8")<<body;std::atomic<bool> cancelled=true;
  assert(!live_usb::import_playlist(files[0],(root/"profile").string(),"123",&cancelled).error.empty());
  assert(live_usb::cache_path(root.string(),"../escape").empty());
  std::string error;assert(live_usb::read_file({(root/"usb").string(),"../profile"},nullptr,error).empty()&&!error.empty());
  error.clear();assert(live_usb::read_file({(root/"usb").string(),"link.m3u"},nullptr,error).empty()&&!error.empty());
  int fd=open((root/"usb/huge.m3u").c_str(),O_CREAT|O_WRONLY,0600);assert(fd>=0);assert(!ftruncate(fd,live_tv::max_bytes+1));close(fd);
  assert(live_usb::scan({(root/"usb").string()}).size()==1);
  error.clear();assert(live_usb::read_file({(root/"usb").string(),"linked/TV.M3U8"},nullptr,error).empty()&&!error.empty());
  assert(live_usb::scan({(root/"usb/missing").string()}).empty());
  fs::remove_all(root);std::cout<<"USB playlists: scan, symlink/path rejection, limits, HLS rejection, atomic replacement, cancellation, permissions, profile isolation and unplugged reload passed\n";
}
