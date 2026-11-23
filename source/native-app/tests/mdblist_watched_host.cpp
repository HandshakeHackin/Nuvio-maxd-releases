// Official nested MDBList history fixtures through the production Store.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tracking.h"
#include "watched_history.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <unistd.h>
namespace {
json page() {return {{"movies",json::array()},{"shows",json::array()},{"seasons",json::array()},{"episodes",json::array()},{"pagination",{{"has_more",false},{"limit",100}}}};}
json media(const std::string& id="tt123") {return {{"ids",{{"imdb",id},{"tmdb",9}}}};}
json episode(int n=1) {return {{"last_watched_at","2025-10-15T20:00:00Z"},{"episode",{{"season",1},{"number",n},{"show",media()}}}};}
}
void mdblist_watched_cases() {
    const auto root="/tmp/nuvio-watched-"+std::to_string(getpid());std::filesystem::create_directories(root);int cases=0;
    auto run=[&](const std::function<json(const std::string&,int)>& response,bool ok,const std::function<void(const json&,int)>& check) {
        const auto dir=root+"/"+std::to_string(++cases);std::filesystem::create_directory(dir);int gets=0;
        tracking::Store store(dir,[&](const auto& url,const auto* body,const auto&)->HttpResponse {
            assert(!body&&starts_with(url,"https://api.mdblist.com/"));
            if(url.find("/user?")!=std::string::npos)return {200,R"({"user_id":123})","","",0};
            assert(url.find("/sync/watched?limit=100")!=std::string::npos);
            auto data=response(url,++gets);return {200,data.dump(),"","",0};
        });
        std::string error;store.action("key",{{"provider","mdblist"},{"api_key","synthetic"}},error);assert(error.empty());
        store.action("source",{{"field","progress"},{"value","mdblist"}},error);assert(error.empty());
        json rows=json::array({"retained"});assert(store.watched(rows,error)==ok);
        if(!ok){assert(rows==json::array({"retained"})&&!error.empty());}
        else assert(error.empty());
        check(rows,gets);
    };
    auto value=[&](const json& body,bool ok,const std::function<void(const json&,int)>& check=[](const auto&,int){}) {
        run([&](const auto&,int){return body;},ok,check);
    };
    auto p=page();p["episodes"].push_back(episode());
    value(p,true,[&](const auto& rows,int count){assert(count==1&&rows[0]["content_id"]=="tt123"&&rows[0]["season"]==1&&rows[0]["episode"]==1);
        sx::watched_history::Snapshot s;assert(sx::watched_history::decode(rows,s));assert(s.watched.count("tt123:1:1")&&s.watched.count("tmdb:9:1:1"));});
    p=page();p["movies"].push_back({{"movie",media()},{"last_watched_at","2025-10-15T20:00:00Z"}});
    value(p,true,[&](const auto& rows,int){sx::watched_history::Snapshot s;assert(sx::watched_history::decode(rows,s)&&s.watched.count("tt123")&&s.watched.count("tmdb:9"));});
    for(bool season:{true,false}) {
        p=page();p[season?"seasons":"shows"].push_back({{season?"season":"show",season?json{{"number",1},{"show",media()}}:media()},
            {"last_watched_at","2025-10-15T20:00:00Z"}});
        value(p,true,[&](const auto& rows,int){sx::watched_history::Snapshot s;assert(sx::watched_history::decode(rows,s));
            assert(rows.empty()&&s.watched.empty());});
    }
    p=page();auto unmarked=episode();unmarked["last_watched_at"]=nullptr;p["episodes"].push_back(unmarked);
    value(p,true,[&](const auto& rows,int){sx::watched_history::Snapshot s;assert(sx::watched_history::decode(rows,s)&&s.watched.empty());});
    value(page(),true,[&](const auto& rows,int){assert(rows.empty());});
    run([](const auto& url,int){auto out=page();const bool second=url.find("offset=100")!=std::string::npos;
        for(int i=second?101:1;i<=(second?101:100);++i)out["episodes"].push_back(episode(i));
        out["pagination"]["has_more"]=!second;out["pagination"]["total_episodes"]=101;return out;},true,
        [&](const auto& rows,int count){assert(rows.size()==101&&rows.back()["episode"]==101&&count==2);});
    // Legacy cursor continuation is still supported, never substituted for documented offsets.
    run([](const auto& url,int){auto out=page();const bool second=url.find("cursor=next")!=std::string::npos;out["episodes"].push_back(episode(second?2:1));
        out["pagination"]["next_cursor"]=second?json(nullptr):json("next");out["pagination"]["has_more"]=!second;return out;},true,
        [&](const auto& rows,int count){assert(rows.size()==2&&count==2);});
    for(int mode=0;mode<12;++mode) {
        p=page();p["episodes"].push_back(episode());
        if(mode==0)p.erase("seasons");
        if(mode==1)p["episodes"]="bad";
        if(mode==2)p["episodes"][0]["episode"].erase("show");
        if(mode==3)p["episodes"][0]["episode"]["number"]=-1;
        if(mode==4)p["episodes"][0]["episode"]["season"]=1.5;
        if(mode==5)p["episodes"][0]["last_watched_at"]="not-a-date";
        if(mode==6)p["pagination"]["has_more"]="false";
        if(mode==7)p["pagination"]["total_episodes"]=2;
        if(mode==8)p["pagination"]["has_more"]=true; // next page repeats the same identity
        if(mode==9)p["pagination"]["next_cursor"]=1;
        if(mode==10){p["shows"].push_back({{"show",media()}});} // activity summary without a date is irrelevant to explicit history
        if(mode==11){p["pagination"]["has_more"]=true;p["pagination"]["limit"]=0;}
        value(p,mode==10);
    }
    run([](const auto&,int){auto out=page();out["pagination"]["has_more"]=true;return out;},false,[&](const auto&,int n){assert(n==1);});
    run([](const auto&,int n){auto out=page();out["episodes"].push_back(episode(n));out["pagination"]["has_more"]=true;out["pagination"]["limit"]=1;return out;},false,
        [&](const auto&,int n){assert(n==100);});
    // Sanitized real-response shapes from #22: activity counts are never evidence
    // of episode identity, including a summary whose counts happen to be equal.
    for(const auto& [marked,total]:{std::pair{3,19},std::pair{1,512},std::pair{2,34},std::pair{8,8}}) {
        p=page();auto show=media();show["watched_episodes"]=marked;show["total_aired_episodes"]=total;
        p["shows"].push_back({{"show",show},{"last_watched_at","2026-11-01T00:00:00Z"}});
        p["seasons"].push_back({{"season",{{"number",1},{"show",media()}}},{"last_watched_at","2026-11-01T00:00:00Z"}});
        for(int n=1;n<=marked;++n)p["episodes"].push_back(episode(n));
        value(p,true,[&](const auto& rows,int){sx::watched_history::Snapshot snapshot;assert(sx::watched_history::decode(rows,snapshot));
            assert(rows.size()==std::size_t(marked));
            for(int n=1;n<=marked;++n)assert(sx::watched_history::episode(snapshot.watched,"tt123","custom",1,n));
            assert(!sx::watched_history::episode(snapshot.watched,"tt123","custom",1,marked+1));
            assert(!sx::watched_history::episode(snapshot.watched,"tt123","custom",2,1));
            assert(!snapshot.watched.count("tt123"));});
    }
    {++cases;sx::watched_history::Snapshot snapshot;
     assert(sx::watched_history::decode(json::array({{{"content_id","tt123"},{"scope","show"}},{{"content_id","tt123"},{"scope","season"},{"season",1}}}),snapshot)&&snapshot.watched.empty());}
    p=page();p["shows"].push_back({{"show",{{"watched_episodes",1},{"total_aired_episodes",512}}}});
    p["seasons"].push_back({{"season",{{"number",1}}}});p["episodes"].push_back(episode());
    value(p,true,[&](const auto& rows,int){assert(rows.size()==1&&rows[0]["episode"]==1);});
    for(auto bad:std::vector<json>{json::object(),json::array({json::object()}),json::array({{{"content_id","tt1"},{"season",1},{"episode",nullptr}}}),
        json::array({{{"content_id","tt1"},{"season",1},{"episode",1},{"watched_at",1e100}}})}) {
        ++cases;sx::watched_history::Snapshot s;s.watched.insert("keep");assert(!sx::watched_history::decode(bad,s)&&s.watched.count("keep"));
    }
    {++cases;std::set<std::string> watched;
     sx::watched_history::completed(watched,"series","tt123","unknown-native",-1,-1);
     assert(watched==std::set<std::string>{"unknown-native"});
     sx::watched_history::completed(watched,"series","tt123","native",1,1);
     assert(watched.count("tt123:1:1"));
     sx::watched_history::completed(watched,"movie","tt456","movie-native",-1,-1);assert(watched.count("tt456"));}
    std::filesystem::remove_all(root);
    std::cout<<"MDBList watched history: "<<cases<<" regression cases passed\n";
}
