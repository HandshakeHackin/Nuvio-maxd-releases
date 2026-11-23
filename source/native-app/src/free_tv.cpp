// SPDX-License-Identifier: GPL-3.0-or-later
#include "free_tv.h"
#include <set>
#include <cmath>
#include <chrono>
#include <map>

namespace free_tv {
namespace {
constexpr const char* unavailable="Free TV is unavailable right now. Try again later.";
bool text(const std::string& value,size_t limit) {
 return !value.empty() && value.size()<=limit && std::none_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;});
}
bool stream_url(const std::string& value) {
 return value.size()<=8192 && feature_http::safe_url(value,false);
}
const char* provider(const std::string& value) {
 if(value=="samsung")return "Samsung TV Plus";
 if(value=="pluto")return "Pluto TV";
 if(value=="roku")return "Roku Channel";
 if(value=="freetv")return "Free-TV";
 if(value=="validated")return "Validated list";
 if(value=="online")return "Health-checked list";
 return value=="iptv"?"iptv-org":nullptr;
}
bool flagged(const json& value) {return value.is_boolean()?value.get<bool>():value.is_number()?value!=0:!value.is_null();}
feature_http::Response request(const std::string& path,size_t limit,const std::atomic<bool>* cancel,const feature_http::Request& transport,long timeout=60000) {
 if(cancel&&*cancel){feature_http::Response r;r.error=unavailable;return r;}
 feature_http::Query q;q.url=std::string(data_root)+path;q.limit=limit;q.timeout_ms=timeout;
 auto r=transport?transport(q,cancel):feature_http::perform(q,cancel);
 if(r.body.size()>limit){r.body.clear();r.error=unavailable;}
 return r;
}
} // namespace
Countries parse_countries(const std::string& bytes) {
 Countries out;
 if(bytes.size()>country_limit){out.error=unavailable;return out;}
 auto data=json::parse(bytes,nullptr,false);
 if(!data.is_object()||data.size()>256){out.error=unavailable;return out;}
 for(const auto& item:data.items()) {
  const auto name=jstr(item.value(),"name");
  if(!jobj(item.value(),"name").is_string()||!valid_country(item.key())||item.key()!=normal_country(item.key())||!text(name,128))continue;
  out.rows.push_back({item.key(),name});
 }
 std::sort(out.rows.begin(),out.rows.end(),[](const auto& a,const auto& b){return a.name<b.name;});
 if(out.rows.empty())out.error=unavailable;
 return out;
}
live_tv::Result parse_channels(const std::string& bytes,const std::string& region,const std::atomic<bool>* cancel) {
 live_tv::Result out;
 if(!valid_country(region)||bytes.size()>catalogue_limit){out.error=unavailable;return out;}
 auto data=json::parse(bytes,nullptr,false);
 if(!data.is_array()||data.size()>channel_limit){out.error=unavailable;return out;}
 std::set<std::string> ids;
 for(const auto& row:data) {
  if(cancel&&*cancel)return {{},unavailable};
  if(!row.is_object()||flagged(jobj(row,"x")))continue;
  const auto id=jstr(row,"id"),name=jstr(row,"n");const auto& streams=jobj(row,"s");
  if(!jobj(row,"id").is_string()||!jobj(row,"n").is_string()||!text(id,200)||!text(name,256)||!streams.is_array()||streams.size()>64||ids.count(id))continue;
  live_tv::Channel channel;channel.id=id;channel.epg_id=id;channel.name=name;channel.guide_country=normal_country(region);
  const auto& categories=jobj(row,"c");
  if(categories.is_array())for(const auto& category:categories) {
   if(!category.is_string()||channel.groups.size()>=16)continue;
   auto label=category.get<std::string>();
   if(label=="xxx"||label=="adult"){channel.name.clear();break;}
   if(text(label,64))channel.groups.push_back(label);
  }
  if(channel.name.empty())continue;
  channel.group=channel.groups.empty()?"Free live TV":channel.groups.front();
  for(const auto& stream:streams) {
   const auto url=jstr(stream,"u");const char* origin=provider(jstr(stream,"o"));
   if(!origin||!jobj(stream,"u").is_string()||!stream_url(url)||(stream.contains("k")&&(!stream["k"].is_number()&&!stream["k"].is_boolean()))||(stream.contains("k")&&!flagged(stream["k"])))continue;
   if((stream.contains("a")&&!stream["a"].is_string())||(stream.contains("r")&&!stream["r"].is_string()))continue;
   const auto agent=jstr(stream,"a"),referrer=jstr(stream,"r");
   if((!agent.empty()&&!text(agent,512))||(!referrer.empty()&&(!text(referrer,1024)||!stream_url(referrer))))continue;
   channel.url=url;
   if(!agent.empty())channel.headers.push_back("User-Agent: "+agent);
   if(!referrer.empty())channel.headers.push_back("Referer: "+referrer);
   if(std::find(channel.groups.begin(),channel.groups.end(),origin)==channel.groups.end())channel.groups.push_back(origin);
   break;
  }
  if(channel.url.empty())continue;
  auto country_group=channel.guide_country;for(auto& c:country_group)c=char(c-'a'+'A');
  channel.groups.push_back(country_group);
  const auto logo=jstr(row,"l");if(stream_url(logo))channel.logo=logo;
  ids.insert(id);out.channels.push_back(std::move(channel));
 }
 // Guide data is fetched per visible channel, avoiding a country-sized XMLTV
 // download or decompression buffer on the console.
 if(out.channels.empty())out.error="No free channels are available for this country.";
 return out;
}
Countries countries(const std::atomic<bool>* cancel,const feature_http::Request& transport) {
 const auto r=request("countries.json",country_limit,cancel,transport);
 if(!r.ok()||(cancel&&*cancel))return {{},unavailable};
 return parse_countries(r.body);
}
live_tv::Result fetch(const std::string& region,const std::atomic<bool>* cancel,const feature_http::Request& transport) {
 return fetch(std::vector<std::string>{region},cancel,transport);
}
live_tv::Result fetch(const std::vector<std::string>& regions,const std::atomic<bool>* cancel,const feature_http::Request& transport) {
 if(!valid_selection(json(regions)))return {{},unavailable};
 live_tv::Result out;std::map<std::string,size_t> ids;size_t bytes=0;bool failed=false,limited=false;
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(load_timeout_ms);
 for(const auto& region:regions) {
  if(cancel&&*cancel)return {{},unavailable};
  const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
  if(bytes>=combined_bytes||out.channels.size()>=live_tv::max_channels||remaining<=0){limited=true;break;}
  const auto limit=std::min(catalogue_limit,combined_bytes-bytes);
  const auto r=request("c/"+normal_country(region)+".json",limit,cancel,transport,std::min<long>(remaining,25000));
  if(cancel&&*cancel)return {{},unavailable};
  // Reserve the entire allowance on failure, including a truncated response.
  // Thus failed/oversized downloads cannot evade the aggregate byte budget.
  bytes+=r.ok()?r.body.size():limit;
  if(!r.ok()){failed=true;continue;}
  auto parsed=parse_channels(r.body,region,cancel);
  if(cancel&&*cancel)return {{},unavailable};
  if(!parsed.error.empty()){failed=true;continue;}
  for(auto& channel:parsed.channels) {
   const auto old=ids.find(channel.id);
   if(old!=ids.end()) {
    auto& groups=out.channels[old->second].groups;const auto& country_group=channel.groups.back();
    if(std::find(groups.begin(),groups.end(),country_group)==groups.end())groups.push_back(country_group);
    continue;
   }
   if(out.channels.size()>=live_tv::max_channels){limited=true;break;}
   ids.emplace(channel.id,out.channels.size());out.channels.push_back(std::move(channel));
  }
 }
 if(cancel&&*cancel)return {{},unavailable};
 out.partial_failure=failed;
 if(out.channels.empty())out.error=unavailable;
 else if(limited)out.notice="Free TV loading limit reached. Choose fewer countries or reload channels.";
 else if(failed)out.notice="Some countries could not be loaded. Reload channels to retry.";
 return out;
}
live_guide::Result parse_guide(const std::string& bytes,const std::string& channel,int64_t now,const std::atomic<bool>* cancel) {
 live_guide::Result out;
 if(bytes.size()>(1u<<20)||!text(channel,200)){out.error=unavailable;return out;}
 auto rows=json::parse(bytes,nullptr,false);
 if(!rows.is_array()||rows.size()>10000){out.error=unavailable;return out;}
 auto& list=out.channels[channel];
 for(const auto& row:rows) {
  if(cancel&&*cancel)return {{},unavailable};
  const auto start=jnum(row,"s",-1),end=jnum(row,"e",-1);const auto title=jstr(row,"t");
  if(!jobj(row,"s").is_number()||!jobj(row,"e").is_number()||!jobj(row,"t").is_string()||!std::isfinite(start)||!std::isfinite(end)||start<0||end<=start||end>4102444800.0||end-start>86400||
     end<now-7200||start>now+36*3600||!text(title,256))continue;
  list.push_back({int64_t(start),int64_t(end),title});
 }
 std::sort(list.begin(),list.end(),[](const auto& a,const auto& b){return a.start<b.start;});
 list.erase(std::unique(list.begin(),list.end(),[](const auto& a,const auto& b){return a.start==b.start&&a.end==b.end&&a.title==b.title;}),list.end());
 if(list.size()>256){list.resize(256);out.limited=true;}
 return out;
}
live_guide::Result guide(const std::string& region,const std::string& channel,int64_t now,const std::atomic<bool>* cancel,const feature_http::Request& transport) {
 if(!valid_country(region)||!text(channel,200))return {{},unavailable};
 const auto r=request("g/"+normal_country(region)+"/"+feature_http::encode(channel)+".json",1u<<20,cancel,transport);
 if(!r.ok()||(cancel&&*cancel))return {{},unavailable};
 return parse_guide(r.body,channel,now,cancel);
}
} // namespace free_tv
