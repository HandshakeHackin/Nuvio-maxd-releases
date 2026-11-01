// Local title matching; shares the existing UTF-8/Latin accent fold with IPTV.
#pragma once
#include "iptv_library.h"
#include "util.h"
#include <sstream>
namespace sx::library_search {
inline std::string query(const std::string& text) {
    auto result=trim(text);
    if(result.size()>256) {
        size_t end=256;
        while(end && (static_cast<unsigned char>(result[end])&0xc0)==0x80) --end;
        result.resize(end);
    }
    return result;
}
inline std::vector<std::string> terms(const std::string& query) {
    std::istringstream input(iptv::search_key(query));
    std::vector<std::string> words;
    for(std::string word; input>>word;) words.push_back(std::move(word));
    return words;
}
inline bool matches(const std::string& title,const std::vector<std::string>& words) {
    if(words.empty()) return true;
    const auto key=iptv::search_key(title);
    for(const auto& word:words) if(key.find(word)==std::string::npos) return false;
    return true;
}
}
