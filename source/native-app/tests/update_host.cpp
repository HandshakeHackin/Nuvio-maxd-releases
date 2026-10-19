// SPDX-License-Identifier: GPL-3.0-or-later
#include "../update/core.hpp"
#include "../update/release_wait.hpp"
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
using namespace nuvio_update;
namespace fs=std::filesystem;
int tests=0;bool fail_rename=false,fail_sync=false;
extern "C" int __real_renameat(int,const char*,int,const char*);
extern "C" int __wrap_renameat(int a,const char* b,int c,const char* d){if(fail_rename){errno=EIO;return -1;}return __real_renameat(a,b,c,d);}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int a){if(fail_sync){errno=EIO;return -1;}return __real_fsync(a);}
void check(bool b,const char* name){++tests;if(!b){std::cerr<<"FAIL: "<<name<<"\n";std::exit(1);}std::cout<<"update: "<<name<<"\n";}
std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void put(const fs::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);f<<s;}
struct Fixture {
    fs::path root,image;std::string bytes,error;Release release;
    Fixture(){char d[]="/tmp/nuvio-update-XXXXXX";root=mkdtemp(d);image=root/"Nuvio.ffpfsc";put(image,"original-image");
        bytes.assign(1<<20,'N');std::fill(bytes.begin(),bytes.begin()+8,0);bytes[0]=2;auto p=root/"candidate";put(p,bytes);int fd=open(p.c_str(),O_RDONLY);
        release={"0.5.9","v0.5.9-native-experimental","https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/download/v0.5.9-native-experimental/NuvioMaxd-0.5.9-native.ffpfsc",sha256_file(fd),"",bytes.size()};close(fd);
    }
    ~Fixture(){fs::remove_all(root);}
    bool stage(Transaction& t){return t.begin(image,error) && t.append(bytes.data(),bytes.size(),error) && t.verify(release,error);}
};
Json asset_release(const Release& r){return Json{{"draft",false},{"tag_name",r.tag},{"body","release notes"},{"assets",Json::array({Json{{"name","NuvioMaxd-"+r.version+"-native.ffpfsc"},{"size",r.size},{"digest","sha256:"+r.sha256},{"browser_download_url",r.url}},Json{{"name","NuvioMaxd-"+r.version+"-source.tar.gz"}}})}};}
int main(){
    check(newer("0.5.10","0.5.9") && !newer("0.5.9","0.5.10"),"numeric versions and downgrade refusal");
    check(!newer("0.5.08","0.5.7") && !newer("0.5.9junk","0.5.7"),"malformed version refusal");
    check(newer("0.5.34.1","0.5.34") && newer("0.5.35","0.5.34.1") && !newer("0.5.34","0.5.34.1"),"temporary revision ordering and upgrade to next release");
    check(!newer("0.5.34.0","0.5.34") && !newer("0.5.34.01","0.5.34") && !newer("0.5.34.1.2","0.5.34"),"canonical temporary revision parsing");
    Fixture f;auto j=asset_release(f.release);
    check(select_release(Json::array({j}),"0.5.7").version=="0.5.9","new native image with matching source");
    auto malformed=j;malformed["draft"]="wrong type";check(select_release(Json::array({malformed,j}),"0.5.7").version=="0.5.9","malformed release cannot hide a valid later release");
    auto no_source=j;no_source["assets"].erase(1);check(select_release(Json::array({no_source}),"0.5.7").version.empty(),"no binary advertised without matching source");
    auto wrong=j;wrong["assets"][0]["browser_download_url"]="http://example.com/image.ffpfsc";check(select_release(Json::array({wrong}),"0.5.7").version.empty(),"wrong repository or plaintext URL refusal");
    wrong=j;wrong["assets"][0]["digest"]="";check(select_release(Json::array({wrong}),"0.5.7").version.empty(),"missing digest refusal");
    wrong=j;wrong["assets"][0]["size"]=max_image_bytes+1;check(select_release(Json::array({wrong}),"0.5.7").version.empty(),"oversized image refusal");
    check(managed_image_path("/mnt/usb0/homebrew/Nuvio.ffpfsc") && !managed_image_path("/mnt/usb0/homebrew/../app.ffpfsc") && !managed_image_path("/data/games/nuvio.ffpfsc"),"managed homebrew paths only");
    Json info={{"status",0},{"title_id",title_id},{"source_type","image"},{"image_backed",true},{"source_available",true},{"installed_pkg",false},{"path","/data/homebrew/Nuvio.ffpfsc"}};
    check(installed_image(info)=="/data/homebrew/Nuvio.ffpfsc","resolve exact title's physical image");
    info["title_id"]="PPSA00000";check(installed_image(info).empty(),"wrong title refusal");
    Json images={{"status",0},{"images",Json::array({Json{{"path","/data/homebrew/Nuvio.ffpfsc"},{"source_available",true},{"mapped",true},{"mounted",true}}})}};
    check(!mount_released(images,"/data/homebrew/Nuvio.ffpfsc"),"persistent backing mount refusal");
    images["images"][0]["mapped"]=false;images["images"][0]["mounted"]=false;
    check(mount_released(images,"/data/homebrew/Nuvio.ffpfsc"),"fully released image accepted");
    images["images"][0]["source_available"]=false;check(!mount_released(images,"/data/homebrew/Nuvio.ffpfsc"),"disconnected storage is not an unmount");
    const auto image_path="/data/homebrew/Nuvio.ffpfsc";
    images["images"][0]["source_available"]=true;
    images["images"][0]["mapped"]=true;
    check(release_state({{"status",0}},images,image_path)==ReleaseState::retry,"successful runtime unmount still waits for backing image");
    images["images"][0]["mapped"]=false;
    check(release_state({{"status",16}},images,image_path)==ReleaseState::retry,"busy title cannot authorize replacement even with released snapshot");
    check(release_state({{"status",0}},images,image_path)==ReleaseState::released,"later independent snapshot completes release wait");
    check(release_state({{"status",5}},images,image_path)==ReleaseState::refused,"terminal unmount error is not retried");
    check(release_state(Json(),images,image_path)==ReleaseState::retry,"transient API outage is retried without replacing");
    check(release_state({{"status","bad"}},images,image_path)==ReleaseState::retry,"malformed status cannot authorize replacement");
    check(release_state({{"status",0}},{{"status",0},{"images",Json::array()}},image_path)==ReleaseState::retry,"missing image is not proof of release");
    images["images"][0]["source_available"]=false;
    check(release_state({{"status",0}},images,image_path)==ReleaseState::retry,"storage disconnection cannot authorize replacement");
    {
        Fixture x;{Transaction t;check(x.stage(t) && read(x.image)=="original-image","verified staging leaves original installed");}
        check(read(x.image)=="original-image" && !fs::exists(x.image.string()+".nuvio-"+std::to_string(getpid())+".part"),"cancellation removes only the owned stage");
    }
    {Fixture x;Transaction t;t.begin(x.image,x.error);t.append(x.bytes.data(),100,x.error);check(!t.verify(x.release,x.error) && read(x.image)=="original-image","truncated transfer refusal");}
    {Fixture x;Transaction t;t.begin(x.image,x.error);x.bytes.back()='X';t.append(x.bytes.data(),x.bytes.size(),x.error);check(!t.verify(x.release,x.error),"corrupt image SHA-256 refusal");}
    {Fixture x;Transaction t;x.stage(t);check(!t.commit([]{return false;},x.error) && read(x.image)=="original-image","busy image retains original");}
    {Fixture x;Transaction t;x.stage(t);put(x.image,"externally changed");check(!t.commit([]{return true;},x.error) && read(x.image)=="externally changed","external install changes are preserved");}
    {Fixture x;fs::remove(x.image);fs::create_symlink(x.root/"candidate",x.image);Transaction t;check(!t.begin(x.image,x.error),"symlink image refusal");}
    {Fixture x;fs::create_directory_symlink(x.root,x.root.string()+"-link");Transaction t;check(!t.begin(x.root.string()+"-link/Nuvio.ffpfsc",x.error),"symlink parent refusal");fs::remove(x.root.string()+"-link");}
    {Fixture x;Transaction t;x.stage(t);put(x.image.string()+".nuvio-"+std::to_string(getpid())+".part","tampered");check(!t.commit([]{return true;},x.error) && read(x.image)=="original-image","stage tampering after verification refusal");}
    {Fixture x;Transaction t;x.stage(t);int calls=0;check(!t.commit([&]{return ++calls==1;},x.error) && read(x.image)=="original-image","remount before commit retains original");}
    {Fixture x;Transaction t;x.stage(t);fail_rename=true;auto ok=t.commit([]{return true;},x.error);fail_rename=false;check(!ok && read(x.image)=="original-image" && read(x.root/t.backup())=="original-image","rename failure retains installed image and verified backup");}
    {Fixture x;Transaction t;x.stage(t);fail_sync=true;auto ok=t.commit([]{return true;},x.error);fail_sync=false;check(!ok && read(x.image)=="original-image","storage sync failure refuses replacement");}
    {Fixture x;Transaction t;x.stage(t);struct stat old{};stat(x.image.c_str(),&old);auto backup=x.image.string()+".nuvio-backup-"+std::to_string(old.st_ino)+"-"+std::to_string(old.st_mtim.tv_sec)+".bak";put(backup,"existing backup");check(!t.commit([]{return true;},x.error) && read(x.image)=="original-image" && read(backup)=="existing backup","existing backup is never overwritten");}
    {Fixture x;put(x.root/"settings.json","saved login and watch progress");Transaction t;x.stage(t);check(t.commit([]{return true;},x.error) && read(x.image)==x.bytes && read(x.root/t.backup())=="original-image" && read(x.root/"settings.json")=="saved login and watch progress","atomic install retains backup and saved data");}
    std::cout<<"FFPFSC updater: "<<tests<<" tests, 0 failures\n";
}
