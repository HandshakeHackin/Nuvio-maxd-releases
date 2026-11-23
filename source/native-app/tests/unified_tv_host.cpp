#include "unified_tv.h"
#include "live_channel_prefs.hpp"
#include <cassert>
#include <iostream>
#include <thread>
using namespace unified_tv;
int main() {
 json state={{"sources",json::array({{{"id","a"},{"type","m3u"},{"name","Personal A"}},{{"id","b"},{"type","xtream"},{"name","Personal B"}}})}};
 const auto original=state;int calls=0;
 const Fetch fixture=[&](const json& source,const auto*) {
  ++calls;live_tv::Result r;live_tv::Channel c;c.id="42";c.epg_id="shared-id";c.name="Station";c.url="https://private.invalid/"+jstr(source,"id");c.headers={"User-Agent: "+jstr(source,"id")};
  r.channels={c,c};r.guide_url="https://private.invalid/guide/"+jstr(source,"id");return r;
 };
 const auto result=fetch(state,nullptr,fixture);
 assert(calls==2&&result.playlist.error.empty()&&result.playlist.channels.size()==2&&result.guide_urls.size()==2&&state==original);
 const auto a=result.playlist.channels[0],b=result.playlist.channels[1];
 assert(a.id==b.id&&a.source_id=="a"&&b.source_id=="b"&&a.headers!=b.headers);
 assert(guide_key(a,source_id)!=guide_key(b,source_id));
 auto old=a;old.source_id.clear();old.source_name.clear();
 assert(live_channel_prefs::key("a",old)==live_channel_prefs::key(source_id,a));
 assert(live_channel_prefs::key(source_id,a)!=live_channel_prefs::key(source_id,b));
 auto edited=state;const auto key=live_channel_prefs::key(source_id,a);const std::string name="My station";const bool hidden=true;
 assert(live_channel_prefs::set(edited,"a",key,&name,&hidden));
 assert(live_channel_prefs::name(edited,source_id,a)==name&&live_channel_prefs::hidden(edited,source_id,a));
 assert(live_channel_prefs::name(edited,source_id,b)=="Station"&&!live_channel_prefs::hidden(edited,source_id,b));
 auto unrelated=b;unrelated.name="Another station";
 assert(live_channel_prefs::backups({a,unrelated},0).empty()); // Equal provider-local EPG IDs prove nothing across accounts.
 assert(live_channel_prefs::backups({a,b},0)==std::vector<size_t>{1}); // Explicit alternative; its own headers survive.
 live_channel_prefs::remember_group(edited,source_id,"news");assert(live_channel_prefs::valid(edited));
 calls=0;auto failed=fetch(state,nullptr,[&](const auto& s,const auto* c){if(jstr(s,"id")=="b")return live_tv::Result{{},"https://private.invalid/?password=secret"};return fixture(s,c);});
 assert(failed.playlist.channels.size()==1&&failed.playlist.error.empty()&&!failed.playlist.notice.empty()&&failed.playlist.notice.find("private")==std::string::npos);
 state["free_tv_enabled"]=true;calls=0;fetch(state,nullptr,fixture);assert(calls==2); // No disclaimer acceptance, no public catalogue.
 state["free_tv_disclaimer_version"]=1;calls=0;auto with_free=fetch(state,nullptr,fixture);assert(calls==3&&with_free.playlist.channels.front().source_id==free_tv::source_id);
 std::atomic<bool> cancelled{false};calls=0;
 auto aborted=fetch(state,&cancelled,[&](const auto& s,const auto* c){auto r=fixture(s,c);if(calls==2)cancelled=true;return r;});
 assert(aborted.playlist.channels.empty()&&!aborted.playlist.error.empty());
 calls=0;const auto started=std::chrono::steady_clock::now();
 const auto timed=fetch(original,nullptr,[&](const auto& s,const auto* cancel){
  auto r=fixture(s,cancel);if(calls==2)while(!*cancel)std::this_thread::sleep_for(std::chrono::milliseconds(2));return r;
 },std::chrono::milliseconds(40));
 assert(calls==2&&timed.playlist.channels.size()==1&&!timed.playlist.notice.empty());
 assert(std::chrono::steady_clock::now()-started<std::chrono::seconds(2));
 auto many=original;many["sources"]=json::array();for(int i=0;i<20;++i)many["sources"].push_back({{"id",std::to_string(i)},{"type","m3u"}});
 calls=0;auto limited=fetch(many,nullptr,fixture);assert(calls==max_sources&&limited.playlist.channels.size()==max_sources&&!limited.playlist.notice.empty());
 auto memory=fetch(original,nullptr,[](const auto&,const auto*) {
  live_tv::Result r;for(int i=0;i<15000;++i){live_tv::Channel c;c.id=std::to_string(i);c.name="bounded";c.url="https://private.invalid/"+std::string(4096,'x');r.channels.push_back(std::move(c));}return r;
 });
 assert(!memory.playlist.channels.empty()&&memory.playlist.channels.size()<15000&&!memory.playlist.notice.empty());
 std::cout<<"Unified TV core: source identities, header isolation, exact duplicates, cross-source backups, consent, partial failures, cancellation, deadline and shared memory/source limits passed\n";
}
