#pragma once
#include "stremio.h"
#include <map>

namespace sx {
// Session memory only. Never persist or reuse signed/query URLs, and refresh
// cached results in the background. The caller includes profile/source/options
// identity in the key; errors and incomplete searches are not cached.
class StreamResultCache {
    struct Entry {double expires;std::vector<::Stream> streams;};
    std::map<std::string,Entry> entries_;
public:
    void clear(){entries_.clear();}
    std::vector<::Stream> get(const std::string& key,double now) {
        auto i=entries_.find(key);if(i==entries_.end())return {};
        if(i->second.expires<=now){entries_.erase(i);return {};}
        return i->second.streams;
    }
    void put(const std::string& key,const std::vector<::Stream>& streams,double now) {
        std::vector<::Stream> safe;
        for(const auto& s:streams)if(s.direct_http() && s.url.find_first_of("?#")==std::string::npos) {
            safe.push_back(s);if(safe.size()==256)break;
        }
        if(safe.empty()){entries_.erase(key);return;}
        if(entries_.size()>=8 && !entries_.count(key))entries_.erase(entries_.begin());
        entries_[key]={now+60,std::move(safe)};
    }
};
}
