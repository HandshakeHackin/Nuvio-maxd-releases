// Production tracker adapters with deterministic official-contract fixtures.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tracking.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

using tracking::Store;
struct Fake {
    struct Call { std::string url; bool post; json body; std::vector<std::string> headers; };
    std::vector<Call> calls;
    int device_status=0;
    bool offline=false, unauthorized=false, malformed=false, reject_key=false, pending=false, paginate=false, reject_item=false;
    HttpResponse operator()(const std::string& url,const std::string* body,const std::vector<std::string>& headers) {
        assert(starts_with(url,"https://api.trakt.tv/") || starts_with(url,"https://api.simkl.com/") || starts_with(url,"https://api.mdblist.com/"));
        calls.push_back({url,body!=nullptr,body ? json::parse(*body) : json(),headers});
        if(reject_item && body && calls.back().body.dump().find("tt404")!=std::string::npos) return {422,"{}","","",0};
        json out; long status=200;
        if (offline) return {0,"private-token URL must never be echoed","secret transport URL","",0};
        if (unauthorized) return {401,"secret response body", "","",0};
        if (url.find("/oauth/device/code")!=std::string::npos) {
            assert(body && calls.back().body["client_id"]=="client");
            out={{"device_code","private-device"},{"user_code","ABCD"},{"expires_in",600},{"interval",2},{"verification_url","https://untrusted.example/steal"}};
        } else if (url.find("/oauth/device/token")!=std::string::npos || url.find("/oauth/token")!=std::string::npos) {
            assert(body);
            if(device_status)return {device_status,"{}","","",0};
            if (pending) return {400,"{}","","",0};
            out={{"access_token","private-access"},{"refresh_token","private-refresh"},{"created_at",now_epoch_ms()/1000},{"expires_in",86400}};
        } else if (url.find("/oauth/pin?")!=std::string::npos) {
            assert(!body); out={{"result","OK"},{"user_code","PIN"},{"expires_in",600},{"interval",3}};
        } else if (url.find("/oauth/pin/PIN")!=std::string::npos) {
            assert(!body); out=pending ? json{{"result","KO"}} : json{{"result","OK"},{"access_token","private-simkl"}};
        } else if (url.find("/users/settings")!=std::string::npos) {
            assert(bool(body)==(url.find("simkl")!=std::string::npos));
            out={{"user",{{"name","Tester"}}},{"account",{{"id",123}}}};
        } else if (url.find("mdblist.com/user?")!=std::string::npos) {
            assert(url.find("apikey=")!=std::string::npos); out={{"user_id",123}};
            if (reject_key) return {401,"private-rejected-key","","",0};
        } else if(url.find("/sync/activities")!=std::string::npos) {
            assert(body);out={{"all","2026-10-18T00:00:00Z"}};
        } else if (url.find("/sync/watchlist?")!=std::string::npos) {
            assert(!body); out=json::array({{{"movie",{{"title","Movie"},{"ids",{{"imdb","tt123"}}}}}}});
        } else if (url.find("/watchlist/items?")!=std::string::npos) {
            out={{"movies",json::array({{{"title","MDB movie"},{"imdb_id",url.find("cursor=next")==std::string::npos ? "tt123" : "tt456"}}})},{"shows",json::array()},
                {"pagination",{{"next_cursor",paginate && url.find("cursor=next")==std::string::npos ? json("next") : json(nullptr)}}}};
        } else if (url.find("/sync/all-items?")!=std::string::npos) {
            out=json::parse(R"({"movies":[{"status":"completed","movie":{"title":"SIMKL movie","ids":{"imdb":"tt123"}}}],"shows":[{"show":{"ids":{"imdb":"tt789"}},"seasons":[{"number":1,"episodes":[{"number":2,"watched_at":"2026-10-18T00:00:00Z"}]}]}]})");
        } else if (url.find("/sync/playback")!=std::string::npos) {
            assert(!body); out=json::array({{{"id",42},{"progress",25},{"runtime",100},{"paused_at","2026-10-18T00:00:00Z"},
                {"movie",{{"title","Movie"},{"runtime",100},{"ids",{{"imdb","tt123"}}}}}},
                {{"progress",50},{"runtime",40},{"paused_at","2026-10-18T00:00:00Z"},{"show",{{"ids",{{"imdb","tt789"}}}}},{"episode",{{"season",1},{"number",2}}}},
                {{"progress",999},{"movie",{{"runtime",100},{"ids",{{"imdb","ttbad"}}}}}}});
        } else if (url.find("/sync/watched/movies")!=std::string::npos) out=json::array({{{"movie",{{"ids",{{"imdb","tt123"}}}}}}});
        else if (url.find("/sync/watched/shows")!=std::string::npos) out=json::array();
        else if (url.find("/sync/watched?")!=std::string::npos) out={{"movies",json::array({{{"movie",{{"ids",{{"imdb","tt123"}}}}}}})}};
        else if (url.find("/scrobble/")!=std::string::npos || url.find("/sync/history")!=std::string::npos || url.find("/sync/watched")!=std::string::npos) { assert(body); out={{"action","pause"}}; }
        else { std::cerr<<"Unexpected fixture path "<<url<<"\n"; assert(false); }
        return {status,malformed ? "not json" : out.dump(),"","",0};
    }
};
int main() {
    const auto root="/tmp/nuvio-tracking-"+std::to_string(getpid());
    std::filesystem::create_directories(root);
    Fake fake;
    auto transport=[&](const auto& url,const auto* body,const auto& headers){return fake(url,body,headers);};
    Store store(root,transport);
    std::string error;
    auto action=[&](const auto& op,const json& args){error.clear();auto r=store.action(op,args,error);assert(error.empty());return r;};
    assert(store.source(false)=="nuvio" && store.source(true)=="nuvio" && fake.calls.empty());
    store.action("source",{{"field","library"},{"value","trakt"}},error); assert(!error.empty());
    action("configure",{{"provider","trakt"},{"client_id","client"},{"client_secret","private-secret"}});
    auto code=action("device_start",{{"provider","trakt"}}); assert(code["url"]=="https://trakt.tv/activate");
    assert(code.dump().find("private-device")==std::string::npos);
    fake.pending=true; auto waiting=action("device_poll",{{"provider","trakt"}}); assert(waiting["pending"]==true);
    fake.pending=false; action("device_poll",{{"provider","trakt"}});
    auto status=store.status(); assert(status["accounts"]["trakt"]["connected"]==true);
    assert(status.dump().find("private-")==std::string::npos);
    assert(store.source(true)=="nuvio" && store.source(false)=="nuvio"); // connection never opts in
    action("source",{{"field","library"},{"value","trakt"}});
    assert(store.source(true)=="nuvio");
    json library,progress;
    assert(store.library(library,error) && library.size()==1 && library[0]["_id"]=="tt123");
    action("source",{{"field","progress"},{"value","trakt"}});
    assert(store.progress(progress,error) && progress.size()==2);
    assert(progress[0]["position"]==1500000 && progress[0]["duration"]==6000000);
    assert(progress[1]["video_id"]=="tt789:1:2");
    auto item=json{{"_id","tt123"},{"type","movie"},{"stream","https://secret-stream.invalid/token"},
        {"state",{{"timeOffset",600000},{"duration",6000000},{"lastWatchedMs",now_epoch_ms()}}}};
    fake.offline=true; error.clear(); assert(!store.write(json::array({item}),error));
    assert(error.find("private-token")==std::string::npos && error.find("secret") == std::string::npos);
    json disk; assert(load_json(root+"/tracking.json",disk));
    assert(disk["pending_trakt"].size()==1 && disk.dump().find("secret-stream")==std::string::npos);
    assert(!store.write(json::array({item}),error)); load_json(root+"/tracking.json",disk); assert(disk["pending_trakt"].size()==1);
    struct stat st{}; assert(stat((root+"/tracking.json").c_str(),&st)==0 && (st.st_mode&0777)==0600);
    fake.offline=false; Store restored(root,transport); error.clear(); assert(restored.progress(progress,error));
    load_json(root+"/tracking.json",disk); assert(disk["pending_trakt"].empty());
    bool found=false;for(const auto& c:fake.calls) if(c.url=="https://api.trakt.tv/scrobble/pause") {assert(c.body["progress"]==10);found=true;} assert(found);
    auto episode=item; episode["_id"]="tt789"; episode["type"]="series";
    episode["state"]["nuvioSeason"]=1;episode["state"]["nuvioEpisode"]=2;episode["state"]["trackingFinal"]=true;
    error.clear();assert(restored.write(json::array({episode}),error));
    assert(fake.calls.back().url=="https://api.trakt.tv/scrobble/stop" && fake.calls.back().body["episode"]["number"]==2);
    json watched;assert(restored.watched(watched,error) && watched.size()==1);
    assert(restored.mark("tt789",1,2,false,error));
    assert(fake.calls.back().url=="https://api.trakt.tv/sync/history/remove");
    assert(fake.calls.back().body["shows"][0]["seasons"][0]["episodes"][0]["number"]==2);
    auto bad=item; bad["_id"]="unknown:unsafe";error.clear();assert(!restored.write(json::array({bad}),error));
    fake.unauthorized=true;error.clear();assert(!restored.progress(progress,error));assert(restored.source(true)=="nuvio");
    assert(restored.status()["progressPreferred"]=="trakt");fake.unauthorized=false;
    restored.action("disconnect",{{"provider","trakt"}},error); assert(restored.status()["progressPreferred"]=="nuvio");
    // MDBList validates before replacing a key, supports cursor pagination and independent selectors.
    Store mdb(root,transport);error.clear();fake.reject_key=true;
    mdb.action("key",{{"provider","mdblist"},{"api_key","rejected"}},error);assert(!error.empty() && !mdb.status()["accounts"]["mdblist"]["connected"].get<bool>());
    fake.reject_key=false;error.clear();mdb.action("key",{{"provider","mdblist"},{"api_key","private-mdb"}},error);assert(error.empty());
    mdb.action("source",{{"field","library"},{"value","mdblist"}},error);
    fake.paginate=true;assert(mdb.library(library,error) && library.size()==2);fake.paginate=false;
    mdb.action("source",{{"field","progress"},{"value","mdblist"}},error);
    assert(mdb.write(json::array({episode}),error));
    assert(fake.calls.back().body["show"]["season"]==1 && fake.calls.back().body["show"]["episode"]==2);
    auto removal=episode;removal["state"]["nuvioRemoveProgress"]=true;
    assert(mdb.write(json::array({removal}),error) && fake.calls.back().url.find("/scrobble/clear?")!=std::string::npos);
    assert(mdb.mark("tt123",-1,-1,true,error) && fake.calls.back().url.find("/sync/watched?")!=std::string::npos);
    assert(mdb.progress(progress,error) && mdb.watched(watched,error));
    // SIMKL PIN flow, POST user validation, tracked library and resume.
    mdb.action("configure",{{"provider","simkl"},{"client_id","client"}},error);
    auto simkl=mdb.action("device_start",{{"provider","simkl"}},error);assert(error.empty() && simkl["url"]=="https://simkl.com/pin");
    fake.pending=true; assert(mdb.action("device_poll",{{"provider","simkl"}},error)["pending"]==true);fake.pending=false;
    mdb.action("device_poll",{{"provider","simkl"}},error);assert(error.empty());
    mdb.action("source",{{"field","library"},{"value","simkl"}},error);
    mdb.action("source",{{"field","progress"},{"value","simkl"}},error);
    assert(mdb.library(library,error) && library.size()==2 && mdb.watched(watched,error) && watched.size()==2);
    assert(mdb.progress(progress,error) && mdb.write(json::array({episode}),error));
    assert(fake.calls.back().url=="https://api.simkl.com/scrobble/stop");
    // App refresh rotation, request origins and headers.
    load_json(root+"/tracking.json",disk);
    disk["accounts"]["trakt"]={{"verified",true},{"client_id","client"},{"client_secret","private-secret"},{"access_token","old"},{"refresh_token","old-refresh"},{"expires_at",1}};
    disk["progress"]="trakt";save_json(root+"/tracking.json",disk);Store expiring(root,transport);
    assert(expiring.progress(progress,error));load_json(root+"/tracking.json",disk);assert(disk["accounts"]["trakt"]["refresh_token"]=="private-refresh");
    bool refresh=false;for (const auto& c:fake.calls) if(c.url=="https://api.trakt.tv/oauth/token") {assert(c.body["grant_type"]=="refresh_token");refresh=true;}assert(refresh);
    // One unsupported remote title cannot block other queued progress.
    fake.reject_item=true;auto rejected=item;rejected["_id"]="tt404";
    error.clear();assert(!expiring.write(json::array({rejected,item}),error));
    assert(expiring.unsent(rejected) && !expiring.unsent(item));
    load_json(root+"/tracking.json",disk);assert(disk["pending_trakt"].empty() && disk["failed_trakt"].size()==1);
    fake.reject_item=false;
    // A different profile has no connections or choices, and malformed state is not overwritten.
    std::filesystem::create_directories(root+"/other");Store other(root+"/other",transport);assert(other.source(true)=="nuvio" && !other.status()["accounts"]["trakt"]["connected"].get<bool>());
    write_file(root+"/other/tracking.json","unreadable");Store broken(root+"/other",transport);
    error.clear();broken.action("disconnect",{{"provider","trakt"}},error);assert(!error.empty());
    std::string raw;read_file(root+"/other/tracking.json",raw);assert(raw=="unreadable");
    // Cancelled approval cannot persist a connection, even if its transport returns a token.
    std::filesystem::create_directories(root+"/cancel");
    std::atomic<bool> cancelled{false}; Store setup(root+"/cancel",transport);
    error.clear(); setup.action("configure",{{"provider","simkl"},{"client_id","client"}},error);
    setup.action("device_start",{{"provider","simkl"}},error);
    auto cancel_transport=[&](const auto& url,const auto* body,const auto& headers) {
        auto response=fake(url,body,headers);
        if(url.find("/oauth/pin/PIN")!=std::string::npos) cancelled=true;
        return response;
    };
    Store cancelling(root+"/cancel",cancel_transport,&cancelled);
    cancelling.action("device_poll",{{"provider","simkl"}},error);
    Store after_cancel(root+"/cancel",transport);
    assert(!after_cancel.status()["accounts"]["simkl"]["connected"].get<bool>());
    // Owner-bundled app registrations make normal phone pairing available
    // without opting anyone into an account or a different source.
    tracking::set_applications({{"trakt",{{"client_id","client"},{"client_secret","registered-app-secret"}}}});
    std::filesystem::create_directories(root+"/bundled");Store bundled(root+"/bundled",transport);
    assert(bundled.status()["accounts"]["trakt"]["configured"].get<bool>());
    assert(!bundled.status()["accounts"]["trakt"]["connected"].get<bool>() && bundled.source(true)=="nuvio");
    error.clear();assert(bundled.action("device_start",{{"provider","trakt"}},error)["url"]=="https://trakt.tv/activate");
    for(int terminal:{404,409,410,418}) {
        error.clear();bundled.action("device_start",{{"provider","trakt"}},error);assert(error.empty());
        fake.device_status=terminal;bundled.action("device_poll",{{"provider","trakt"}},error);assert(!error.empty());
        fake.device_status=0;error.clear();bundled.action("device_poll",{{"provider","trakt"}},error);assert(error.find("expired")!=std::string::npos);
    }
    error.clear();bundled.action("device_start",{{"provider","trakt"}},error);assert(error.empty());
    fake.device_status=429;auto slow=bundled.action("device_poll",{{"provider","trakt"}},error);assert(error.empty() && slow["pending"]==true && slow["slow"]==true);fake.device_status=0;
    // An interrupted older configuration with a matching public client ID
    // can be completed by the owner's registered application, without tokens.
    std::filesystem::create_directories(root+"/partial");
    save_json(root+"/partial/tracking.json",json{{"version",1},{"accounts",{{"trakt",{{"client_id","client"}}}}}});
    Store repaired(root+"/partial",transport);assert(repaired.status()["accounts"]["trakt"]["configured"]==true);
    tracking::set_applications(json::object());
    Store unconfigured(root+"/unconfigured",transport);
    assert(unconfigured.status()["accounts"]["trakt"]["configured"]==false);
    error.clear();unconfigured.action("device_start",{{"provider","trakt"}},error);assert(error.find("no registered")!=std::string::npos);
    std::filesystem::remove_all(root);
    std::cout<<"Tracker contracts passed: pairing, validation, refresh, sources, pagination, resume, scrobble, watched marks, offline retry, redaction, permissions and profile isolation\n";
}
