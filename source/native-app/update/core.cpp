// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <openssl/evp.h>
#include <unistd.h>
#include <vector>
namespace nuvio_update {
namespace {
bool version(const std::string& s, std::array<unsigned,3>& out) {
    std::size_t p=0;
    for (int i=0;i<3;++i) {
        auto start=p; unsigned n=0;
        while(p<s.size() && s[p]>='0' && s[p]<='9') { if(n>999) return false; n=n*10+s[p++]-'0'; }
        if(p==start || n>9999 || (p-start>1 && s[start]=='0')) return false;
        out[i]=n;
        if(i<2 && (p>=s.size() || s[p++]!='.')) return false;
    }
    return p==s.size();
}
std::string string(const Json& j, const char* key) {
    return j.is_object() && j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : "";
}
bool same(const struct stat& a,const struct stat& b) {
    return a.st_dev==b.st_dev && a.st_ino==b.st_ino && a.st_size==b.st_size &&
        a.st_mtim.tv_sec==b.st_mtim.tv_sec && a.st_mtim.tv_nsec==b.st_mtim.tv_nsec &&
        a.st_ctim.tv_sec==b.st_ctim.tv_sec && a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
}
bool write_all(int fd,const void* data,std::size_t size) {
    auto* p=static_cast<const char*>(data);
    while(size) { auto n=write(fd,p,size); if(n<0 && errno==EINTR) continue; if(n<=0) return false; p+=n; size-=n; }
    return true;
}
}
bool newer(const std::string& a,const std::string& b) {
    std::array<unsigned,3> av{},bv{}; return version(a,av) && version(b,bv) && av>bv;
}
bool valid_release(const Release& r) {
    std::array<unsigned,3> v{};
    return version(r.version,v) && r.tag=="v"+r.version+"-native-experimental" &&
        r.url=="https://github.com/"+std::string(repository)+"/releases/download/"+r.tag+"/NuvioMaxd-"+r.version+"-native.ffpfsc" &&
        r.sha256.size()==64 && std::all_of(r.sha256.begin(),r.sha256.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}) &&
        r.size>=(1ull<<20) && r.size<=max_image_bytes;
}
Release select_release(const Json& list,const std::string& current) {
    Release best; if(!list.is_array()) return best;
    for(const auto& j:list) {
        try {
        const auto tag=string(j,"tag_name"); const std::string suffix="-native-experimental";
        if(j.value("draft",true) || tag.size()<=suffix.size()+1 || tag.front()!='v' || !tag.ends_with(suffix)) continue;
        const auto v=tag.substr(1,tag.size()-suffix.size()-1);
        if(!newer(v,best.version.empty()?current:best.version) || !j.contains("assets") || !j["assets"].is_array()) continue;
        // A source archive is required in the same release before advertising a binary.
        bool source=false; for(const auto& a:j["assets"]) if(string(a,"name")=="NuvioMaxd-"+v+"-source.tar.gz") source=true;
        if(!source) continue;
        for(const auto& a:j["assets"]) {
            if(string(a,"name")!="NuvioMaxd-"+v+"-native.ffpfsc" || !a.contains("size") || !a["size"].is_number_unsigned()) continue;
            auto digest=string(a,"digest");
            Release r{v,tag,string(a,"browser_download_url"),digest.starts_with("sha256:")?digest.substr(7):"",string(j,"body").substr(0,4000),a["size"].get<std::uint64_t>()};
            if(valid_release(r)) best=std::move(r);
        }
        } catch(const Json::exception&) { continue; }
    }
    return best;
}
bool managed_image_path(const std::string& p) {
    if(p.size()>1000 || !p.ends_with(".ffpfsc") || p.find('\0')!=std::string::npos || p.find("//")!=std::string::npos) return false;
    std::vector<std::string> roots{"/data/homebrew/"};
    for(int i=0;i<8;++i) roots.push_back("/mnt/usb"+std::to_string(i)+"/homebrew/");
    for(int i=0;i<2;++i) roots.push_back("/mnt/ext"+std::to_string(i)+"/homebrew/");
    if(std::none_of(roots.begin(),roots.end(),[&](const auto& root){return p.starts_with(root);})) return false;
    std::size_t pos=1;
    while(pos<p.size()) { auto end=p.find('/',pos); const auto c=p.substr(pos,end-pos); if(c.empty() || c=="." || c=="..") return false; if(end==std::string::npos) break; pos=end+1; }
    return true;
}
std::string installed_image(const Json& j) {
    if(!j.is_object() || j.value("status",-1)!=0 || string(j,"title_id")!=title_id || string(j,"source_type")!="image" ||
       !j.value("image_backed",false) || !j.value("source_available",false) || j.value("installed_pkg",true)) return "";
    const auto p=string(j,"path"); return managed_image_path(p)?p:"";
}
bool mount_released(const Json& j,const std::string& path) {
    if(!j.is_object() || j.value("status",-1)!=0 || !j.contains("images") || !j["images"].is_array()) return false;
    // A missing source is not proof of an unmount; refuse disconnected storage.
    for(const auto& i:j["images"]) if(string(i,"path")==path)
        return i.value("source_available",false) && !i.value("mapped",true) && !i.value("mounted",true);
    return false;
}
std::string sha256_file(int fd) {
    EVP_MD_CTX* ctx=EVP_MD_CTX_new(); if(!ctx) return "";
    bool ok=EVP_DigestInit_ex(ctx,EVP_sha256(),nullptr)==1;
    std::array<unsigned char,65536> buffer{}; off_t pos=0;
    while(ok) { auto n=pread(fd,buffer.data(),buffer.size(),pos); if(n<0 && errno==EINTR) continue; if(n<0) {ok=false;break;} if(n==0) break; ok=EVP_DigestUpdate(ctx,buffer.data(),n)==1; pos+=n; }
    unsigned char digest[EVP_MAX_MD_SIZE]; unsigned len=0;
    ok=ok && EVP_DigestFinal_ex(ctx,digest,&len)==1 && len==32; EVP_MD_CTX_free(ctx);
    if(!ok) return "";
    std::string out; const char* hex="0123456789abcdef";
    for(unsigned i=0;i<len;++i) {out+=hex[digest[i]>>4];out+=hex[digest[i]&15];} return out;
}
Transaction::~Transaction() {
    if(stage_fd_>=0) close(stage_fd_);
    if(dir_>=0) { if(!committed_ && !stage_.empty()) unlinkat(dir_,stage_.c_str(),0); close(dir_); }
    if(old_>=0) close(old_);
}
bool Transaction::begin(const std::string& path,std::string& error) {
    if(dir_>=0 || path.empty() || path.front()!='/') {error="Invalid install path";return false;}
    dir_=open("/",O_RDONLY|O_DIRECTORY); if(dir_<0) {error="Cannot open storage";return false;}
    std::size_t p=1;
    while(true) {
        const auto end=path.find('/',p); const auto component=path.substr(p,end-p);
        if(component.empty() || component=="." || component=="..") {error="Invalid storage path";return false;}
        if(end==std::string::npos) {name_=component;break;}
        int next=openat(dir_,component.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
        if(next<0) {error="Storage is unavailable or uses a symbolic link";return false;}
        close(dir_); dir_=next; p=end+1;
    }
    old_=openat(dir_,name_.c_str(),O_RDONLY|O_NOFOLLOW);
    if(old_<0 || fstat(old_,&original_) || !S_ISREG(original_.st_mode) || original_.st_size<0) {error="Cannot read the installed image";return false;}
    stage_=name_+".nuvio-"+std::to_string(getpid())+".part";
    stage_fd_=openat(dir_,stage_.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(stage_fd_<0) {stage_.clear();error="Cannot stage the update beside the existing image";return false;}
    return true;
}
bool Transaction::append(const void* data,std::size_t size,std::string& error) {
    if(stage_fd_<0 || verified_ || size>max_image_bytes-received_ || !write_all(stage_fd_,data,size)) {error="Could not write the update to storage";return false;}
    received_+=size; return true;
}
bool Transaction::verify(const Release& r,std::string& error) {
    std::uint64_t header=0;
    if(!valid_release(r) || received_!=r.size || pread(stage_fd_,&header,sizeof(header),0)!=sizeof(header) || header!=2 ||
       sha256_file(stage_fd_)!=r.sha256 || fsync(stage_fd_)) {error="Update size, container header or SHA-256 does not match the release";return false;}
    verified_=true;verified_release_=r;return true;
}
bool Transaction::unchanged() const {
    struct stat now{},opened{}; return fstatat(dir_,name_.c_str(),&now,AT_SYMLINK_NOFOLLOW)==0 &&
        fstat(old_,&opened)==0 && S_ISREG(now.st_mode) && same(original_,now) && same(original_,opened);
}
bool Transaction::commit(const std::function<bool()>& released,std::string& error) {
    if(!verified_ || committed_ || !unchanged() || !released()) {error="The installed image changed or is still mounted; original retained";return false;}
    struct stat stage_path{},stage_open{};
    if(fstatat(dir_,stage_.c_str(),&stage_path,AT_SYMLINK_NOFOLLOW) || fstat(stage_fd_,&stage_open) ||
       !S_ISREG(stage_path.st_mode) || !same(stage_path,stage_open) || std::uint64_t(stage_open.st_size)!=verified_release_.size ||
       sha256_file(stage_fd_)!=verified_release_.sha256) {error="Staged update changed; original retained";return false;}
    backup_=name_+".nuvio-backup-"+std::to_string(original_.st_ino)+"-"+std::to_string(original_.st_mtim.tv_sec)+".bak";
    int backup_fd=openat(dir_,backup_.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(backup_fd<0) {error="Cannot create the backup; original retained";return false;}
    std::array<char,65536> buf{}; off_t pos=0; bool ok=true;
    while(pos<original_.st_size) {auto n=pread(old_,buf.data(),std::min<off_t>(buf.size(),original_.st_size-pos),pos); if(n<0 && errno==EINTR) continue; if(n<=0 || !write_all(backup_fd,buf.data(),n)) {ok=false;break;} pos+=n;}
    auto old_hash=sha256_file(old_);
    ok=ok && !old_hash.empty() && sha256_file(backup_fd)==old_hash && fsync(backup_fd)==0 && unchanged(); close(backup_fd);
    if(!ok) {unlinkat(dir_,backup_.c_str(),0);error="Backup verification failed; original retained";return false;}
    if(fsync(dir_) || !released() || !unchanged() || sha256_file(stage_fd_)!=verified_release_.sha256) {error="Storage could not be synced, image changed or is still mounted; original retained";return false;}
    if(fchmod(stage_fd_,original_.st_mode&0777) || fsync(stage_fd_) || renameat(dir_,stage_.c_str(),dir_,name_.c_str())) {error="Could not replace the image; original and backup retained";return false;}
    committed_=true;
    // A post-rename durability error cannot safely be called a failed install.
    if(fsync(dir_)) error="Update installed, but storage sync failed. Keep the backup.";
    return true;
}
}
