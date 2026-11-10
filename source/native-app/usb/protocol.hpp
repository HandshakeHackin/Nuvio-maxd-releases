// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../src/live_usb.hpp"
#include "../third_party/json.hpp"
namespace usb_reader {
using Json=nlohmann::json;
constexpr int protocol=1;
inline constexpr const char* title="PPSA99288";
inline bool path_ok(const std::string& path) {
    if(path.empty()||path.size()>1024||!live_usb::playlist(path))return false;
    size_t at=0,parts=0;
    while(at<path.size()){
        auto end=path.find('/',at);auto p=path.substr(at,end-at);
        if(p.empty()||p=="."||p==".."||p.find('\\')!=std::string::npos||p.find('\0')!=std::string::npos||++parts>5)return false;
        if(end==std::string::npos)return true;at=end+1;
    }return false;
}
inline bool request_ok(const Json& j) {
    if(!j.is_object()||!j.contains("protocol")||!j["protocol"].is_number_integer()||j["protocol"]!=protocol||!j.contains("title_id")||j["title_id"]!=title||!j.contains("op")||!j["op"].is_string())return false;
    if(j["op"]=="scan")return j.size()==3;
    if(j["op"]!="read"||j.size()!=5||!j.contains("mount")||!j["mount"].is_number_integer()||!j.contains("path")||!j["path"].is_string())return false;
    auto mount=j["mount"].get<int64_t>();return mount>=0&&mount<8&&path_ok(j["path"].get<std::string>());
}
inline std::vector<std::string> roots() {
    std::vector<std::string> out;for(int i=0;i<8;++i)out.push_back("/mnt/usb"+std::to_string(i));return out;
}
inline int mount_index(const std::string& root){for(int i=0;i<8;++i)if(root=="/mnt/usb"+std::to_string(i))return i;return -1;}
inline Json diagnostic(const live_usb::MountDiagnostics& d,size_t i) {
    return {{"kind","mount"},{"index",i},{"metadata_rc",d.metadata_rc},{"metadata_errno",d.metadata_errno},{"open_errno",d.open_errno},{"directories",d.directories},{"entries",d.entries},{"candidates",d.candidates},{"metadata_failures",d.metadata_failures},{"type_fallbacks",d.type_fallbacks},{"rejected",d.rejected},{"non_directory",d.non_directory},{"cancelled",d.cancelled},{"first_metadata_errno",d.first_metadata_errno},{"directory_failures",d.directory_failures},{"first_directory_errno",d.first_directory_errno},{"read_errno",d.read_errno},{"limited",d.limited}};
}
}
