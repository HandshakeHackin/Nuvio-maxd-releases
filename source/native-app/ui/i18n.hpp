// Application-owned text only. Never translate addon/user strings or IDs.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace sx::i18n {
struct Locale {
    const char* code;
    const char* native_name;
    const char* metadata_language;
    bool rtl;
    std::size_t translated_entries;
};
namespace detail {
#include "locales/catalogs.inc"
inline std::atomic<std::size_t> active{0};
}
inline auto locales(){return std::span<const Locale>(detail::catalog_locales);}
inline const char* normalize_code(std::string_view input){
    if(input.empty()||input.size()>35)return "en";
    std::string tag;
    for(unsigned char c:input){
        if(c=='_')c='-';
        if(c>='A'&&c<='Z')c+=32;
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return "en";
        tag+=char(c);
    }
    if(tag.front()=='-'||tag.back()=='-'||tag.find("--")!=std::string::npos)return "en";
    for(const auto& locale:locales()){
        std::string canonical=locale.code;
        for(char& c:canonical)if(c>='A'&&c<='Z')c+=32;
        if(tag==canonical)return locale.code;
    }
    // Only Simplified Chinese is bundled; do not relabel Traditional Chinese.
    if(tag=="zh"||tag=="zh-cn"||tag=="zh-sg"||tag=="zh-hans"||tag=="zh-hans-cn"||tag=="zh-hans-sg")return "zh-Hans";
    auto base=std::string_view(tag).substr(0,tag.find('-'));
    for(const auto& locale:locales())if(base==locale.code)return locale.code;
    return "en";
}
inline std::size_t index(std::string_view code){
    const auto normalized=std::string_view(normalize_code(code));
    const auto list=locales();
    for(std::size_t n=0;n<list.size();++n)if(normalized==list[n].code)return n;
    return 0;
}
inline const Locale& current(){return detail::catalog_locales[detail::active.load(std::memory_order_relaxed)];}
inline const char* code(){return current().code;}
inline const char* metadata_language(){return current().metadata_language;}
inline const char* native_name(std::string_view code){return detail::catalog_locales[index(code)].native_name;}
inline bool german(){return std::string_view(code())=="de";}
inline bool rtl(){return current().rtl;}
inline void set_language(std::string_view code){detail::active.store(index(code),std::memory_order_relaxed);}
inline const char* tr(const char* s){
    const auto language=detail::active.load(std::memory_order_relaxed);
    if(!language)return s;
    const auto begin=std::begin(detail::key_offsets),end=std::end(detail::key_offsets);
    const auto key=std::string_view(s);
    auto it=std::lower_bound(begin,end,key,[](std::uint32_t offset,std::string_view needle){return std::string_view(detail::text_blob+offset)<needle;});
    if(it==end||std::string_view(detail::text_blob+*it)!=key)return s;
    const auto offset=detail::value_offsets[language][it-begin];
    return offset?detail::text_blob+offset:s;
}
inline std::string tr(const std::string& s){return tr(s.c_str());}
inline std::string media_type(const std::string& s){return s=="movie"?tr("Movie"):s=="series"?tr("Series"):s;}

// Menu search only: preserve CJK, fold Latin accents/case and Arabic marks.
// Never change process locale or protocol/numeric parsing.
inline std::string search_key(std::string_view text){
    std::string out;
    for(std::size_t i=0;i<text.size();){
        const auto start=i;unsigned char c=text[i++];std::uint32_t cp=c;
        const int extra=c<0x80?0:(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:(c&0xf8)==0xf0?3:-1;
        if(extra<0||i+std::size_t(extra)>text.size())continue;
        cp&=extra==0?0x7f:extra==1?0x1f:extra==2?0xf:7;
        bool valid=true;
        for(int n=0;n<extra;++n){auto d=static_cast<unsigned char>(text[i++]);if((d&0xc0)!=0x80)valid=false;cp=(cp<<6)|(d&0x3f);}
        if(!valid)continue;
        if((cp>=0x300&&cp<=0x36f)||(cp>=0x64b&&cp<=0x65f)||cp==0x670||cp==0x640)continue;
        if(cp>='A'&&cp<='Z'){out+=char(cp+32);continue;}
        if(cp==0xdf||cp==0x1e9e){out+="ss";continue;}
        if((cp>=0xc0&&cp<=0xc5)||(cp>=0xe0&&cp<=0xe5)){out+='a';continue;}
        if(cp==0xc6||cp==0xe6){out+="ae";continue;}
        if(cp==0xc7||cp==0xe7){out+='c';continue;}
        if((cp>=0xc8&&cp<=0xcb)||(cp>=0xe8&&cp<=0xeb)){out+='e';continue;}
        if((cp>=0xcc&&cp<=0xcf)||(cp>=0xec&&cp<=0xef)){out+='i';continue;}
        if(cp==0xd1||cp==0xf1){out+='n';continue;}
        if((cp>=0xd2&&cp<=0xd6)||(cp>=0xf2&&cp<=0xf6)||cp==0xd8||cp==0xf8){out+='o';continue;}
        if(cp==0x152||cp==0x153){out+="oe";continue;}
        if((cp>=0xd9&&cp<=0xdc)||(cp>=0xf9&&cp<=0xfc)){out+='u';continue;}
        if(cp==0xdd||cp==0xfd||cp==0xff||cp==0x178){out+='y';continue;}
        if(cp==0x622||cp==0x623||cp==0x625||cp==0x671){out+="ا";continue;}
        out.append(text.substr(start,i-start));
    }
    return out;
}
}
