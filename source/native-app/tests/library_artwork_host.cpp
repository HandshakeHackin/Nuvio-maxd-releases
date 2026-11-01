#include "library_artwork_boundary.hpp"
#include <cassert>
#include <iostream>
#include "http.h"
std::vector<std::string> requests;
bool offline=false;
std::string HttpResponse::describe() const { return "fixture unavailable"; }
HttpResponse http_get(const std::string& url,long,const std::atomic<bool>*,const std::vector<std::string>&) {
    requests.push_back(url);
    if(offline) return {503,"","fixture unavailable","",0};
    json out={{"meta",{{"name","Resolved"},{"poster",url.find("no-poster")!=std::string::npos ? "" : "https://images.example/poster.jpg"}}}};
    return {200,out.dump(),"","application/json",0};
}

int main() {
    sx::Session s;
    for(auto base:{"https://no-poster.example","https://metadata.example"}) {
        auto a=std::make_unique<Addon>();a->base=base;a->types={"movie","series"};
        AddonResource r;r.name="meta";a->resources.push_back(r);s.addons_.push_back(std::move(a));
    }
    for(int i=0;i<80;++i) {
        auto id="tt"+std::to_string(10000+i);
        s.library_[id]={{"name","Title "+std::to_string(i)},{"type",i==1?"series":"movie"},{"poster",i==0?"https://existing.example/poster.jpg":""}};
    }
    s.refresh_library();s.refresh_art({});
    assert(g_tasks.pending.size()==4 && s.meta_inflight_==4); // bounded cold Library hydration
    s.refresh_art({});assert(g_tasks.pending.size()==4); // no duplicate requests
    g_tasks.one();
    assert(requests.size()==2 && requests[0].find("/meta/series/tt10001.json")!=std::string::npos);
    assert(s.lib_items_[1].poster=="https://images.example/poster.jpg");
    s.refresh_art({});
    assert(s.library.items[1].poster==1); // metadata completion reaches the image loader
    assert(s.lib_items_[0].poster=="https://existing.example/poster.jpg");
    while(!g_tasks.pending.empty())g_tasks.one();
    auto count=requests.size();s.refresh_art({60});
    assert(g_tasks.pending.size()==4);g_tasks.one();
    assert(requests.size()==count+2); // scrolling loads another window without Board
    ++s.profile_generation_;s.meta_cache_.clear();s.meta_asked_.clear();s.meta_inflight_=0;
    while(!g_tasks.pending.empty())g_tasks.one();
    assert(s.meta_cache_.empty() && s.meta_inflight_==0); // old-profile results discarded
    offline=true;s.refresh_library();s.refresh_art({});
    while(!g_tasks.pending.empty())g_tasks.one();
    assert(s.meta_inflight_==0);count=requests.size();s.refresh_art({});
    assert(g_tasks.pending.size()<=4); // failures do not create an unbounded retry loop
    while(!g_tasks.pending.empty())g_tasks.one();
    s.page_=sx::Page::board;s.meta_asked_.clear();s.refresh_art({});assert(g_tasks.pending.empty());
    std::cout<<"PASS: cold Library posters, addon fallback, existing art, scrolling, request bounds, offline handling and profile isolation\n";
}
