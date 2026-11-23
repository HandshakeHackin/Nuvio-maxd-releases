// Production Store regression cases: MDBList wire contracts and reconciliation.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tracking.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
struct Fixture {
    json receipt={{"added",1},{"existing",0},{"not_found",0}};
    json rows=json::array();
    std::vector<json> writes;
    std::vector<std::string> reads;
    bool commit=true,offline=false,nested_only=false,fail_readback=false;
    long status=200;
    std::function<json(const std::string&)> page;
    std::function<void()> after_write;
    HttpResponse operator()(const std::string& url,const std::string* body,const std::vector<std::string>&) {
        assert(starts_with(url,"https://api.mdblist.com/"));
        if(url.find("/user?")!=std::string::npos)return {200,R"({"user_id":123})","","",0};
        if(offline)return {0,"secret-response","private-key-url","",0};
        if(body) {
            assert(url.find("/watchlist/items/add?")!=std::string::npos || url.find("/watchlist/items/remove?")!=std::string::npos);
            const auto request=json::parse(*body);writes.push_back(request);
            assert(request.size()==1);
            const auto bucket=request.begin().key();assert(bucket=="movies"||bucket=="shows");
            const auto& entry=request.begin().value()[0];
            if(nested_only&&!entry.contains("ids"))return {422,"{}","","",0};
            const auto& external=entry.contains("ids")?entry["ids"]:entry;
            assert(external.size()==1 && (external.contains("imdb")||external.contains("tmdb")));
            if(commit) {
                rows=json::array();
                if(url.find("/add?")!=std::string::npos) {
                    json row{{"ids",external},{"bucket",bucket}};
                    if(external.contains("imdb"))row["imdb_id"]=external["imdb"];
                    rows.push_back(row);
                }
            }
            if(after_write)after_write();
            return {status,receipt.dump(),status==0?"private-transport-error":"","",0};
        }
        assert(url.find("/watchlist/items?")!=std::string::npos);
        reads.push_back(url);
        if(fail_readback&&reads.size()>1)return {503,"secret-response","","",0};
        if(page)return {200,page(url).dump(),"","",0};
        json out{{"movies",json::array()},{"shows",json::array()},{"pagination",{{"has_more",false}}}};
        for(const auto& row:rows)out[jstr(row,"bucket","movies")].push_back(row);
        return {200,out.dump(),"","",0};
    }
};
json title(bool movie=true,const std::string& id="tt12345") {
    return {{"_id",id},{"type",movie?"movie":"series"},{"name","Synthetic"},{"stream","private-stream"}};
}
json page(int start,int count,bool more) {
    json rows=json::array();
    for(int i=start;i<start+count;++i)rows.push_back({{"id",i+1},{"imdb_id","tt"+std::to_string(1000000+i)}});
    return {{"movies",rows},{"shows",json::array()},
        {"pagination",{{"limit",100},{"has_more",more}}}};
}
}
void mdblist_membership_cases() {
    const auto root="/tmp/nuvio-mdblist-"+std::to_string(getpid());
    std::filesystem::create_directories(root);int cases=0;
    auto run=[&](Fixture& fake,const std::function<void(tracking::Store&,const std::string&)>& test) {
        const auto dir=root+"/"+std::to_string(++cases);std::filesystem::create_directory(dir);
        tracking::Store store(dir,[&](const auto& u,const auto* b,const auto& h){return fake(u,b,h);});
        std::string error;store.action("key",{{"provider","mdblist"},{"api_key","synthetic-key"}},error);assert(error.empty());
        store.action("source",{{"field","library"},{"value","mdblist"}},error);assert(error.empty());
        test(store,dir);
    };
    auto queued=[](const std::string& dir){json disk;assert(load_json(dir+"/tracking.json",disk));return disk["membership_pending_mdblist"].size();};
    for(bool movie:{true,false})for(bool scalar:{true,false})for(bool add:{true,false}) {
        Fixture f;const auto bucket=movie?"movies":"shows";
        f.receipt={{add?"added":"removed",scalar?json(1):json{{bucket,1}}},{"not_found",0}};
        if(!add)f.rows=json::array({{{"imdb_id","tt12345"},{"bucket",bucket}}});
        run(f,[&](auto& s,const auto& dir){bool synced=false;std::string e;
            assert(s.membership(title(movie),add,synced,e)&&synced&&e.empty()&&queued(dir)==0);
            assert(f.writes.size()==1 && f.writes[0][bucket][0]["imdb"]=="tt12345");});
    }
    // A scalar existing receipt is accepted even when our preflight was stale.
    {Fixture f;f.receipt={{"added",0},{"existing",1},{"not_found",0}};
     run(f,[&](auto& s,const auto& dir){bool synced=false;std::string e;assert(s.membership(title(),true,synced,e)&&synced&&queued(dir)==0);});}
    // Invalid receipts cannot manufacture success from a pending local overlay.
    for(const auto& receipt:std::vector<json>{json::object(),{{"added",-1},{"existing",2}},{{"added",2}},
        {{"added",true}},{{"added",0.5}},{{"added",1},{"not_found",1}},{{"added",{{"movies",1},{"shows",1}}}},
        {{"added",{{"movies",1}}},{"existing","bad"}}}) {
        Fixture f;f.commit=false;f.receipt=receipt;
        run(f,[&](auto& s,const auto& dir){bool synced=true;std::string e;assert(s.membership(title(),true,synced,e)&&!synced&&!e.empty()&&queued(dir)==1&&f.writes.size()==1);});
    }
    // Lost/malformed responses reconcile through raw remote state without a second POST.
    for(long status:{0L,200L}) {Fixture f;f.status=status;f.receipt=json::object();
        run(f,[&](auto& s,const auto& dir){bool synced=false;std::string e;assert(s.membership(title(),true,synced,e)&&synced&&queued(dir)==0&&f.writes.size()==1&&f.reads.size()==2);});}
    // Explicit shape rejection permits one compatibility retry after a full read.
    {Fixture f;f.nested_only=true;
     run(f,[&](auto& s,const auto& dir){bool synced=false;std::string e;assert(s.membership(title(),true,synced,e)&&synced&&queued(dir)==0);
        assert(f.writes.size()==2&&!f.writes[0]["movies"][0].contains("ids")&&f.writes[1]["movies"][0]["ids"]["imdb"]=="tt12345");
        f.receipt={{"removed",1}};assert(s.membership(title(),false,synced,e)&&synced);
        assert(f.writes.size()==3&&f.writes.back()["movies"][0].contains("ids"));});}
    // Timeout without a remote change never tries the alternate shape immediately.
    {Fixture f;f.commit=false;f.status=0;
     run(f,[&](auto& s,const auto& dir){bool synced=true;std::string e;assert(s.membership(title(),true,synced,e)&&!synced&&queued(dir)==1&&f.writes.size()==1);assert(e.find("private")==std::string::npos);});}
    // Interrupted operations remain private, durable and scoped to their profile.
    {Fixture f;f.offline=true;
     run(f,[&](auto& s,const auto& dir){bool synced=true;std::string e;assert(s.membership(title(),true,synced,e)&&!synced&&queued(dir)==1);
        struct stat st{};assert(stat((dir+"/tracking.json").c_str(),&st)==0&&(st.st_mode&0777)==0600);
        json disk;assert(load_json(dir+"/tracking.json",disk));assert(disk.dump().find("private-stream")==std::string::npos);
        f.offline=false;f.rows=json::array({{{"imdb_id","tt12345"}}});
        tracking::Store restored(dir,[&](const auto& u,const auto* b,const auto& h){return f(u,b,h);});
        json items;assert(restored.library(items,e)&&queued(dir)==0&&f.writes.empty());
        const auto other=dir+"/other";std::filesystem::create_directory(other);tracking::Store isolated(other);
        assert(isolated.source(false)=="nuvio");});}
    {Fixture f;f.offline=true;
     run(f,[&](auto& s,const auto& dir){bool synced=true;std::string e;assert(s.membership(title(),true,synced,e));assert(s.membership(title(),false,synced,e));assert(queued(dir)==1);
        f.offline=false;json items;assert(s.library(items,e)&&queued(dir)==0&&f.writes.empty());});}
    {Fixture f;run(f,[&](auto& s,const auto& dir){bool synced=false;std::string e;assert(s.membership(title(),false,synced,e)&&synced&&queued(dir)==0&&f.writes.empty());});}
    // Retain both aliases: TMDB targets reconcile against a row canonically displayed as IMDb.
    {Fixture f;f.rows=json::array({{{"id",9},{"imdb_id","tt12345"}}});
     run(f,[&](auto& s,const auto& dir){bool synced=false;std::string e;assert(s.membership(title(true,"tmdb:9"),true,synced,e)&&synced&&queued(dir)==0&&f.writes.empty());});}
    {Fixture f;run(f,[&](auto& s,const auto&){bool synced=false;std::string e;assert(s.membership(title(true,"tmdb:9"),true,synced,e)&&synced&&f.writes[0]["movies"][0]["tmdb"]==9);});}
    // A row lacking the target alias must not falsely confirm a removal after an uncertain POST.
    {Fixture f;f.rows=json::array({{{"id",9}}});f.commit=false;f.receipt=json::object();
     run(f,[&](auto& s,const auto& dir){bool synced=true;std::string e;assert(s.membership(title(),false,synced,e)&&!synced&&queued(dir)==1&&f.writes.size()==1);});}
    {Fixture f;run(f,[&](auto& s,const auto&){bool synced=false;std::string e;assert(!s.membership(title(true,"tvdb:9"),true,synced,e)&&f.writes.empty());});}
    // A different media type sharing an external ID cannot satisfy this intent.
    {Fixture f;f.rows=json::array({{{"imdb_id","tt12345"},{"bucket","shows"}}});
     run(f,[&](auto& s,const auto&){bool synced=false;std::string e;assert(s.membership(title(),true,synced,e)&&synced&&f.writes.size()==1);});}
    {Fixture f;f.status=429;f.commit=false;
     run(f,[&](auto& s,const auto& dir){bool synced=true;std::string e;assert(s.membership(title(),true,synced,e)&&!synced&&queued(dir)==1&&f.writes.size()==1&&f.reads.size()==1);});}
    // A failed readback must retain the queue even if the mutation reached the server.
    {Fixture f;f.receipt=json::object();f.fail_readback=true;
     run(f,[&](auto& s,const auto& dir){bool synced=true;std::string e;assert(s.membership(title(),true,synced,e)&&!synced&&queued(dir)==1&&f.writes.size()==1);});}
    // Source changes while an HTTP request is in flight cannot commit stale queue state.
    {Fixture f;run(f,[&](auto& s,const auto& dir){f.after_write=[&]{std::string e;s.action("source",{{"field","library"},{"value","nuvio"}},e);assert(e.empty());};
        bool synced=true;std::string e;assert(s.membership(title(),true,synced,e)&&!synced&&queued(dir)==1);});}
    {Fixture f;run(f,[&](auto& s,const auto& dir){f.after_write=[&]{std::string e;s.action("disconnect",{{"provider","mdblist"}},e);assert(e.empty());};
        bool synced=true;std::string e;assert(s.membership(title(),true,synced,e)&&!synced&&queued(dir)==0&&s.source(false)=="nuvio");});}
    {Fixture f;run(f,[&](auto&,const auto& dir){std::atomic<bool> cancelled{false};
        tracking::Store s(dir,[&](const auto& u,const auto* b,const auto& h){return f(u,b,h);},&cancelled);
        f.after_write=[&]{cancelled=true;};bool synced=true;std::string e;
        assert(s.membership(title(),true,synced,e)&&!synced&&queued(dir)==1&&f.writes.size()==1);});}
    // Real offset pages are accumulated; repeated/malformed/partial pages fail closed.
    {Fixture f;f.page=[](const auto& url){return url.find("offset=100")!=std::string::npos?page(100,1,false):page(0,100,true);};
     run(f,[&](auto& s,const auto&){json items;std::string e;assert(s.library(items,e)&&items.size()==101&&f.reads.size()==2);});}
    for(int mode=0;mode<5;++mode) {Fixture f;f.page=[mode](const auto& url){
        auto out=page(0,100,true);
        if(mode==1)out["movies"]="bad";
        if(mode==2)out["pagination"]["has_more"]="true";
        if(mode==3)out.erase("pagination");
        if(mode==4&&url.find("offset=100")!=std::string::npos)out["movies"]=json::array();
        return out;};
        run(f,[&](auto& s,const auto&){json items;std::string e;assert(!s.library(items,e)&&!e.empty());});}
    {Fixture f;f.page=[](const auto& url){const auto start=url.find("offset=")+7;const int n=std::stoi(url.substr(start));auto p=page(n,1,true);p["pagination"]["limit"]=1;return p;};
     run(f,[&](auto& s,const auto&){json items;std::string e;assert(!s.library(items,e)&&!e.empty()&&f.reads.size()==100);});}
    for(bool missing_bucket:{true,false}) {Fixture f;f.page=[missing_bucket](const auto&){auto p=page(0,1,false);
        if(missing_bucket)p.erase("shows");else p["pagination"]["total_movies"]=2;return p;};
        run(f,[&](auto& s,const auto&){json items;std::string e;assert(!s.library(items,e)&&!e.empty());});}
    std::filesystem::remove_all(root);
    std::cout<<"MDBList membership: "<<cases<<" regression cases passed\n";
}
