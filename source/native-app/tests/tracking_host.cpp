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
    int device_status=0, simkl_refresh_status=200, simkl_sync_revision=0;
    std::string simkl_poll_error;
    bool simkl_read_only=false, simkl_stale=false;
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
            assert(body && !calls.back().body.contains("client_secret") && !calls.back().body.contains("redirect_uri"));
            if(device_status)return {device_status,"{}","","",0};
            if (pending) return {400,"{}","","",0};
            out={{"access_token","private-access"},{"refresh_token","private-refresh"},{"created_at",now_epoch_ms()/1000},{"expires_in",86400}};
        } else if (url=="https://api.simkl.com/oauth2/device") {
            assert(body && calls.back().body["client_id"]=="client" && calls.back().body["scope"]=="media:read media:write");
            out={{"device_code","private-simkl-device"},{"user_code","ABCD-EFGH"},{"expires_in",900},{"interval",5},
                 {"verification_uri_complete","https://untrusted.example/steal"}};
        } else if (url=="https://api.simkl.com/oauth2/token") {
            const auto& form=calls.back().body;
            assert(body && form["client_id"]=="client" && !form.contains("client_secret"));
            if (form["grant_type"]=="refresh_token") {
                assert(form["refresh_token"]=="simkl_rt_fixture" && !form.contains("device_code"));
                if (simkl_refresh_status!=200)
                    return {simkl_refresh_status, simkl_refresh_status==400 ? R"({"error":"invalid_grant"})" : "{}", "", "", 0};
                simkl_stale=false;
            } else {
                assert(form["grant_type"]=="urn:ietf:params:oauth:grant-type:device_code" && form["device_code"]=="private-simkl-device");
                if (!simkl_poll_error.empty()) return {400,json({{"error",simkl_poll_error}}).dump(),"","",0};
            }
            out={{"access_token","simkl_at_fixture"},{"refresh_token","simkl_rt_fixture"},{"expires_in",604800},
                {"scope",simkl_read_only ? "media:read" : "media:read media:write"}};
        } else if (url.find("/users/settings")!=std::string::npos) {
            assert(bool(body)==(url.find("simkl")!=std::string::npos));
            out={{"user",{{"name","Tester"}}},{"account",{{"id",123}}}};
        } else if (url.find("mdblist.com/user?")!=std::string::npos) {
            assert(url.find("apikey=")!=std::string::npos); out={{"user_id",123}};
            if (reject_key) return {401,"private-rejected-key","","",0};
        } else if(url.find("/sync/activities")!=std::string::npos) {
            assert(bool(body)==(url.find("client_id=")==std::string::npos));
            const std::string ts="2026-10-18T00:00:0"+std::to_string(simkl_sync_revision)+"Z";
            out={{"all",ts},{"movies",{{"completed",ts},{"removed_from_list",simkl_sync_revision==2 ? ts : ""}}}};
        } else if (url.find("/sync/watchlist?")!=std::string::npos) {
            assert(!body); out=json::array({{{"movie",{{"title","Movie"},{"ids",{{"imdb","tt123"}}}}}}});
        } else if (url.find("/watchlist/items?")!=std::string::npos) {
            out={{"movies",json::array({{{"title","MDB movie"},{"imdb_id",url.find("cursor=next")==std::string::npos ? "tt123" : "tt456"}}})},{"shows",json::array()},
                {"pagination",{{"next_cursor",paginate && url.find("cursor=next")==std::string::npos ? json("next") : json(nullptr)}}}};
        } else if (url.find("/sync/all-items?")!=std::string::npos) {
            if (url.find("extended=simkl_ids_only")!=std::string::npos)
                return {200,R"({"movies":[{"movie":{"ids":{"simkl":321}}}]})","","",0};
            if (url.find("date_from=")!=std::string::npos)
                return {200,simkl_sync_revision==1
                    ? R"({"movies":[{"status":"completed","movie":{"title":"SIMKL delta","ids":{"imdb":"tt321","simkl":321}}}]})"
                    : "{}", "", "", 0};
            out=json::parse(R"({"movies":[{"status":"completed","movie":{"title":"SIMKL movie","ids":{"imdb":"tt123","simkl":123}}}],"shows":[{"show":{"ids":{"imdb":"tt789","simkl":789}},"seasons":[{"number":1,"episodes":[{"number":2,"watched_at":"2026-10-18T00:00:00Z"}]}]}]})");
        } else if (url.find("/sync/playback")!=std::string::npos) {
            assert(!body); out=json::array({{{"id",42},{"progress",25},{"runtime",100},{"paused_at","2026-10-18T00:00:00Z"},
                {"movie",{{"title","Movie"},{"runtime",100},{"ids",{{"imdb","tt123"}}}}}},
                {{"progress",50},{"runtime",40},{"paused_at","2026-10-18T00:00:00Z"},{"show",{{"ids",{{"imdb","tt789"}}}}},{"episode",{{"season",1},{"number",2}}}},
                {{"progress",999},{"movie",{{"runtime",100},{"ids",{{"imdb","ttbad"}}}}}}});
        } else if (url.find("/sync/watched/movies")!=std::string::npos) out=json::array({{{"movie",{{"ids",{{"imdb","tt123"}}}}}}});
        else if (url.find("/sync/watched/shows")!=std::string::npos) out=json::array();
        else if (url.find("/sync/watched?")!=std::string::npos) out={{"movies",json::array({{{"movie",{{"ids",{{"imdb","tt123"}}}}}}})}};
        else if (url.find("/scrobble/")!=std::string::npos || url.find("/sync/history")!=std::string::npos || url.find("/sync/watched")!=std::string::npos) {
            assert(body);
            if (starts_with(url,"https://api.simkl.com/") && url.find("client_id=")!=std::string::npos) {
                assert(url.find("client_id=client&app-name=nuvio-maxd&app-version=")!=std::string::npos);
                assert(std::find(headers.begin(),headers.end(),"Authorization: Bearer simkl_at_fixture")!=headers.end());
                if (simkl_stale) return {401,R"({"error":"invalid_token"})","","",0};
            }
            out={{"action","pause"}};
        }
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
    action("configure",{{"provider","trakt"},{"client_id","client"}});
    auto code=action("device_start",{{"provider","trakt"}}); assert(code["url"]=="https://auth.trakt.tv/activate");
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
    // A clean profile pairs with the bundled V2 client, never exposes the private device code,
    // rate-limits poll retries and doesn't connect on a read-only grant.
    const json simkl_args{{"provider","simkl"}};
    tracking::set_applications({{"simkl",{{"client_id","client"}}}});
    const auto poll_ready=[&](const std::string& dir) {
        json saved; assert(load_json(dir+"/tracking.json",saved));
        saved["pairing_simkl"]["next_poll"]=0; assert(save_json(dir+"/tracking.json",saved));
    };
    Store settings(root,transport);
    auto simkl=settings.action("device_start",simkl_args,error);
    assert(error.empty() && simkl["url"]=="https://simkl.com/pin" && simkl["qr_url"]=="https://simkl.com/pin?user_code=ABCD-EFGH");
    assert(simkl.dump().find("private-simkl-device")==std::string::npos);
    auto calls=fake.calls.size();
    assert(settings.action("device_poll",simkl_args,error)["pending"]==true && fake.calls.size()==calls);
    poll_ready(root);
    Store poll(root,transport);
    fake.simkl_poll_error="authorization_pending";
    assert(poll.action("device_poll",simkl_args,error)["interval"]==5 && error.empty());
    poll_ready(root); Store slow_poll(root,transport);
    fake.simkl_poll_error="slow_down";
    assert(slow_poll.action("device_poll",simkl_args,error)["interval"]==10 && error.empty());
    poll_ready(root); Store scoped_poll(root,transport);
    fake.simkl_poll_error.clear(); fake.simkl_read_only=true;
    scoped_poll.action("device_poll",simkl_args,error);
    assert(error.find("permission")!=std::string::npos && scoped_poll.status()["accounts"]["simkl"]["connected"]==false);
    fake.simkl_read_only=false; error.clear();
    scoped_poll.action("device_start",simkl_args,error); assert(error.empty());
    poll_ready(root); Store ready(root,transport);
    ready.action("device_poll",simkl_args,error); assert(error.empty());
    assert(ready.status()["accounts"]["simkl"]["connected"]==true && ready.status().dump().find("simkl_at_")==std::string::npos);
    assert(ready.source(false)=="mdblist" && ready.source(true)=="mdblist");
    ready.action("source",{{"field","library"},{"value","simkl"}},error);
    ready.action("source",{{"field","progress"},{"value","simkl"}},error);
    assert(ready.library(library,error) && library.size()==2 && ready.watched(watched,error) && watched.size()==2);
    assert(ready.progress(progress,error));
    calls=fake.calls.size(); fake.simkl_stale=true;
    assert(ready.write(json::array({episode}),error));
    assert(fake.calls.size()==calls+3 && fake.calls[calls+1].url=="https://api.simkl.com/oauth2/token");
    assert(starts_with(fake.calls.back().url,"https://api.simkl.com/scrobble/stop?"));
    assert(ready.mark("tt123",-1,-1,true,error) && starts_with(fake.calls.back().url,"https://api.simkl.com/sync/history?"));
    assert(load_json(root+"/tracking.json",disk)); disk["accounts"]["simkl"]["expires_at"]=1;
    assert(save_json(root+"/tracking.json",disk));
    Store expired_simkl(root,transport); fake.simkl_refresh_status=503; calls=fake.calls.size();
    assert(!expired_simkl.write(json::array({item}),error) && fake.calls.size()==calls+1);
    assert(expired_simkl.status()["accounts"]["simkl"]["connected"]==true);
    assert(load_json(root+"/tracking.json",disk) && disk["pending_simkl"].size()==1);
    fake.simkl_refresh_status=200;
    assert(expired_simkl.progress(progress,error) && load_json(root+"/tracking.json",disk) && disk["pending_simkl"].empty());
    // Updates use date_from, removals use a lightweight ID index, and an empty
    // watchlist is a valid snapshot, not an authentication or fallback error.
    const auto age_snapshot=[&] {
        assert(load_json(root+"/tracking.json",disk));
        disk["simkl_v2_library"]["checked"]=0;
        assert(save_json(root+"/tracking.json",disk));
    };
    age_snapshot(); fake.simkl_sync_revision=1;
    Store delta(root,transport);
    assert(delta.library(library,error) && library.size()==3);
    assert(fake.calls.back().url.find("date_from=")!=std::string::npos);
    age_snapshot(); fake.simkl_sync_revision=2;
    Store deletions(root,transport);
    assert(deletions.library(library,error) && library.size()==1 && library[0]["_id"]=="tt321");
    assert(fake.calls.back().url.find("extended=simkl_ids_only")!=std::string::npos);
    fake.simkl_sync_revision=0;
    age_snapshot();
    Store revoked(root,transport);
    fake.simkl_refresh_status=400; fake.simkl_stale=true;
    assert(!revoked.write(json::array({item}),error) && revoked.status()["accounts"]["simkl"]["reconnect"]==true);
    fake.simkl_refresh_status=200; fake.simkl_stale=false;
    // Saved legacy tokens continue using their original client ID and API contract.
    std::filesystem::create_directories(root+"/legacy");
    Store legacy(root+"/legacy",transport);
    error.clear();
    legacy.action("import",{{"provider","simkl"},{"credential",{{"client_id","old-owner"},{"access_token","legacy-token"}}}},error);
    assert(error.empty() && legacy.status()["accounts"]["simkl"]["connected"]==true);
    assert(std::find(fake.calls.back().headers.begin(),fake.calls.back().headers.end(),"simkl-api-key: old-owner")!=fake.calls.back().headers.end());
    assert(load_json(root+"/legacy/tracking.json",disk) && disk["accounts"]["simkl"]["client_id"]=="old-owner");
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
    setup.action("device_start",simkl_args,error);
    poll_ready(root+"/cancel");
    auto cancel_transport=[&](const auto& url,const auto* body,const auto& headers) {
        auto response=fake(url,body,headers);
        if(url=="https://api.simkl.com/oauth2/token") cancelled=true;
        return response;
    };
    Store cancelling(root+"/cancel",cancel_transport,&cancelled);
    cancelling.action("device_poll",{{"provider","simkl"}},error);
    Store after_cancel(root+"/cancel",transport);
    assert(!after_cancel.status()["accounts"]["simkl"]["connected"].get<bool>());
    // Owner-bundled app registrations make normal phone pairing available
    // without opting anyone into an account or a different source.
    tracking::set_applications({{"trakt",{{"client_id","client"}}}});
    std::filesystem::create_directories(root+"/bundled");Store bundled(root+"/bundled",transport);
    assert(bundled.status()["accounts"]["trakt"]["configured"].get<bool>());
    assert(!bundled.status()["accounts"]["trakt"]["connected"].get<bool>() && bundled.source(true)=="nuvio");
    error.clear();assert(bundled.action("device_start",{{"provider","trakt"}},error)["url"]=="https://auth.trakt.tv/activate");
    for(int terminal:{404,409,410,418}) {
        error.clear();bundled.action("device_start",{{"provider","trakt"}},error);assert(error.empty());
        fake.device_status=terminal;bundled.action("device_poll",{{"provider","trakt"}},error);assert(!error.empty());
        fake.device_status=0;error.clear();bundled.action("device_poll",{{"provider","trakt"}},error);assert(error.find("expired")!=std::string::npos);
    }
    error.clear();bundled.action("device_start",{{"provider","trakt"}},error);assert(error.empty());
    fake.device_status=429;auto slow=bundled.action("device_poll",{{"provider","trakt"}},error);assert(error.empty() && slow["pending"]==true && slow["slow"]==true);fake.device_status=0;
    // A saved public client ID needs no client secret to enable pairing.
    std::filesystem::create_directories(root+"/partial");
    save_json(root+"/partial/tracking.json",json{{"version",1},{"accounts",{{"trakt",{{"client_id","client"}}}}}});
    Store repaired(root+"/partial",transport);assert(repaired.status()["accounts"]["trakt"]["configured"]==true);
    // Disconnect restores the owner registration immediately; no app restart.
    error.clear();bundled.action("disconnect",{{"provider","trakt"}},error);assert(error.empty());
    assert(bundled.status()["accounts"]["trakt"]["configured"]==true);
    bundled.action("device_start",{{"provider","trakt"}},error);assert(error.empty());
    bundled.action("device_poll",{{"provider","trakt"}},error);assert(error.empty());
    Store signed_in(root+"/bundled",transport);assert(signed_in.status()["accounts"]["trakt"]["connected"]==true);
    // Owner registration changes cannot rewrite an existing token's client ID.
    tracking::set_applications({{"trakt",{{"client_id","new-owner-client"}}}});
    Store preserved(root+"/bundled",transport);
    preserved.action("device_cancel",{{"provider","trakt"}},error);assert(error.empty());
    assert(load_json(root+"/bundled/tracking.json",disk));
    assert(disk["accounts"]["trakt"]["client_id"]=="client" && disk["accounts"]["trakt"]["access_token"]=="private-access");
    preserved.action("disconnect",{{"provider","trakt"}},error);assert(error.empty());
    assert(load_json(root+"/bundled/tracking.json",disk));
    assert(disk["accounts"]["trakt"]["client_id"]=="new-owner-client" && !disk["accounts"]["trakt"].contains("access_token"));
    assert(!disk["accounts"]["trakt"].contains("client_secret"));
    std::filesystem::create_directories(root+"/isolated");Store isolated(root+"/isolated",transport);
    assert(isolated.status()["accounts"]["trakt"]["configured"]==true && isolated.status()["accounts"]["trakt"]["connected"]==false);
    assert(isolated.source(false)=="nuvio" && isolated.source(true)=="nuvio");
    tracking::set_applications(json::object());
    Store unconfigured(root+"/unconfigured",transport);
    assert(unconfigured.status()["accounts"]["trakt"]["configured"]==false);
    error.clear();unconfigured.action("device_start",{{"provider","trakt"}},error);assert(error.find("no registered")!=std::string::npos);
    std::filesystem::remove_all(root);
    std::cout<<"Tracker contracts passed: pairing, validation, refresh, sources, pagination, resume, scrobble, watched marks, offline retry, redaction, permissions and profile isolation\n";
}
