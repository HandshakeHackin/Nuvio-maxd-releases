#include "stremio.h"
#include "cloudstream.h"
#include "http.h"
#include <cassert>
#include <iostream>

static json plugins;
static std::vector<std::string> calls;
static bool duplicate=false,foreign=false,fail=false;
std::string HttpResponse::describe() const {return error.empty()?"HTTP "+std::to_string(status):error;}
HttpResponse http_get_manifest(const std::string& url,long timeout,const std::atomic<bool>* cancel,bool pinned) {
    assert(pinned && timeout>0 && timeout<=15);
    calls.push_back(url);
    if(cancel && *cancel)return {0,"","cancelled",""};
    if(fail)return {404,"","",""};
    if(url==cloudstream::source_url) {
        json repo{{"name","German providers repository"},{"manifestVersion",1},
            {"pluginLists",json::array({foreign?"https://127.0.0.1/private":cloudstream::list_url})}};
        return {200,repo.dump(),"","application/json"};
    }
    assert(url==cloudstream::list_url);
    auto list=plugins;if(duplicate)list.push_back(list[0]);
    return {200,list.dump(),"","application/json"};
}
HttpResponse http_get(const std::string&,long,const std::atomic<bool>*,const std::vector<std::string>&) {assert(false);return {};}
int main(int argc,char** argv) {
    assert(argc==2 && load_json(argv[1],plugins));
    assert(normalize_addon_url("https://raw.githubusercontent.com/Bnyro/GermanProviders/main/repo.json")==cloudstream::source_url);
    assert(normalize_addon_url("https://repo.example/manifest?private=fixture#ignored")=="https://repo.example/manifest?private=fixture");
    assert(normalize_addon_url("https://repo.example/custom.json?x=fixture")=="https://repo.example/custom.json?x=fixture");
    assert(normalize_addon_url("https://repo.example/ConfigToken?x=fixture")=="https://repo.example/ConfigToken/manifest.json?x=fixture");
    assert(!cloudstream::known_source("https://raw.githubusercontent.com.evil.invalid/Bnyro/GermanProviders/master/repo.json"));
    assert(!cloudstream::known_source(std::string(cloudstream::source_url)+"?private=fixture"));
    const std::string alias="https://raw.githubusercontent.com/Bnyro/GermanProviders/main/repo.json";
    std::vector<std::string> extras{alias,cloudstream::source_url};
    json enabled={{alias,{{"kellerkino",true}}},{cloudstream::source_url,{{"ard",false}}}};
    json settings={{alias,{{"kellerkino",{{"language","de"}}}}}};
    assert(cloudstream::migrate_aliases(extras,enabled,settings));
    assert(extras==std::vector<std::string>{cloudstream::source_url});
    assert(enabled[cloudstream::source_url]["kellerkino"]==true && enabled[cloudstream::source_url]["ard"]==false);
    assert(settings[cloudstream::source_url]["kellerkino"]["language"]=="de");
    assert(!cloudstream::migrate_aliases(extras,enabled,settings));
    Addon a;std::string error;
    assert(fetch_addon(cloudstream::source_url,a,error));
    assert(a.cloudstream_repository && a.providers.size()==20 && a.unsupported_providers.size()==7);
    assert(calls==std::vector<std::string>({cloudstream::source_url,cloudstream::list_url}));
    for(const auto& p:a.providers)assert(!p.enabled && !p.native_port.empty() && p.filename=="builtin:cloudstream/"+p.id);
    assert(!a.supports("stream","movie","tmdb:1"));
    Addon restored;assert(parse_addon(a.manifest,a.transport_url,restored));
    assert(restored.cloudstream_repository && restored.providers.size()==20);
    for(auto& p:restored.providers)if(p.id=="kellerkino")p.enabled=true;
    assert(restored.supports("stream","movie","tmdb:1") && !restored.supports("stream","series","tmdb:1:1:1"));
    restored.unavailable=true;assert(!restored.supports("stream","movie","tmdb:1"));
    auto malformed=a.manifest;malformed["scrapers"][0]["nativePort"]="../../private";
    assert(parse_addon(malformed,a.transport_url,restored) && restored.providers.size()==19);
    duplicate=true;assert(!fetch_addon(cloudstream::source_url,a,error));duplicate=false;
    calls.clear();foreign=true;assert(!fetch_addon(cloudstream::source_url,a,error));foreign=false;assert(calls.size()==1);
    fail=true;assert(!fetch_addon(cloudstream::source_url,a,error) && error=="HTTP 404");fail=false;
    std::atomic<bool> cancel=true;assert(!fetch_addon(cloudstream::source_url,a,error,&cancel));
    json empty;assert(!cloudstream::adapt(json::object(),plugins,empty,error));
    std::cout<<"CloudStream import: canonical URL, 20 native ports, seven unavailable entries, profile defaults, serialized restore, strict origin/list/ID guards, cancellation and HTTP404 passed\n";
}
