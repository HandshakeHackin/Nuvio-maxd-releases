// Bounded playlist discovery; import rejects symlinks and rechecks identities.
#pragma once
#include "live_tv.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
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
// Only counts, fixed states and numeric errno values leave the scanner. Never
// include paths: folder/playlist names can themselves contain private data.
struct MountDiagnostics {
  int metadata_rc=0, metadata_errno=0, open_errno=0, first_metadata_errno=0, first_directory_errno=0, read_errno=0;
  size_t directories=0, entries=0, candidates=0, metadata_failures=0, type_fallbacks=0, rejected=0, directory_failures=0;
  bool missing=false, access_denied=false, non_directory=false, limited=false, cancelled=false;
  std::string summary(size_t index) const {
    const char* state=cancelled?"cancelled":access_denied?"access-denied":missing?"missing":non_directory?"not-directory":open_errno?"open-error":
      read_errno?"enumeration-error":directory_failures?"directory-error":limited?"limit":metadata_failures?"metadata-error":entries==0?"empty":"listed";
    char text[640];
    snprintf(text,sizeof(text),"USB playlist mount %zu: state=%s root_meta_rc=%d root_meta_errno=%d open_errno=%d dirs=%zu entries=%zu candidates=%zu metadata_failures=%zu first_meta_errno=%d type_fallbacks=%zu rejected=%zu directory_failures=%zu first_dir_errno=%d read_errno=%d limited=%d",
      index,state,metadata_rc,metadata_errno,open_errno,directories,entries,candidates,metadata_failures,first_metadata_errno,type_fallbacks,rejected,directory_failures,first_directory_errno,read_errno,int(limited));
    return text;
  }
};
inline void metadata_failed(MountDiagnostics& d,int error) {
  ++d.metadata_failures;if(!d.first_metadata_errno)d.first_metadata_errno=error?error:EIO;
}
inline bool stopped(const std::atomic<bool>* cancel) {return cancel&&cancel->load();}
inline void walk(const std::string& root,const std::string& prefix,int depth,size_t& visited,std::vector<File>& out,
                 MountDiagnostics& d,const std::atomic<bool>* cancel) {
  if(stopped(cancel)){d.cancelled=true;return;}
  const auto directory=root+(prefix.empty()?"":"/"+prefix.substr(0,prefix.size()-1));
  struct stat before{};errno=0;const int rc=lstat(directory.c_str(),&before);const int meta_errno=rc?errno:0;
  if(depth==0){d.metadata_rc=rc;d.metadata_errno=meta_errno;}
  // Reject observed symlinks and non-directories, but a failed metadata call
  // alone does not prove the directory cannot be enumerated in a title sandbox.
  if(rc==0 && !S_ISDIR(before.st_mode)){++d.rejected;d.non_directory=depth==0;return;}
  if(rc)metadata_failed(d,meta_errno);
  errno=0;DIR* dir=opendir(directory.c_str());const int open_errno=dir?0:(errno?errno:EIO);
  if(!dir){++d.directory_failures;if(!d.first_directory_errno)d.first_directory_errno=open_errno;
    if(depth==0){d.open_errno=open_errno;d.missing=meta_errno==ENOENT&&open_errno==ENOENT;d.access_denied=open_errno==EACCES||open_errno==EPERM;}return;}
  ++d.directories;
  for(;;) {
    if(stopped(cancel)){d.cancelled=true;break;}
    if(visited>=8192 || out.size()>=512){d.limited=true;break;}
    // Reset immediately before readdir: lstat may have left errno nonzero.
    errno=0;auto* e=readdir(dir);if(!e){if(errno&&!d.read_errno)d.read_errno=errno;break;}
    ++visited;
    const std::string name=e->d_name;if(name=="."||name=="..")continue;
    ++d.entries;
    if(name.empty()||name.find('/')!=std::string::npos){++d.rejected;continue;}
    const auto path=prefix+name;const auto full=root+"/"+path;
    struct stat st{};errno=0;const int result=lstat(full.c_str(),&st);
    unsigned type=DT_UNKNOWN;
    if(result==0){if(S_ISREG(st.st_mode))type=DT_REG;else if(S_ISDIR(st.st_mode))type=DT_DIR;else {++d.rejected;continue;}}
    else {
      metadata_failed(d,errno);
      // d_type is only a picker hint. Unknown entries and symlinks fail closed;
      // import still requires every component's metadata and open identity.
      type=e->d_type;
      if(type!=DT_REG && type!=DT_DIR){++d.rejected;continue;}
      ++d.type_fallbacks;
    }
    if(type==DT_REG && playlist(name)) {
      if(result==0 && (st.st_size<=0||uint64_t(st.st_size)>live_tv::max_bytes)){++d.rejected;continue;}
      out.push_back({root,path});++d.candidates;
    } else if(type==DT_DIR && depth<4)walk(root,path+"/",depth+1,visited,out,d,cancel);
  }
  if(closedir(dir)&&!d.read_errno)d.read_errno=errno?errno:EIO;
}
inline std::vector<File> scan(const std::vector<std::string>& roots,std::vector<MountDiagnostics>* diagnostics=nullptr,
                             const std::atomic<bool>* cancel=nullptr) {
  std::vector<File> out;size_t visited=0;
  if(diagnostics)diagnostics->clear();
  for(const auto& root:roots){MountDiagnostics d;walk(root,"",0,visited,out,d,cancel);if(diagnostics)diagnostics->push_back(d);}
  std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.root+a.path<b.root+b.path;});return out;
}
// Validate every component and retain identities for rechecking after open.
// These are imported libc functions; the SDK's *at/dup stubs issue raw syscalls.
struct Component {std::string path;struct stat identity;};
inline bool same_file(const struct stat& a,const struct stat& b){return a.st_dev==b.st_dev && a.st_ino==b.st_ino && (a.st_mode&S_IFMT)==(b.st_mode&S_IFMT);}
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
  std::vector<Component> chain;struct stat root{};
  if(lstat(file.root.c_str(),&root)||!S_ISDIR(root.st_mode)){error="USB drive is unavailable.";return {};}
  chain.push_back({file.root,root});std::string full=file.root;size_t start=0;
  for(;;){const auto end=file.path.find('/',start);const auto part=file.path.substr(start,end-start);
    if(part.empty()||part=="."||part==".."){error="Invalid playlist path.";return {};}
    full+="/"+part;struct stat st{};
    if(lstat(full.c_str(),&st) || (end==std::string::npos?!S_ISREG(st.st_mode):!S_ISDIR(st.st_mode))){error="Playlist is unavailable or contains a symlink. Scan again.";return {};}
    chain.push_back({full,st});if(end==std::string::npos)break;start=end+1;
  }
  char* canonical=realpath(full.c_str(),nullptr);
  const bool canonical_matches=canonical && full==canonical;free(canonical);
  if(!canonical_matches){error="Playlist path changed or contains a symlink. Scan again.";return {};}
  int fd=open(full.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
  if(fd<0){error="Playlist is unavailable. Reconnect the USB drive and scan again.";return {};}
  struct stat opened{};bool unchanged=fstat(fd,&opened)==0 && same_file(opened,chain.back().identity);
  for(const auto& component:chain){struct stat now{};if(lstat(component.path.c_str(),&now)||!same_file(now,component.identity))unchanged=false;}
  if(!unchanged){close(fd);error="USB playlist changed while opening. Scan again.";return {};}

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
