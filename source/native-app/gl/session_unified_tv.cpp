// A combined channel view; credentials remain in the selected profile.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "stalker.h"
#include "../ui/i18n.hpp"
#include <algorithm>
namespace sx {
void Session::live_guide_unified_update(int focus) {
 const auto& indices=page_==Page::sports?sports_guide_indices_:live_indices_;
 if(page_==Page::sports){if(!sports_visible||indices.empty())return;focus=sports_guide_cursor_;}
 const auto time=now_seconds();if(time<live_guide_tick_)return;live_guide_tick_=time+1;
 if(!live_guide_cancel_)live_guide_cancel_=std::make_shared<std::atomic<bool>>(false);
 const auto cancel=live_guide_cancel_;const auto generation=live_guide_generation_,profile=profile_generation_;
 for(int at=std::max(0,focus-4);at<std::min<int>(indices.size(),focus+6)&&live_guide_inflight_<2;++at) {
  if(indices[at]>=live_channels_.size())continue;
  const auto c=live_channels_[indices[at]];const auto key=unified_tv::guide_key(c,live_source_);
  auto cached=live_guide_channels_.find(key);
  if(live_guide_pending_.count(key)||(cached!=live_guide_channels_.end()&&cached->second.expires>time))continue;
  const auto source=unified_tv::source(live_state_,c,live_source_);const auto id=unified_tv::owner(c,live_source_);
  if(!source.is_object()||live_guide_sources_pending_.count(id)||live_guide_source_retry_[id]>time)continue;
  const auto offset=int64_t(live_guide::offset_minutes(source))*60;
  const auto automatic=live_source_guides_.find(id);
  const auto url=jstr(source,"epg_url").empty()?(automatic==live_source_guides_.end()?"":automatic->second):jstr(source,"epg_url");
  const auto type=jstr(source,"type");
  if(url.empty()&&type!="xtream"&&type!="stalker"&&type!="free-tv")continue;
  std::vector<live_tv::Channel> wanted{c};
  if(!url.empty()) {
   wanted.clear();
   for(int n=std::max(0,focus-4);n<std::min<int>(indices.size(),focus+6);++n) {
    if(indices[n]>=live_channels_.size())continue;
    const auto& row=live_channels_[indices[n]];
    if(row.source_id==id)wanted.push_back(row);
   }
   live_guide_sources_pending_.insert(id);
  }
  std::shared_ptr<const std::string> document;
  const auto doc=live_guide_documents_.find(id);
  if(doc!=live_guide_documents_.end()&&doc->second.expires>time){document=doc->second.body;doc->second.touched=time;}
  for(const auto& row:wanted)live_guide_pending_.insert(unified_tv::guide_key(row,unified_tv::source_id));
  ++live_guide_inflight_;
  bg<live_guide::Result>([source,c,url,wanted,document,cancel,offset] {
   const auto now=now_epoch_ms()/1000-offset;
   if(!url.empty()) {
    std::set<std::string> ids;for(const auto& row:wanted)ids.insert(row.epg_id.empty()?row.id:row.epg_id);
    return document?live_guide::parse_xmltv(*document,ids,now,cancel.get()):live_guide::fetch_xmltv(url,ids,now,cancel.get());
   }
   const auto type=jstr(source,"type");
   if(type=="free-tv")return free_tv::guide(c.guide_country.empty()?jstr(source,"country"):c.guide_country,c.id,now,cancel.get());
   if(type=="stalker")return stalker::guide(stalker::source(source),c.id,now,cancel.get());
   return live_guide::fetch_xtream(jstr(source,"url"),jstr(source,"username"),jstr(source,"password"),c.id,now,cancel.get());
  },[this,generation,profile,cancel,id,wanted,url](auto& result) {
   if(generation!=live_guide_generation_||profile!=profile_generation_||*cancel)return;
   --live_guide_inflight_;live_guide_sources_pending_.erase(id);
   const auto time=now_seconds();
   if(!result.error.empty())live_guide_source_retry_[id]=time+60;
   if(result.error.empty()&&result.document) {
    // One global raw-document budget across all providers, with LRU eviction.
    live_guide_documents_.erase(id);size_t used=result.document->size();
    for(const auto& row:live_guide_documents_)if(row.second.body)used+=row.second.body->size();
    while(!live_guide_documents_.empty()&&used>live_guide::max_bytes) {
     auto old=std::min_element(live_guide_documents_.begin(),live_guide_documents_.end(),[](const auto& a,const auto& b){return a.second.touched<b.second.touched;});
     if(old->second.body)used-=old->second.body->size();
     live_guide_documents_.erase(old);
    }
    if(used<=live_guide::max_bytes)live_guide_documents_[id]={result.document,time+900,time};
   }
   for(const auto& row:wanted) {
    const auto key=unified_tv::guide_key(row,unified_tv::source_id);live_guide_pending_.erase(key);
    if(!live_guide_channels_.count(key)&&live_guide_channels_.size()>=256) {
     auto old=std::min_element(live_guide_channels_.begin(),live_guide_channels_.end(),[](const auto& a,const auto& b){return a.second.touched<b.second.touched;});live_guide_channels_.erase(old);
    }
    auto& entry=live_guide_channels_[key];entry.touched=time;entry.expires=time+(result.error.empty()?300:60);
    const auto programme_id=url.empty()?row.id:(row.epg_id.empty()?row.id:row.epg_id);
    if(result.error.empty()){const auto list=result.channels.find(programme_id);entry.programmes=list==result.channels.end()?live_guide::Listings{}:list->second;}
   }
   live_build();
  });
 }
 if(time>=live_guide_redraw_){live_guide_redraw_=time+30;live_build();}
}
}
