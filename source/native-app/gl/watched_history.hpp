// Only explicit movie/episode history is evidence of a watched item.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "util.h"
#include <set>
namespace sx::watched_history {
inline void completed(std::set<std::string>& watched,const std::string& type,
                      const std::string& content,const std::string& native,int season,int number) {
    if(!native.empty())watched.insert(native);
    const auto key=type=="movie"?content:season>=0&&season<=100000&&number>0&&number<=100000?
        content+":"+std::to_string(season)+":"+std::to_string(number):"";
    if(!content.empty()&&!key.empty())watched.insert(key);
}
inline bool episode(const std::set<std::string>& watched,const std::string& content,
                    const std::string& native,int season,int number) {
    if(season<0||number<1)return false;
    return watched.count(native)||watched.count(content+":"+std::to_string(season)+":"+std::to_string(number));
}
struct Snapshot {std::set<std::string> watched;};
inline bool decode(const json& rows,Snapshot& output) {
    if(!rows.is_array()||rows.size()>45000)return false;
    Snapshot next;std::set<std::string> unmarked;
    for(const auto& row:rows) {
        const auto scope=jstr(row,"scope");
        if(row.contains("scope")&&!jobj(row,"scope").is_string())return false;
        // Discard the old 1.0.0.22 normalization if it reaches this boundary.
        // Activity summaries cannot identify any completed episode.
        if(scope=="show"||scope=="season")continue;
        if(!scope.empty())return false;
        const auto id=jstr(row,"content_id");if(id.empty()||id.size()>256)return false;
        const auto& sn=jobj(row,"season");const auto& en=jobj(row,"episode");
        const auto& flag=jobj(row,"watched");if(!flag.is_null()&&!flag.is_boolean())return false;
        const bool marked=flag.is_null()||flag.get<bool>();
        const auto& at=jobj(row,"watched_at");
        if(!at.is_null()&&(!at.is_number_integer()||at.get<double>()<0||at.get<double>()>253402300799999.))return false;
        std::set<std::string> aliases{id};
        const auto& ids=jobj(row,"aliases");
        if(!ids.is_null()) {
            if(!ids.is_array()||ids.size()>8)return false;
            for(const auto& alias:ids){if(!alias.is_string()||alias.get<std::string>().empty()||alias.get<std::string>().size()>256)return false;aliases.insert(alias.get<std::string>());}
        }
        if((!sn.is_null()||!en.is_null()) &&
           (!sn.is_number_integer()||!en.is_number_integer()||jnum(row,"season",-1)<0||jnum(row,"season")>100000||jnum(row,"episode")<1||jnum(row,"episode")>100000))return false;
        for(const auto& alias:aliases) {
            const auto key=alias+(sn.is_null()?"":":"+std::to_string(int(jnum(row,"season")))+":"+std::to_string(int(jnum(row,"episode"))));
            if(marked)next.watched.insert(key);else unmarked.insert(key);
        }
    }
    for(const auto& key:unmarked)next.watched.erase(key);
    if(next.watched.size()+unmarked.size()>225000)return false;
    output=std::move(next);return true;
}
inline bool movie(const std::string& type,const std::string& id,const std::set<std::string>& watched) {
    return type=="movie"&&watched.count(id)!=0;
}
}
