// Read-side watched scopes. Canonical history never changes source numbering.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "util.h"
#include "airtime.hpp"
#include <map>
#include <set>
namespace sx::watched_history {
using Dates=std::map<std::string,int64_t>;
inline std::string scope_key(const std::string& id,int season=-1) {
    return id+(season<0?":*":":"+std::to_string(season)+":*");
}
inline bool episode(const std::set<std::string>& watched,const Dates& scopes,const Dates& exclusions,
                    const std::string& content,const std::string& native,int season,int number,
                    const std::string& released,int64_t now_ms) {
    const auto key=content+":"+std::to_string(season)+":"+std::to_string(number);
    if(exclusions.count(key)||exclusions.count(native)||season<0||number<1)return false;
    if(watched.count(key)||watched.count(native))return true;
    const auto air=airtime::iso(released);if(!air||*air>now_ms/1000)return false;
    for(int s:{-1,season}) {
        const auto it=scopes.find(scope_key(content,s));
        if(it!=scopes.end() && *air<=it->second/1000)return true;
    }
    return false;
}
struct Snapshot {std::set<std::string> watched;Dates scopes,exclusions;};
inline bool decode(const json& rows,Snapshot& output,const Dates& old_scopes={},const Dates& local_exclusions={}) {
    if(!rows.is_array()||rows.size()>45000)return false;
    Snapshot next;Dates times;
    for(const auto& row:rows) {
        const auto id=jstr(row,"content_id");if(id.empty()||id.size()>256)return false;
        const auto& sn=jobj(row,"season");const auto& en=jobj(row,"episode");
        const auto scope=jstr(row,"scope");
        const auto& flag=jobj(row,"watched");if(!flag.is_null()&&!flag.is_boolean())return false;
        const bool marked=flag.is_null()||flag.get<bool>();
        const auto& at=jobj(row,"watched_at");
        if(!at.is_null()&&(!at.is_number_integer()||at.get<double>()<0||at.get<double>()>253402300799999.))return false;
        const int64_t stamp=at.is_null()?0:at.get<int64_t>();
        std::set<std::string> aliases{id};
        const auto& ids=jobj(row,"aliases");
        if(!ids.is_null()) {
            if(!ids.is_array()||ids.size()>8)return false;
            for(const auto& alias:ids){if(!alias.is_string()||alias.get<std::string>().empty()||alias.get<std::string>().size()>256)return false;aliases.insert(alias.get<std::string>());}
        }
        const bool season_scope=scope=="season",show_scope=scope=="show";
        if(!scope.empty()&&!season_scope&&!show_scope)return false;
        if((season_scope||show_scope)&&(!en.is_null()||(marked&&stamp<=0)))return false;
        if(season_scope&&(!sn.is_number_integer()||jnum(row,"season",-1)<0||jnum(row,"season")>100000))return false;
        if(show_scope&&!sn.is_null())return false;
        if(scope.empty() && (!sn.is_null()||!en.is_null()) &&
           (!sn.is_number_integer()||!en.is_number_integer()||jnum(row,"season",-1)<0||jnum(row,"season")>100000||jnum(row,"episode")<1||jnum(row,"episode")>100000))return false;
        for(const auto& alias:aliases) {
            if(season_scope||show_scope) {
                if(marked)next.scopes[scope_key(alias,season_scope?int(jnum(row,"season")):-1)]=stamp;
                continue;
            }
            const auto key=alias+(sn.is_null()?"":":"+std::to_string(int(jnum(row,"season")))+":"+std::to_string(int(jnum(row,"episode"))));
            if(marked){next.watched.insert(key);times[key]=stamp;}
            else next.exclusions[key]=stamp;
        }
    }
    // A compact parent scope must not resurrect a locally unmarked episode.
    // Retain that exception only while the same parent receipt remains active;
    // a newer explicit remote mark or a new parent receipt supersedes it.
    for(const auto& [key,stamp]:local_exclusions) {
        const auto end=key.rfind(':');if(end==std::string::npos)continue;
        const auto start=key.rfind(':',end-1);if(start==std::string::npos)continue;
        const auto content=key.substr(0,start),season=key.substr(start+1,end-start-1);
        bool same=false;
        for(const auto& parent:{scope_key(content),content+":"+season+":*"}) {
            const auto a=old_scopes.find(parent);
            const auto b=next.scopes.find(parent);
            if(a!=old_scopes.end()&&b!=next.scopes.end()&&a->second==b->second)same=true;
        }
        if(same && (!times.count(key)||times[key]<=stamp)) {next.exclusions[key]=stamp;next.watched.erase(key);}
    }
    if(next.watched.size()+next.scopes.size()+next.exclusions.size()>225000)return false;
    output=std::move(next);return true;
}
inline bool movie(const std::string& type,const std::string& id,const std::set<std::string>& watched) {
    return type=="movie"&&watched.count(id)!=0;
}
}
