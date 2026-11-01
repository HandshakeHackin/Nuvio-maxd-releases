#include "series_updates.hpp"
#include <cassert>
#include <iostream>
int main() {
    sx::SeriesUpdates cache;auto now=*airtime::iso("2026-11-01T12:30:00Z");
    auto episode=[](int s,int e,const std::string& release){Video v;v.id="show:"+std::to_string(s)+":"+std::to_string(e);v.season=s;v.episode=e;v.released=release;return v;};
    std::vector<Video> videos{episode(1,1,"2026-10-01T00:00:00Z"),episode(1,2,"2026-11-01T07:00:00-05:00"),episode(1,3,"2026-11-02T00:00:00Z"),episode(0,1,"2026-10-01T00:00:00Z")};
    cache.put("show",videos,100);std::set<std::string> watched{"show:1:1"};
    assert(cache.badge("show",watched,now,true)=="New: S1 E2");
    assert(cache.badge("show",watched,now,false).empty()&&cache.badge("show",{},now,true).empty());
    assert(cache.badge("show",watched,now,true,"show:1:2").empty());
    watched.insert("show:1:2");assert(cache.badge("show",watched,now,true).empty());
    watched.erase("show:1:2");videos[1].released="";cache.put("show",videos,100);assert(cache.badge("show",watched,now,true).empty());
    videos[1].released="2026-02-30T12:00:00Z";cache.put("show",videos,100);assert(cache.badge("show",watched,now,true).empty());
    videos[1].released="2026-11-01";cache.put("show",videos,100);assert(cache.badge("show",watched,now,true).empty());
    assert(cache.badge("show",watched,now+86400,true)=="New: S1 E2");
    assert(!cache.due("show",3599)&&cache.due("show",3701));
    cache.put("show",{},3800,false);assert(!cache.due("show",4000)&&cache.due("show",4101));
    auto aliases=videos;aliases[0].id="provider-episode-one";aliases[1].id="provider-episode-two";aliases[1].released="2020-01-01T00:00:00Z";
    cache.put("aliases",aliases,4200);
    assert(cache.badge("aliases",{"aliases:1:1"},now,true)=="New: S1 E2");
    assert(cache.badge("aliases",{"provider-episode-one"},now,true)=="New: S1 E2");
    assert(cache.badge("aliases",{"aliases:1:1"},now,true,"aliases:1:2").empty());
    cache.clear();assert(cache.badge("show",watched,now,true).empty()&&cache.due("show",1));
    for(int i=0;i<130;++i)cache.put(std::to_string(i),videos,i);
    assert(cache.due("0",131)&&!cache.due("129",131));
    std::cout<<"PASS: aired-only series badges, known history, future/unknown/date-only releases, in-progress exclusion, TTL and cache bounds\n";
}
