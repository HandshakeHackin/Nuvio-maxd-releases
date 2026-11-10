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
// Fault injection exercises title-sandbox failures on the real scanner.
static std::string fault_root;
static bool fail_root_meta=false,fail_entry_meta=false,deny_open=false,fail_read=false,unknown_type=false;
extern "C" int __real_lstat(const char*,struct stat*);
extern "C" DIR* __real_opendir(const char*);
extern "C" struct dirent* __real_readdir(DIR*);
extern "C" int __wrap_lstat(const char* path,struct stat* st){
  const std::string p=path;
  if((fail_root_meta&&p==fault_root)||(fail_entry_meta&&p.starts_with(fault_root+"/"))){errno=EPERM;return -1;}
  return __real_lstat(path,st);
}
extern "C" DIR* __wrap_opendir(const char* path){if(deny_open&&path==fault_root){errno=EACCES;return nullptr;}return __real_opendir(path);}
extern "C" struct dirent* __wrap_readdir(DIR* d){if(fail_read){errno=EIO;return nullptr;}auto* e=__real_readdir(d);if(e&&unknown_type)e->d_type=DT_UNKNOWN;return e;}
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
  fault_root=(root/"usb").string();std::vector<live_usb::MountDiagnostics> diagnostics;
  fs::create_directories(root/"empty");
  auto missing=(root/"missing").string();
  live_usb::scan({missing,(root/"empty").string(),fault_root},&diagnostics);
  assert(diagnostics.size()==3&&diagnostics[0].missing&&diagnostics[0].open_errno==ENOENT);
  assert(diagnostics[1].entries==0&&diagnostics[1].directories==1);
  assert(diagnostics[1].summary(1).find("state=empty")!=std::string::npos);
  deny_open=true;live_usb::scan({fault_root},&diagnostics);deny_open=false;
  assert(diagnostics[0].access_denied&&diagnostics[0].open_errno==EACCES);
  fail_root_meta=true;files=live_usb::scan({fault_root},&diagnostics);fail_root_meta=false;
  assert(files.size()==1&&diagnostics[0].metadata_errno==EPERM&&diagnostics[0].directories==2);
  fail_entry_meta=true;files=live_usb::scan({fault_root},&diagnostics);
  assert(files.size()==2&&diagnostics[0].metadata_failures>0&&diagnostics[0].type_fallbacks>0); // includes huge file as unverified candidate
  error.clear();assert(live_usb::read_file({fault_root,"nested/TV.M3U8"},nullptr,error).empty()&&!error.empty());
  unknown_type=true;assert(live_usb::scan({fault_root},&diagnostics).empty());
  unknown_type=false;fail_entry_meta=false;
  unknown_type=true;assert(live_usb::scan({fault_root},&diagnostics).size()==1);unknown_type=false;
  fail_read=true;live_usb::scan({fault_root},&diagnostics);fail_read=false;
  assert(diagnostics[0].read_errno==EIO&&diagnostics[0].summary(0).find("enumeration-error")!=std::string::npos);
  assert(live_usb::scan({fault_root},&diagnostics).size()==1&&diagnostics[0].read_errno==0); // lstat errno must not become readdir error
  assert(live_usb::scan({fault_root},&diagnostics,&cancelled).empty()&&diagnostics[0].cancelled);
  auto diagnostic=diagnostics[0].summary(0);assert(diagnostic.find(root.string())==std::string::npos&&diagnostic.find("TV.M3U8")==std::string::npos);
  fs::create_directories(root/"deep/a/b/c/d/e");
  std::ofstream(root/"deep/a/b/c/d/allowed.m3u")<<body;
  std::ofstream(root/"deep/a/b/c/d/e/excluded.m3u")<<body;
  assert(live_usb::scan({(root/"deep").string()}).size()==1);
  fs::create_directories(root/"many");
  for(int i=0;i<520;++i)std::ofstream(root/"many"/(std::to_string(i)+".m3u"))<<body;
  assert(live_usb::scan({(root/"many").string()},&diagnostics).size()==512&&diagnostics[0].limited);
  fs::create_directories(root/"entries");for(int i=0;i<8200;++i)std::ofstream(root/"entries"/std::to_string(i));
  assert(live_usb::scan({(root/"entries").string(),missing},&diagnostics).empty());
  assert(diagnostics[0].limited&&diagnostics[0].entries<=8192&&diagnostics[1].missing);
  std::cout<<"USB diagnostics: missing/access/empty mounts, metadata fallback, unknown types, enumeration errors, privacy, cancellation and global count/depth limits passed\n";
  fs::remove_all(root);std::cout<<"USB playlists: scan, symlink/path rejection, limits, HLS rejection, atomic replacement, cancellation, permissions, profile isolation and unplugged reload passed\n";
}
