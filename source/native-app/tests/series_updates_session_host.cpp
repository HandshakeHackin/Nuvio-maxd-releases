#include "library_artwork_boundary.hpp"
#include "http.h"
#include <cassert>
#include <iostream>
int requests=0;bool offline=false;
std::string HttpResponse::describe()const{return "fixture unavailable";}
HttpResponse http_get(const std::string&,long,const std::atomic<bool>*,const std::vector<std::string>&) {
    ++requests;if(offline)return {503,"","fixture unavailable","",0};
    json meta={{"id","show"},{"name","Fixture series"},{"videos",json::array({{{"id","show:1:1"},{"season",1},{"episode",1},{"released","2020-01-01T00:00:00Z"}},{{"id","show:1:2"},{"season",1},{"episode",2},{"released","2020-01-02T00:00:00Z"}}})}};
    return {200,json({{"meta",meta}}).dump(),"","application/json",0};
}
int main(){
    sx::Session s;auto a=std::make_unique<Addon>();a->base="https://metadata.example";AddonResource resource;resource.name="meta";a->resources.push_back(resource);s.addons_.push_back(std::move(a));
    sx::Item item;item.id="show";item.type="series";item.name="Fixture series";
    s.library_["show"]={{"type","series"},{"name",item.name}};s.watched_.insert("show:1:1");
    s.library_["show"]["state"]={{"timeOffset",1000},{"video_id","show:1:2"}};
    assert(s.make_title(item,false).new_episode.empty()); // Board cards use Library progress too.
    s.library_["show"].erase("state");
    s.want_series_updates(item);assert(g_tasks.pending.empty()); // Unknown remote history never implies new episodes.
    s.series_history_ready_=true;s.want_series_updates(item);s.want_series_updates(item);assert(g_tasks.pending.size()==1&&s.series_updates_inflight_==1);
    g_tasks.one();assert(requests==1&&s.series_updates_inflight_==0&&s.library.items[0].new_episode=="New: S1 E2");
    s.want_series_updates(item);assert(g_tasks.pending.empty());
    s.watched_.insert("show:1:2");s.refresh_library();assert(s.library.items[0].new_episode.empty());
    s.series_updates_reset();s.series_history_ready_=true;s.want_series_updates(item);s.series_updates_reset();g_tasks.one();
    assert(s.series_updates_inflight_==0&&s.series_updates_.due("show",now_seconds())); // No stale failure-cache insertion after a source reset.
    s.series_history_ready_=true;offline=true;s.want_series_updates(item);g_tasks.one();
    assert(s.series_updates_inflight_==0&&!s.series_updates_.due("show",now_seconds())&&s.library.items[0].new_episode.empty());
    s.series_updates_reset();s.series_history_ready_=true;s.want_series_updates(item);++s.profile_generation_;s.series_updates_reset();g_tasks.one();assert(s.series_updates_.due("show",now_seconds()));
    std::cout<<"PASS: production series requests, deduplication, card badge updates, watched marks, cooldown and stale source/profile replies\n";
}
