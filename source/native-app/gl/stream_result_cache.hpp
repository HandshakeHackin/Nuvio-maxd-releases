#pragma once
#include "stremio.h"
#include <algorithm>
#include <cctype>
#include <map>

namespace sx {
// Session memory only. Cache stable source descriptors, never resolved signed
// playback URLs. The caller scopes keys by profile/title/source/options and
// refreshes every source in the background. Failed/incomplete searches are not stored.
class StreamResultCache {
    struct Entry {double created,last_access;std::vector<::Stream> streams;};
    std::map<std::string,Entry> entries_;
    static bool torrent_descriptor(const ::Stream& s) {
        return s.client_resolve.is_null() && s.torrent() && (s.info_hash.size()==40 || s.info_hash.size()==64) &&
            std::all_of(s.info_hash.begin(),s.info_hash.end(),[](unsigned char c){return std::isxdigit(c);});
    }
    static double lifetime(const ::Stream& s) {return torrent_descriptor(s)?300:60;}
    static size_t bytes(const ::Stream& s) {
        size_t n=sizeof(s)+s.name.size()+s.description.size()+s.addon.size()+s.url.size()+s.info_hash.size()+s.filename.size()+s.binge_group.size()+s.debrid_service.size();
        for(const auto& v:s.request_headers)n+=sizeof(v)+v.size();
        for(const auto& v:s.sources)n+=sizeof(v)+v.size();
        return n;
    }
public:
    void clear(){entries_.clear();}
    void erase(const std::string& key){entries_.erase(key);}
    std::vector<::Stream> get(const std::string& key,double now) {
        auto i=entries_.find(key);if(i==entries_.end())return {};
        if(now<i->second.created){entries_.erase(i);return {};}
        auto& entry=i->second;
        std::erase_if(entry.streams,[&](const auto& s){return now-entry.created>=lifetime(s);});
        if(entry.streams.empty()){entries_.erase(i);return {};}
        entry.last_access=now;return entry.streams;
    }
    void put(const std::string& key,const std::vector<::Stream>& streams,double now) {
        std::vector<::Stream> safe;size_t size=0;
        for(const auto& s:streams) {
            const bool stable_url=s.direct_http() && s.url.find_first_of("?#")==std::string::npos && s.client_resolve.is_null();
            if(!stable_url && !torrent_descriptor(s))continue;
            const size_t count=bytes(s);
            if(count>1024*1024-size)continue;
            size+=count;safe.push_back(s);
            // These depend on the current profile/service state, not the cached search.
            safe.back().debrid_resolvable=false;safe.back().debrid_service.clear();
            if(safe.size()==256)break;
        }
        if(safe.empty()){entries_.erase(key);return;}
        if(entries_.size()>=8 && !entries_.count(key)) {
            const auto oldest=std::min_element(entries_.begin(),entries_.end(),[](const auto& a,const auto& b){return a.second.last_access<b.second.last_access;});
            entries_.erase(oldest);
        }
        entries_[key]={now,now,std::move(safe)};
    }
};
}
