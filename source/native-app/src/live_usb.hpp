// Bounded local playlist import. Symlinks are never followed on removable media.
#pragma once
#include "live_tv.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace live_usb {
struct File { std::string root, path; };
inline bool playlist(std::string name) {
  std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
  return name.ends_with(".m3u") || name.ends_with(".m3u8");
}
inline void walk(int fd,const std::string& root,const std::string& prefix,int depth,size_t& visited,std::vector<File>& out) {
  DIR* dir=fdopendir(dup(fd));if(!dir)return;
  while(auto* e=readdir(dir)) {
    if(++visited>8192 || out.size()>=512)break;
    const std::string name=e->d_name;if(name=="."||name=="..")continue;
    struct stat st{};if(fstatat(fd,name.c_str(),&st,AT_SYMLINK_NOFOLLOW))continue;
    const auto path=prefix+name;
    if(S_ISREG(st.st_mode)&&playlist(name)&&st.st_size>0&&uint64_t(st.st_size)<=live_tv::max_bytes)out.push_back({root,path});
    else if(S_ISDIR(st.st_mode)&&depth<4){int child=openat(fd,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(child>=0){walk(child,root,path+"/",depth+1,visited,out);close(child);}}
  }
  closedir(dir);
}
inline std::vector<File> scan(const std::vector<std::string>& roots) {
  std::vector<File> out;size_t visited=0;
  for(const auto& root:roots){int fd=open(root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(fd<0)continue;walk(fd,root,"",0,visited,out);close(fd);}
  std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.root+a.path<b.root+b.path;});return out;
}
inline std::string read_fd(int fd,const std::atomic<bool>* cancel,std::string& error) {
  struct stat st{};std::string body;
  if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||uint64_t(st.st_size)>live_tv::max_bytes){error="Playlist is empty, unreadable or exceeds 128 MiB.";return {};}
  char buffer[65536];
  for(;;){if(cancel&&*cancel){error="Playlist import cancelled.";return {};}
    const auto n=read(fd,buffer,sizeof(buffer));if(n==0)break;if(n<0){error="Could not read playlist. Check the USB drive.";return {};}
    if(body.size()+size_t(n)>live_tv::max_bytes){error="Playlist exceeds 128 MiB.";return {};}
    body.append(buffer,size_t(n));
  }return body;
}
inline std::string read_file(const File& file,const std::atomic<bool>* cancel,std::string& error) {
  int fd=open(file.root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(fd<0){error="USB drive is unavailable.";return {};}
  size_t start=0;
  for(;;){const auto end=file.path.find('/',start);const auto part=file.path.substr(start,end-start);
    if(part.empty()||part=="."||part==".."){close(fd);error="Invalid playlist path.";return {};}
    const int next=openat(fd,part.c_str(),O_RDONLY|O_NOFOLLOW|(end==std::string::npos?O_NONBLOCK:O_DIRECTORY));close(fd);fd=next;
    if(fd<0){error="Playlist is unavailable. Reconnect the USB drive and scan again.";return {};}
    if(end==std::string::npos)break;start=end+1;
  }
  auto body=read_fd(fd,cancel,error);close(fd);return body;
}
inline std::string cache_path(const std::string& profile,const std::string& id){
  if(id.empty()||id.size()>40||id.find_first_not_of("0123456789")!=std::string::npos)return {};
  return profile+"/usb-playlist-"+id+".m3u";
}
inline live_tv::Result load(const std::string& profile,const std::string& id,const std::atomic<bool>* cancel){
  std::string error;const auto path=cache_path(profile,id);if(path.empty())return {{},"Invalid imported playlist."};
  int fd=open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return {{},"Imported playlist is missing. Import it again from USB."};
  auto body=read_fd(fd,cancel,error);close(fd);if(!error.empty())return {{},error};return live_tv::parse(body,"",cancel);
}
inline live_tv::Result import_playlist(const File& file,const std::string& profile,const std::string& id,const std::atomic<bool>* cancel){
  std::string error;auto body=read_file(file,cancel,error);if(!error.empty())return {{},error};
  auto result=live_tv::parse(body,"",cancel);if(!result.error.empty())return result;
  const auto path=cache_path(profile,id);if(path.empty())return {{},"Invalid imported playlist."};
  static std::atomic<unsigned> sequence{0};
  const auto temp=path+".tmp-"+std::to_string(getpid())+"-"+std::to_string(sequence++);int fd=open(temp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
  if(fd<0)return {{},"Could not save imported playlist. Try again."};
  size_t at=0;bool ok=true;while(at<body.size()){if(cancel&&*cancel){ok=false;break;}auto n=write(fd,body.data()+at,body.size()-at);if(n<=0){ok=false;break;}at+=size_t(n);}
  if(cancel&&*cancel)ok=false;
  if(ok)ok=fsync(fd)==0;if(close(fd))ok=false;if(cancel&&*cancel)ok=false;if(ok)ok=rename(temp.c_str(),path.c_str())==0;
  if(!ok){unlink(temp.c_str());return {{},"Could not save imported playlist; previous copy was kept."};}return result;
}
}
