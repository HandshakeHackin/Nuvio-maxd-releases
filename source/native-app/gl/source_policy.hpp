#include "../ui/i18n.hpp"
// Conservative source hints: addon metadata is unverified, never a playback guarantee.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "stream_display.hpp"
#include "source_preferences.hpp"
#include <regex>
#include <cstdlib>
#include <sstream>
namespace sx::source_policy {
struct Context { int season=-1,episode=-1; };
inline Context context(const Meta& meta,const std::string& id) {
    for(const auto& v:meta.videos)if(v.id==id) {
        if(v.imdb_episode>0)return {v.imdb_season,v.imdb_episode};
        if(starts_with(id,"kitsu:")||starts_with(id,"anilist:")||starts_with(id,"mal:"))return {};
        return {v.season,v.episode};
    }
    return {};
}
struct Assessment { bool hidden=false;int score=0;std::string reason,warning; };
inline double bytes(const ::Stream& s) {
    if(std::isfinite(s.video_size)&&s.video_size>0)return s.video_size;
    const auto label=lower(stream_display::size_of(s));
    // Only accept an unambiguous decimal, never guess the meaning of commas.
    if(label.find(',')!=std::string::npos)return 0;
    char* end=nullptr;const double n=std::strtod(label.c_str(),&end);
    if(!std::isfinite(n)||n<=0||!end)return 0;
    const auto unit=trim(end);double mul=0;
    if(unit=="gb")mul=1e9;else if(unit=="gib")mul=1073741824.0;
    else if(unit=="mb")mul=1e6;else if(unit=="mib")mul=1048576.0;
    else if(unit=="tb")mul=1e12;else if(unit=="tib")mul=1099511627776.0;
    return std::isfinite(n*mul)?n*mul:0;
}
inline bool language_match(const ::Stream& s,const std::string& wanted) {
    const auto advertised=lower(stream_display::language_of(s));
    const auto text=stream_display::metadata(s)+" "+advertised;
    const std::pair<const char*,const char*> langs[]={
        {"eng","english en eng us gb"},{"spa","spanish espanol español es spa"},{"fra","french français francais fr fra fre"},
        {"deu","german deutsch de deu ger"},{"ita","italian italiano it ita"},{"por","portuguese português portugues pt por br"},
        {"jpn","japanese japan ja jpn jp"},{"kor","korean ko kor kr"},{"hin","hindi hi hin"}};
    for(const auto& lang:langs)if(wanted==lang.first) {
        std::istringstream words(lang.second);std::string word;
        while(words>>word)if(stream_display::has(word.size()<=2?advertised:text,word))return true;
    }
    return false;
}
inline Assessment assess(const ::Stream& s,const Preferences& p,Context ctx={}) {
    Assessment a;const auto text=stream_display::metadata(s).substr(0,8192);
    // Check a supplied filename, rather than matching unrelated prose or URLs.
    const auto file=lower(stream_display::plain(s.filename)).substr(0,4096);
    const auto n=bytes(s);const auto res=stream_display::resolution_of(s);
    const int height=res=="4K"?2160:res=="1440p"?1440:res=="1080p"?1080:res=="720p"?720:res=="SD"?480:0;
    const auto has=[&](const char* word){return stream_display::has(text,word);};
    const bool hdr=has("hdr")||has("hdr10")||has("hdr10+")||has("hdr10plus")||has("hdr10 plus")||has("hlg")||has("dolby vision")||has("dovi")||has("dv");
    const bool cam=has("cam")||has("camrip")||has("hdcam")||has("telesync")||has("hdts")||has("telecine")||has("screener")||has("dvdscr");
    std::vector<std::string> reasons;
    const auto hide=[&](const char* reason){a.hidden=true;if(a.reason.empty())a.reason=reason;};
    if(p.checks) {
        const auto dot=file.find_last_of('.');const auto ext=dot==std::string::npos?"":file.substr(dot);
        if(ext==".exe"||ext==".scr"||ext==".bat"||ext==".cmd"||ext==".msi"||ext==".apk"||ext==".lnk"||ext==".zip"||ext==".rar"||ext==".7z"||ext==".iso")hide(sx::i18n::tr("Archive or non-video file"));
        if(stream_display::has(file,"sample")) {
            if(n>0&&n<400*1048576.0)hide(sx::i18n::tr("Likely sample file"));
            else a.warning=sx::i18n::tr("Filename mentions sample");
        }
        if(ctx.season>=0&&ctx.episode>0&&!file.empty()) {
            static const std::regex episode(R"((?:^|[^a-z0-9])s([0-9]{1,2})[ ._-]?e([0-9]{1,3})(?:[ ._-]*(?:-|e|to)[ ._-]*e?([0-9]{1,3}))?(?![a-z0-9]))");
            bool found=false,match=false;
            for(auto i=std::sregex_iterator(file.begin(),file.end(),episode);i!=std::sregex_iterator();++i) {
                found=true;const int season=std::stoi((*i)[1]),first=std::stoi((*i)[2]);
                const int last=(*i)[3].matched?std::stoi((*i)[3]):first;
                match|=season==ctx.season&&first<=ctx.episode&&ctx.episode<=last;
            }
            const bool pack=stream_display::has(file,"complete")||stream_display::has(file,"pack")||stream_display::has(file,"season");
            if(found&&!match){if(pack)a.warning=sx::i18n::tr("Check episode in this pack");else hide(sx::i18n::tr("Filename names another episode"));}
        }
    }
    if(cam){a.warning=sx::i18n::tr("Possible cinema recording / screener");a.score-=300;if(p.hide_cam)hide(sx::i18n::tr("Cinema recording / screener excluded"));}
    if(p.min_height&&height&&height<p.min_height)hide(sx::i18n::tr("Below preferred resolution"));
    if(p.max_gb&&n>double(p.max_gb)*1e9)hide(sx::i18n::tr("Above file-size limit"));
    if(p.hdr=="avoid"&&hdr)hide(sx::i18n::tr("HDR excluded by preference"));
    if(!p.language.empty()&&language_match(s,p.language)){a.score+=200;reasons.push_back(sx::i18n::tr("Preferred language advertised"));}
    if(s.direct_http()){a.score+=40;reasons.push_back(sx::i18n::tr("Direct link"));}
    else if(s.debrid_resolvable){a.score+=30;reasons.push_back(sx::i18n::tr("Debrid resolvable"));}
    if(height){a.score+=height/60;reasons.push_back(res);}
    if(p.hdr=="prefer"&&hdr){a.score+=25;reasons.push_back("HDR advertised");}
    if(!a.warning.empty())a.score-=100;
    if(a.reason.empty())a.reason=join(reasons," · ");
    return a;
}
struct Selection { std::vector<size_t> order;std::vector<Assessment> assessments;size_t hidden=0; };
inline Selection select(const std::vector<::Stream>& streams,const Preferences& p,Context ctx,bool allow_torrents,bool show_all=false) {
    Selection out;out.assessments.reserve(streams.size());
    for(size_t i=0;i<streams.size();++i){out.assessments.push_back(assess(streams[i],p,ctx));
        if(!streams[i].allowed(allow_torrents)&&!streams[i].debrid_resolvable)continue;
        if(out.assessments.back().hidden&&!show_all){++out.hidden;continue;}
        out.order.push_back(i);
    }
    std::stable_sort(out.order.begin(),out.order.end(),[&](size_t a,size_t b){
        const auto& x=out.assessments[a];const auto& y=out.assessments[b];
        if(x.hidden!=y.hidden)return !x.hidden;
        if(x.score!=y.score)return x.score>y.score;
        return stream_display::seeds_of(streams[a].description)>stream_display::seeds_of(streams[b].description);
    });return out;
}
}
