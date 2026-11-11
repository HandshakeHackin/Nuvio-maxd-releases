#include "../ui/i18n.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
#include "usb_reader.hpp"
#include "update_channel.hpp"
#include "../usb/protocol.hpp"
#include <set>
namespace sx::usb_reader_client {
namespace {
const char* helper_path(){
#ifdef NUVIO_USB_READER_HOST_TEST
    if(const char* path=std::getenv("NUVIO_USB_READER_TEST_HELPER"))return path;
#endif
    return "/app0/nuvio-usb-reader.elf";
}
using usb_reader::Json;
bool message(UpdateChannel& channel,Json& j,const std::atomic<bool>& cancel){std::string line;if(!channel.line(line,cancel))return false;j=Json::parse(line,nullptr,false);return j.is_object();}
bool begin(UpdateChannel& channel,const Json& request,const std::atomic<bool>& cancel,std::string& error){
    if(!usb_reader::request_ok(request)){error=sx::i18n::tr("Invalid USB reader request.");return false;}
    if(!channel.open(helper_path(),cancel,error,30)){error=sx::i18n::tr("Could not start the USB reader. The existing local payload loader must be available on port 9021.");return false;}
    auto data=request.dump()+"\n";Json hello;
    if(!channel.send(data.data(),data.size(),cancel)||!message(channel,hello,cancel)||hello.value("kind",std::string())!="ready"||hello.value("protocol",0)!=usb_reader::protocol||hello.value("title_id",std::string())!=usb_reader::title){error=sx::i18n::tr("USB reader did not complete its handshake.");return false;}return true;
}
live_usb::MountDiagnostics mount(const Json& j){
    live_usb::MountDiagnostics d;d.metadata_rc=j.at("metadata_rc").get<int>();d.metadata_errno=j.at("metadata_errno").get<int>();d.open_errno=j.at("open_errno").get<int>();
    auto count=[&](const char* key){auto n=j.at(key).get<int64_t>();if(n<0||n>20000)throw std::runtime_error("count");return size_t(n);};
    d.directories=count("directories");d.entries=count("entries");d.candidates=count("candidates");d.metadata_failures=count("metadata_failures");d.type_fallbacks=count("type_fallbacks");d.rejected=count("rejected");d.non_directory=j.at("non_directory").get<bool>();d.cancelled=j.at("cancelled").get<bool>();d.directory_failures=count("directory_failures");
    d.first_metadata_errno=j.at("first_metadata_errno").get<int>();d.first_directory_errno=j.at("first_directory_errno").get<int>();d.read_errno=j.at("read_errno").get<int>();d.limited=j.at("limited").get<bool>();
    d.missing=d.metadata_errno==ENOENT&&d.open_errno==ENOENT;d.access_denied=d.open_errno==EPERM||d.open_errno==EACCES;return d;
}
}
Scan scan(const std::atomic<bool>& cancel){
    Scan result;UpdateChannel channel;
    try{
        if(!begin(channel,{{"protocol",usb_reader::protocol},{"title_id",usb_reader::title},{"op","scan"}},cancel,result.error))return result;
        std::set<std::string> seen;size_t mounts=0;
        for(size_t lines=0;lines<521&&!cancel;++lines){Json j;if(!message(channel,j,cancel))break;const auto kind=j.value("kind",std::string());
            if(kind=="mount"){if(mounts>=8||j.at("index")!=mounts)break;result.mounts.push_back(mount(j));++mounts;continue;}
            if(kind=="file"){
                if(mounts!=8||result.files.size()>=512||!j.contains("mount")||!j.contains("path"))break;
                Json request={{"protocol",usb_reader::protocol},{"title_id",usb_reader::title},{"op","read"},{"mount",j["mount"]},{"path",j["path"]}};
                if(!usb_reader::request_ok(request))break;auto root="/mnt/usb"+std::to_string(j["mount"].get<int>());auto path=j["path"].get<std::string>();if(!seen.insert(root+"/"+path).second)break;
                result.files.push_back({root,path,true});continue;
            }
            if(kind=="done"&&mounts==8&&j.at("files")==result.files.size())return result;
            break;
        }
    }catch(...){ }
    result.files.clear();result.error=cancel?sx::i18n::tr("USB scan cancelled."):sx::i18n::tr("USB reader scan failed or returned invalid data.");return result;
}
live_tv::Result import_playlist(const live_usb::File& file,const std::string& profile,const std::string& id,const std::atomic<bool>& cancel){
    UpdateChannel channel;std::string error;const int mount=usb_reader::mount_index(file.root);
    try{
        if(!begin(channel,{{"protocol",usb_reader::protocol},{"title_id",usb_reader::title},{"op","read"},{"mount",mount},{"path",file.path}},cancel,error))return {{},error};
        Json j;if(!message(channel,j,cancel)||j.value("kind",std::string())!="body"||!j.contains("bytes")||!j["bytes"].is_number_unsigned())return {{},sx::i18n::tr("USB reader rejected the file or stopped responding. Scan again.")};
        auto size=j["bytes"].get<uint64_t>();if(!size||size>live_tv::max_bytes)return {{},sx::i18n::tr("USB playlist is empty or exceeds 128 MiB.")};
        std::string body(size,'\0');if(!channel.receive(body.data(),body.size(),cancel)||cancel)return {{},cancel?sx::i18n::tr("Playlist import cancelled."):sx::i18n::tr("USB playlist transfer failed. Previous copy was kept.")};
        return live_usb::save_body(body,profile,id,&cancel);
    }catch(...){return {{},sx::i18n::tr("USB reader transfer failed. Previous copy was kept.")};}
}
}
