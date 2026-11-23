// SPDX-License-Identifier: GPL-3.0-or-later
#include "unified_tv.h"
#include <condition_variable>
#include <mutex>
#include <thread>
namespace unified_tv {
namespace {
class Budget {
 const std::atomic<bool>* external_;
 std::chrono::steady_clock::time_point deadline_;
 std::mutex mutex_;std::condition_variable wake_;bool done_=false;
 std::thread timer_;
public:
 std::atomic<bool> cancel{false};
 Budget(const std::atomic<bool>* external,std::chrono::milliseconds timeout):external_(external),deadline_(std::chrono::steady_clock::now()+timeout) {
  timer_=std::thread([this]{std::unique_lock lock(mutex_);while(!done_){
   if((external_&&*external_)||expired()){cancel=true;return;}
   wake_.wait_for(lock,std::chrono::milliseconds(20),[this]{return done_;});
  }});
 }
 ~Budget(){{std::lock_guard lock(mutex_);done_=true;}wake_.notify_one();timer_.join();}
 bool expired() const {return std::chrono::steady_clock::now()>=deadline_;}
 bool stopped(){if((external_&&*external_)||expired())cancel=true;return cancel;}
};
size_t bytes(const live_tv::Channel& c) {
 // Account conservatively for string/vector capacity, the growing output
 // vector, and the duplicate index; this is separate from parser buffers.
 size_t n=2*sizeof(c)+64;
 for(const auto* value:{&c.id,&c.name,&c.group,&c.logo,&c.url,&c.epg_id,&c.guide_country,&c.source_id,&c.source_name})n+=value->capacity()+1;
 n+=(c.headers.capacity()+c.groups.capacity())*sizeof(std::string);
 for(const auto& s:c.headers)n+=s.capacity()+1;
 for(const auto& s:c.groups)n+=s.capacity()+1;
 return n;
}
bool same(const live_tv::Channel& a,const live_tv::Channel& b) {
 return a.source_id==b.source_id&&a.id==b.id&&a.name==b.name&&a.url==b.url&&a.headers==b.headers;
}
}
Result fetch(const json& state,const std::atomic<bool>* cancel,const Fetch& fetcher,std::chrono::milliseconds timeout) {
 Result out;auto& result=out.playlist;
 auto rows=free_tv::sources(state);
 // Make the explicitly enabled public catalogue available even when a private provider stalls.
 const auto public_source=std::find_if(rows.begin(),rows.end(),[](const auto& row){return jstr(row,"id")==free_tv::source_id;});
 if(public_source!=rows.end())std::rotate(rows.begin(),public_source,public_source+1);
 if(cancel&&*cancel){result.error="Live TV loading cancelled.";return out;}
 Budget budget(cancel,timeout);size_t count=0,used=0;bool failed=false,limited=false,partial_failure=false;
 std::map<std::uint64_t,size_t> seen;std::set<std::string> origins;
 for(const auto& row:rows) {
  if(cancel&&*cancel){out={};out.playlist.error="Live TV loading cancelled.";return out;}
  if(budget.stopped()||count>=max_sources||result.channels.size()>=live_tv::max_channels){limited=true;break;}
  const auto id=jstr(row,"id"),type=jstr(row,"type");
  if(id.empty()||id.size()>128||id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos||id==source_id||(type!="m3u"&&type!="xtream"&&type!="usb"&&type!="stalker"&&type!="free-tv"))continue;
  if(!origins.insert(id).second)continue;
  ++count;Budget source_budget(&budget.cancel,std::chrono::seconds(30));
  auto loaded=fetcher(row,&source_budget.cancel);
  if(cancel&&*cancel){out={};out.playlist.error="Live TV loading cancelled.";return out;}
  if(budget.stopped()){limited=true;break;}
  if(source_budget.stopped()||!loaded.error.empty()){failed=partial_failure=true;continue;}
  partial_failure=partial_failure||loaded.partial_failure;
  if(!loaded.notice.empty())failed=true;
  out.guide_urls[id]=loaded.guide_url;
  auto name=jstr(row,"name").substr(0,256);
  name.erase(std::remove_if(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;}),name.end());
  for(auto& c:loaded.channels) {
   if(budget.stopped()){limited=true;break;}
   c.source_id=id;c.source_name=type=="free-tv"?"Free live TV":name;
   const auto hash=fnv1a(id+"\n"+c.id+"\n"+c.name+"\n"+c.url);
   auto previous=seen.find(hash);
   if(previous!=seen.end()&&same(result.channels[previous->second],c))continue;
   const auto size=bytes(c);
   if(size>retained_bytes-used||result.channels.size()>=live_tv::max_channels){limited=true;break;}
   used+=size;seen[hash]=result.channels.size();result.channels.push_back(std::move(c));
  }
  if(limited)break;
 }
 if(cancel&&*cancel){out={};out.playlist.error="Live TV loading cancelled.";return out;}
 result.partial_failure=partial_failure||budget.expired();
 if(result.channels.empty())result.error="No channels loaded. Check your Live TV sources and try again.";
 else if(limited)result.notice="Unified TV loading limit reached. Some sources may be missing; reload or choose a source.";
 else if(failed)result.notice="Some TV sources could not fully load. Available channels are shown; reload to retry.";
 return out;
}
}
