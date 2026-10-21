// Source badges: decoded picture/selected audio override release-name hints.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace sx::playback_badges {
inline std::string normalized(std::string s) {
    s.resize(std::min<std::size_t>(s.size(),8192));
    for(char& c:s) { if(c=='_')c=' '; else c=char(std::tolower(static_cast<unsigned char>(c))); }
    return s;
}
inline bool token(const std::string& s,const std::string& n) {
    for(std::size_t p=0;(p=s.find(n,p))!=std::string::npos;p+=n.size())
        if((p==0 || !std::isalnum(static_cast<unsigned char>(s[p-1]))) &&
           (p+n.size()==s.size() || !std::isalnum(static_cast<unsigned char>(s[p+n.size()]))))return true;
    return false;
}
inline bool any(const std::string& s,std::initializer_list<const char*> terms) {
    for(auto term:terms)if(token(s,term))return true;
    return false;
}
inline std::string resolution(int w,int h) {
    if(w<=0 || h<=0)return {};
    if(h>w)return std::to_string(w)+"×"+std::to_string(h);
    // Width recognizes cropped cinema pictures (3840×1600 is still 4K).
    if(w>=7680 || h>=4320)return "8K";
    if(w>=3840 || h>=2160)return "4K";
    if(w>=2560 || h>=1440)return "1440p";
    if(w>=1920 || h>=1080)return "1080p";
    if(w>=1280 || h>=720)return "720p";
    if(w>=1024 || h>=576)return "576p";
    if(w>=960 || h>=540)return "540p";
    if(w>=720 || h>=480)return "480p";
    if(w>=640 || h>=360)return "360p";
    if(h>=240)return "240p";
    return std::to_string(w)+"×"+std::to_string(h);
}
inline std::string advertised_resolution(const std::string& s) {
    for(auto value:{"4320p","2160p","1440p","1080p","1080i","720p","576p","540p","480p","360p","240p"})
        if(token(s,value))return std::string(value)=="4320p"?"8K":std::string(value)=="2160p"?"4K":value;
    if(any(s,{"8k"}))return "8K";
    if(any(s,{"4k","uhd"}))return "4K";
    if(token(s,"fhd"))return "1080p";
    return {};
}
inline std::string channels(std::string s) {
    // Providers frequently concatenate a codec and layout: DDP5.1 / AAC2.0.
    for(auto prefix:{"ddp","dd+","aac","eac3","ac3","truehd","dts"}) {
        std::size_t p=0;
        while((p=s.find(prefix,p))!=std::string::npos) {
            p+=std::char_traits<char>::length(prefix);
            if(p<s.size() && std::isdigit(static_cast<unsigned char>(s[p])))s.insert(p++," ");
        }
    }
    std::string result;std::size_t last=0;
    const auto quantity=[&](std::size_t p) {
        const auto tail=s.substr(p+3);const auto first=tail.find_first_not_of(' ');
        if(first==std::string::npos)return false;
        const auto unit=tail.substr(first,tail.find_first_not_of("abcdefghijklmnopqrstuvwxyz",first)-first);
        return unit=="gb" || unit=="gib" || unit=="mb" || unit=="mib" || unit=="tb" || unit=="tib" || unit=="fps";
    };
    for(auto value:{"1.0","2.0","2.1","3.0","3.1","4.0","4.1","5.0","5.1","6.0","6.1","7.0","7.1"})
        for(std::size_t p=0;(p=s.find(value,p))!=std::string::npos;++p)
            if((p==0 || !std::isalnum(static_cast<unsigned char>(s[p-1]))) &&
               (p+3==s.size() || !std::isalnum(static_cast<unsigned char>(s[p+3]))) && !quantity(p) && (result.empty() || p>=last)) {result=value;last=p;}
    for(auto value:{"mono","stereo"}) {
        const auto p=s.rfind(value);
        if(p!=std::string::npos && token(s,value) && (result.empty() || p>=last)) {result=std::string(value)=="mono"?"1.0":"2.0";last=p;}
    }
    // VLC exposes channel count, not speaker layout. Keep that distinction.
    for(std::size_t p=0;p<s.size();++p)if(std::isdigit(static_cast<unsigned char>(s[p])) && (p==0 || !std::isalnum(static_cast<unsigned char>(s[p-1])))) {
        auto e=p;while(e<s.size() && std::isdigit(static_cast<unsigned char>(s[e])))++e;
        const auto digits=s.substr(p,e-p);auto b=e;while(e<s.size() && s[e]==' ')++e;
        if(e>b && (s.compare(e,2,"ch")==0) && (e+2==s.size() || !std::isalpha(static_cast<unsigned char>(s[e+2])) || s.compare(e,8,"channels")==0) && (result.empty() || p>=last)) {
            const int n=digits.size()<3?std::stoi(digits):0;
            if(n>0 && n<=32){result=n==1?"1.0":n==2?"2.0":digits+" ch";last=p;}
        }
        p=b-1;
    }
    return result;
}
inline std::string video_codec(const std::string& s) {
    if(any(s,{"hevc","h265","h.265","x265"}))return "HEVC";
    if(any(s,{"avc","h264","h.264","x264"}))return "H.264";
    if(token(s,"av1"))return "AV1";
    if(token(s,"vp9"))return "VP9";
    if(any(s,{"mpeg2video","mpeg-2"}))return "MPEG-2";
    return {};
}
inline std::string audio_codec(const std::string& s) {
    if(any(s,{"truehd","true hd"}))return "TrueHD";
    if(any(s,{"dts-hd","dts hd","dts-hdma","dts-hd.ma"}))return "DTS-HD";
    if(token(s,"dts:x"))return "DTS:X";
    if(token(s,"dts"))return "DTS";
    if(any(s,{"eac3","e-ac3","e-ac-3","ddp","dd+","dolby digital plus","ddp2.0","ddp5.1","ddp7.1","dd+2.0","dd+5.1","dd+7.1"}))return "DD+";
    if(any(s,{"ac3","ac-3","dolby digital"}))return "Dolby Digital";
    if(any(s,{"aac","aac2.0","aac5.1","aac7.1"}))return "AAC";
    if(token(s,"flac"))return "FLAC";
    if(any(s,{"pcm","pcm_s16le","pcm_s24le","pcm_f32le","lpcm"}))return "PCM";
    return {};
}
inline std::string release_type(const std::string& s) {
    if(any(s,{"web-dl","web dl","webdl"}))return "WEB-DL";
    if(any(s,{"webrip","web-rip","web rip"}))return "WEBRip";
    if(any(s,{"bdrip","bd-rip","bd rip"}))return "BDRip";
    if(any(s,{"brrip","br-rip","br rip"}))return "BRRip";
    if(any(s,{"blu-ray","blu ray","bluray"}))return "Blu-ray";
    if(any(s,{"dvdrip","dvd-rip","dvd rip"}))return "DVD-Rip";
    if(any(s,{"dvdscr","dvd-scr","dvd scr"}))return "DVD-SCR";
    if(token(s,"dvd"))return "DVD";
    if(token(s,"hdtv"))return "HDTV";
    if(token(s,"pdtv"))return "PDTV";
    if(any(s,{"hdcam","hd-cam","camrip","cam"}))return "CAM";
    if(any(s,{"hdts","telesync","ts"}))return "TS";
    if(any(s,{"telecine","tc"}))return "TC";
    if(any(s,{"screener","scr"}))return "Screener";
    return {};
}
inline std::vector<std::string> build(const std::string& metadata,int w=0,int h=0,bool picture_known=false,
                                     bool hdr=false,bool ten_bit=false,const std::string& audio={},const std::string& codec={}) {
    const auto m=normalized(metadata),a=normalized(audio);std::vector<std::string> out;
    const auto add=[&](const std::string& value){if(!value.empty() && std::find(out.begin(),out.end(),value)==out.end())out.push_back(value);};
    add(picture_known?resolution(w,h):advertised_resolution(m));
    if(picture_known)add(hdr?"HDR10":"SDR");
    else if(any(m,{"hdr10+","hdr10plus","hdr10 plus"}))add("HDR10+");
    else if(token(m,"hdr10"))add("HDR10");
    else if(token(m,"hdr"))add("HDR");
    else if(token(m,"sdr"))add("SDR");
    add(channels(audio.empty()?m:a));
    add(release_type(m));if(token(m,"remux"))add("REMUX");
    const auto actual=video_codec(normalized(codec.substr(0,codec.find(' '))));
    add(actual.empty()?video_codec(m):actual);
    add(audio_codec(audio.empty()?m:a));
    if(token(audio.empty()?m:a,"atmos"))add("Atmos");
    if((!picture_known || hdr) && any(m,{"dolby vision","dovi","dv"}))add("Dolby Vision source");
    if(picture_known && hdr && any(m,{"hdr10+","hdr10plus","hdr10 plus"}))add("HDR10+ source");
    if(picture_known && ten_bit)add("10-bit");
    return out;
}
} // namespace sx::playback_badges
