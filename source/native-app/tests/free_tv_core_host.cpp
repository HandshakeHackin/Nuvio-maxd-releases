#include "free_tv.h"
#include "config_backup.h"
#include "live_channel_prefs.hpp"
#include <cassert>
#include <iostream>

void free_tv_core_cases() {
 using namespace free_tv;
 json state={{"sources",json::array({{{"id","personal"},{"type","m3u"},{"url","https://private.invalid/list?secret=private"}}})}};
 const auto personal=state["sources"];
 assert(!enabled(state)&&sources(state)==personal);
 state["sources"].push_back({{"id","forged"},{"type","free-tv"},{"country","us"}});
 assert(sources(state)==personal);state["sources"]=personal;
 state["free_tv_enabled"]=true;assert(!enabled(state));
 state["free_tv_disclaimer_version"]=1;assert(enabled(state));
 assert(sources(state).size()==2&&state["sources"]==personal);
 assert(sources(state).back()["id"]==source_id);
 json edits=json::object();bool hidden=true;
 for(int i=0;i<16;++i) {
  const auto id="personal"+std::to_string(i);
  live_channel_prefs::remember_group(edits,id,"news");
  assert(live_channel_prefs::set(edits,id,"1",nullptr,&hidden));
 }
 live_channel_prefs::remember_group(edits,source_id,"news");
 assert(live_channel_prefs::set(edits,source_id,"1",nullptr,&hidden)&&live_channel_prefs::valid(edits));
 assert(edits["last_groups"].size()==17&&edits["channel_edits"].size()==17);
 assert(!live_channel_prefs::set(edits,"another-personal","1",nullptr,&hidden));
 assert(country(state)=="us"&&normal_country("GB")=="uk"&&!valid_country("../us"));
 auto cloned=state;cloned["free_tv_country"]="../../private";assert(!valid_settings(cloned));normalise(cloned);assert(!enabled(cloned)&&country(cloned)=="us");
 cloned=state;cloned["free_tv_enabled"]="true";assert(!valid_settings(cloned));
 cloned=state;cloned["free_tv_disclaimer_version"]=2;assert(!valid_settings(cloned));
 cloned=state;cloned["free_tv_guide_offset_minutes"]=721;assert(!valid_settings(cloned));
 const auto countries=parse_countries(R"({"us":{"name":"United States"},"uk":{"name":"United Kingdom"},"../private":{"name":"Invalid"},"zz":{"name":"bad\nname"}})");
 assert(countries.error.empty()&&countries.rows.size()==2&&countries.rows[0].code=="uk");
 assert(!parse_countries("[]").error.empty());
 assert(!parse_countries(std::string(country_limit+1,' ')).error.empty());

 json rows=json::array();
 for(const auto* origin:{"samsung","pluto","roku","freetv","validated","online","iptv"})rows.push_back({
  {"id",std::string("channel-")+origin},{"n",std::string("Fixture ")+origin},{"c",{"news"}},
  {"s",json::array({{{"u","https://public.invalid/broken.m3u8"},{"o",origin},{"k",0}},
   {{"u",std::string("https://public.invalid/")+origin+".m3u8"},{"o",origin},{"k",1},{"a","FixturePlayer"},{"r","https://public.invalid/"}}})}});
 auto result=parse_channels(rows.dump(),"GB");
 assert(result.error.empty()&&result.channels.size()==7);
 assert(result.guide_url.empty()); // The guide is loaded per visible channel.
 for(const auto& c:result.channels) {
  assert(c.url.find("broken")==std::string::npos&&c.headers.size()==2&&c.epg_id==c.id&&c.groups.size()==2);
  assert(c.headers[0]=="User-Agent: FixturePlayer"&&c.headers[1]=="Referer: https://public.invalid/");
 }
 rows.push_back(rows.front()); // Duplicate IDs do not multiply channel rows.
 auto adult=rows.front();adult["id"]="adult";adult["x"]=1;rows.push_back(adult);
 auto bad=rows.front();bad["id"]="injected";bad["s"][1]["a"]="UA\r\nAuthorization: private";rows.push_back(bad);
 bad["id"]="credential-url";bad["s"][1]["a"]="Fixture";bad["s"][1]["u"]="https://user:private@public.invalid/live";rows.push_back(bad);
 bad["id"]="local-file";bad["s"][1]["u"]="file:///private";rows.push_back(bad);
 bad["id"]="bad-provider";bad["s"][1]["u"]="https://public.invalid/live";bad["s"][1]["o"]="unknown";rows.push_back(bad);
 assert(parse_channels(rows.dump(),"us").channels.size()==7);
 assert(!parse_channels("{}","us").error.empty());
 assert(!parse_channels("[]","us").error.empty());
 assert(!parse_channels(std::string(catalogue_limit+1,' '),"us").error.empty());
 std::atomic<bool> cancel{true};assert(parse_channels(rows.dump(),"us",&cancel).channels.empty());
 int calls=0;
 feature_http::Request transport=[&](const auto& q,const auto*) {
  ++calls;assert(q.headers.empty()&&q.method=="GET"&&q.body.empty()&&q.https_only);
  assert(q.url==std::string(data_root)+"c/us.json"&&q.limit==catalogue_limit);
  feature_http::Response r;r.status=200;r.body=rows.dump();return r;
 };
 assert(fetch("us",&cancel,transport).channels.empty()&&calls==0);
 cancel=false;assert(fetch("../us",&cancel,transport).channels.empty()&&calls==0);
 assert(fetch("US",&cancel,transport).channels.size()==7&&calls==1);
 auto failed=fetch("us",nullptr,[](const auto&,const auto*){feature_http::Response r;r.error="private hostname and token";return r;});
 assert(failed.channels.empty()&&failed.error.find("private")==std::string::npos);
 auto epg=guide("GB","channel/with spaces",10000,nullptr,[](const auto& q,const auto*) {
  assert(q.url==std::string(data_root)+"g/uk/channel%2Fwith%20spaces.json"&&q.limit==(1u<<20)&&q.headers.empty());
  feature_http::Response r;r.status=200;r.body=R"([{"s":10001,"e":10200,"t":"Next"},{"s":9900,"e":10001,"t":"Now"},{"s":9900,"e":10001,"t":"Now"},{"s":10000,"e":9999,"t":"Invalid"}])";return r;
 });
 assert(epg.error.empty()&&epg.channels["channel/with spaces"].size()==2&&epg.channels["channel/with spaces"][0].title=="Now");
 assert(!parse_guide(std::string((1u<<20)+1,' '),"fixture",10000).error.empty());

 json snapshot={{"version",1},{"scope",std::string(64,'a')},{"files",{
  {"profile-settings.json","{}"},{"tvdb.json","{}"},{"live-tv.json",json{{"sources",json::array()},
   {"free_tv_enabled",true},{"free_tv_disclaimer_version",1},{"free_tv_country","us"}}.dump()}}}};
 std::string error;assert(config_backup::validate(snapshot,std::string(64,'a'),error));
 const auto encrypted=config_backup::encrypt(snapshot,"free-tv-fixture-passphrase");assert(encrypted.error.empty());
 const auto decrypted=config_backup::decrypt(encrypted.bytes,"free-tv-fixture-passphrase",std::string(64,'a'));
 assert(decrypted.error.empty()&&decrypted.snapshot==snapshot);
 auto saved=json::parse(snapshot["files"]["live-tv.json"].get<std::string>());
 saved.erase("free_tv_disclaimer_version");snapshot["files"]["live-tv.json"]=saved.dump();
 assert(!config_backup::validate(snapshot,std::string(64,'a'),error));
 saved["free_tv_disclaimer_version"]=1;saved["sources"].push_back({{"id",source_id},{"type","m3u"},{"url","https://public.invalid/"}});
 snapshot["files"]["live-tv.json"]=saved.dump();assert(!config_backup::validate(snapshot,std::string(64,'a'),error));
 std::cout<<"Free TV catalogue: seven providers, country paths, safe headers/URLs, adult/duplicate/broken filtering, limits, cancellation, private failures and backup consent validation passed\n";
}
