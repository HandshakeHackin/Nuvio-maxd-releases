// Profile-local library membership. Resume/history are owned by their separate stores.
// RPC contract: NuvioTV 11689894f02ddc7f81c513abc1de1afe9247db1c,
// SupabaseLibrarySyncRemoteDataSource. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "watch_sync.h"
#include <algorithm>
#include <cstdio>
#include <cerrno>
#include <sys/stat.h>
#include <openssl/rand.h>

namespace library_sync {
inline std::string key(const json& item) {
    return jstr(item, "content_type") + ":" + jstr(item, "content_id");
}
inline bool valid(const json& item) {
    const auto id = jstr(item, "content_id"), type = jstr(item, "content_type");
    return !id.empty() && id.size() <= 512 && (type == "movie" || type == "series") &&
        std::none_of(id.begin(), id.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
inline std::string text(const std::string& value,std::size_t limit) {
    auto end=std::min(limit,value.size());
    while(end>0&&end<value.size()&&(static_cast<unsigned char>(value[end])&0xc0)==0x80)--end;
    return value.substr(0,end);
}
inline json portable(const json& item) {
    json out{{"content_id", jstr(item,"_id")}, {"content_type",jstr(item,"type")},
        {"name",text(jstr(item,"name"),1024)}, {"poster",text(jstr(item,"poster"),4096)},
        {"background",text(jstr(item,"background"),4096)}, {"poster_shape","POSTER"},
        {"description",text(jstr(item,"description"),8192)}, {"release_info",text(jstr(item,"releaseInfo"),256)},
        {"imdb_rating",nullptr}, {"genres",json::array()}, {"addon_base_url",nullptr}, {"added_at",now_epoch_ms()}};
    return out;
}
inline json native(const json& item) {
    return {{"_id",jstr(item,"content_id")}, {"type",jstr(item,"content_type")},
        {"name",jstr(item,"name")}, {"poster",jstr(item,"poster")}, {"background",jstr(item,"background")},
        {"removed",false}, {"temp",false}};
}
// Restrict the temporary file before writing, and never unlink the previous copy.
inline bool save_private(const std::string& path, const json& value) {
    const auto tmp=path+".tmp", data=value.dump();
    if(data.size()>16*1024*1024)return false;
    FILE* f=fopen(tmp.c_str(),"wb"); if(!f)return false;
    bool ok=fchmod(fileno(f),0600)==0;
    if(ok)ok=fwrite(data.data(),1,data.size(),f)==data.size();
    ok=fclose(f)==0 && ok;
    if(!ok || rename(tmp.c_str(),path.c_str())!=0){remove(tmp.c_str());return false;}
    return true;
}
class Store {
    std::string path_; int profile_; json state_; bool readable_=true;
    bool save(){return readable_ && save_private(path_,state_);}
    static std::string client() {
        unsigned char bytes[16];if(RAND_bytes(bytes,sizeof bytes)!=1)return {};
        std::string value="nuvio-ps5-";
        for(unsigned char byte:bytes){char hex[3];snprintf(hex,sizeof hex,"%02x",byte);value+=hex;}
        return value;
    }
public:
    Store(const std::string& directory,int profile):path_(directory+"/library-sync.json"),profile_(profile),
        state_{{"version",1},{"profile",profile},{"client",client()},{"items",json::array()},{"pending",json::array()}} {
        if(jstr(state_,"client").empty()){readable_=false;return;}
        FILE* f=fopen(path_.c_str(),"rb"); if(!f){readable_=errno==ENOENT;return;}
        if(fseek(f,0,SEEK_END)!=0){fclose(f);readable_=false;return;}
        const auto size=ftell(f); if(size<0 || size>16*1024*1024 || fseek(f,0,SEEK_SET)!=0){fclose(f);readable_=false;return;}
        std::string raw(size,'\0'); const bool read=fread(raw.data(),1,raw.size(),f)==raw.size();fclose(f);
        auto data=json::parse(raw,nullptr,false);
        readable_=read && data.is_object() && jnum(data,"version")==1 && jnum(data,"profile",profile)==profile && jobj(data,"items").is_array() &&
            data["items"].size()<=10000 && jobj(data,"pending").is_array() && data["pending"].size()<=128;
        if(readable_)for(const auto& row:data["items"])if(!valid(row))readable_=false;
        if(readable_)for(const auto& op:data["pending"])if(!valid(jobj(op,"item")) || !jobj(op,"add").is_boolean())readable_=false;
        if(readable_){if(jstr(data,"client").empty())data["client"]=state_["client"];state_=std::move(data);}
    }
    bool readable()const{return readable_;}
    bool flush(const WatchSync::Rpc& rpc) {
        if(!readable_)return false;
        while(!state_["pending"].empty()) {
            const auto op=state_["pending"].front();const auto& item=op["item"];const bool add=jbool(op,"add");
            json payload=add?item:json{{"content_id",item["content_id"]},{"content_type",item["content_type"]}};
            const auto h=rpc(add?"sync_push_library_items":"sync_delete_library_items",
                {{"p_profile_id",profile_},{"p_origin_client_id",state_["client"]},{add?"p_items":"p_keys",json::array({payload})}});
            if(!h.ok())return false;
            // Retain acknowledged membership before retiring its durable intent.
            auto& items=state_["items"];
            items.erase(std::remove_if(items.begin(),items.end(),[&](const auto& row){return key(row)==key(item);}),items.end());
            if(add)items.push_back(item);
            state_["pending"].erase(state_["pending"].begin());if(!save())return false;
        }
        return true;
    }
    json items()const {
        json out=state_["items"];
        for(const auto& op:state_["pending"]) {
            const auto& item=op["item"];
            out.erase(std::remove_if(out.begin(),out.end(),[&](const auto& row){return key(row)==key(item);}),out.end());
            if(jbool(op,"add"))out.push_back(item);
        }
        return out;
    }
    bool pull(const WatchSync::Rpc& rpc) {
        if(!readable_)return false;
        const bool sent=flush(rpc);json remote=json::array();bool complete=false;
        for(int offset=0;offset<=10000;offset+=500) {
            const auto h=rpc("sync_pull_library",{{"p_profile_id",profile_},{"p_limit",500},{"p_offset",offset}});
            const auto rows=json::parse(h.body,nullptr,false);
            if(!h.ok() || !rows.is_array() || rows.size()>500)return false;
            for(const auto& row:rows) {
                if(jnum(row,"profile_id",profile_)!=profile_)continue;
                if(!valid(row) || remote.size()>=10000)return false;
                auto safe=portable(native(row));
                safe["added_at"]=jnum(row,"added_at",now_epoch_ms());
                remote.push_back(std::move(safe));
            }
            if(rows.size()<500){complete=true;break;}
        }
        if(!complete)return false;
        state_["items"]=std::move(remote);return save() && sent;
    }
    bool update(const json& item,bool add,const WatchSync::Rpc& rpc,bool& synced,std::string& error) {
        if(!readable_){error="Library cache is unreadable; existing data was retained";return false;}
        if(!valid(item)){error="Unsupported library title";return false;}
        auto next=state_;
        auto& queue=next["pending"];
        queue.erase(std::remove_if(queue.begin(),queue.end(),[&](const auto& op){return key(op["item"])==key(item);}),queue.end());
        if(queue.size()>=128 || (add && items().size()>=10000)){error="Library sync queue or library is full";return false;}
        queue.push_back({{"item",item},{"add",add}});
        if(!save_private(path_,next)){error="Could not save library change";return false;}
        state_=std::move(next);synced=flush(rpc);return true;
    }
};
}
