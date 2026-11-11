// Bundled application text only. Never run addon/user text through this lookup.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <atomic>
#include <string>
#include <string_view>
namespace sx::i18n {
enum class Language {english,german};
inline std::atomic<Language> active{Language::english};
inline const char* code(){return active.load(std::memory_order_relaxed)==Language::german?"de":"en";}
inline bool german(){return active.load(std::memory_order_relaxed)==Language::german;}
inline void set_language(std::string_view s){active.store(s=="de"?Language::german:Language::english,std::memory_order_relaxed);}
struct Entry {std::string_view english;const char* german;};
inline constexpr Entry dictionary[]={
#include "locales/de.inc"
};
inline const char* tr(const char* s){
    if(!german())return s;
    auto it=std::lower_bound(std::begin(dictionary),std::end(dictionary),std::string_view(s),[](const Entry& e,std::string_view k){return e.english<k;});
    return it!=std::end(dictionary)&&it->english==s?it->german:s;
}
inline std::string tr(const std::string& s){return tr(s.c_str());}
inline std::string media_type(const std::string& s){return s=="movie"?tr("Movie"):s=="series"?tr("Series"):s;}
// UTF-8 case/diacritic folding for the built-in settings search. This is not
// process locale state: protocol/numeric parsing keeps the classic locale.
inline std::string search_key(std::string_view text){
    std::string out;
    for(size_t i=0;i<text.size();){unsigned char c=text[i++];if(c<128){out+=char(c>='A'&&c<='Z'?c+32:c);continue;}
        if((c==0xcc&&i<text.size()&&static_cast<unsigned char>(text[i])>=0x80&&static_cast<unsigned char>(text[i])<=0xbf)||(c==0xcd&&i<text.size()&&static_cast<unsigned char>(text[i])>=0x80&&static_cast<unsigned char>(text[i])<=0xaf)){++i;continue;}
        if(c==0xc3&&i<text.size()) {unsigned char d=text[i++];switch(d){
            case 0x84:case 0xa4:out+="a";break;case 0x96:case 0xb6:out+="o";break;case 0x9c:case 0xbc:out+="u";break;case 0x9f:out+="ss";break;
            default:out+=char(c);out+=char(d);break;}continue;}
        if(c==0xe1&&i+1<text.size()&&static_cast<unsigned char>(text[i])==0xba&&static_cast<unsigned char>(text[i+1])==0x9e){out+="ss";i+=2;continue;}
        out+=char(c);
    }return out;
}
}
