// Profile-local presentation preferences. Provider names/IDs/URLs stay intact.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "live_tv.h"
#include "free_tv.h"
#include "util.h"
#include <algorithm>
#include <array>
#include <set>
#include <string_view>
namespace live_channel_prefs {
using Json=nlohmann::json;
inline constexpr std::size_t max_edits=1000,max_name=160,max_backups=32;
inline std::string key(const std::string& source,const live_tv::Channel& c) {
 return std::to_string(fnv1a(source+"\n"+c.id+"\n"+c.name));
}
inline bool source_id(const std::string& value) {
 return !value.empty()&&value.size()<=128&&value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos;
}
inline bool channel_id(const std::string& value) {
 return !value.empty()&&value.size()<=20&&value.find_first_not_of("0123456789")==std::string::npos;
}
inline bool text(const std::string& value,std::size_t limit) {
 if(value.size()>limit||std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;}))return false;
 // Match the strict JSON writer, including overlong encodings/surrogates.
 try {(void)Json(value).dump();return true;}catch(...){return false;}
}
inline const Json& object_at(const Json& j,const std::string& field) {
 static const Json empty=Json::object();
 if(!j.is_object())return empty;
 auto it=j.find(field);return it!=j.end()&&it->is_object()?*it:empty;
}
inline const Json& edit(const Json& state,const std::string& source,const std::string& id) {
 return object_at(object_at(object_at(state,"channel_edits"),source),id);
}
inline bool valid(const Json& state) {
 if(!state.is_object())return false;
 if(state.contains("last_groups")) {
  const auto& groups=state["last_groups"];
  if(!groups.is_object()||groups.size()>16+size_t(groups.contains(free_tv::source_id)))return false;
  for(const auto& item:groups.items())if(!source_id(item.key())||!item.value().is_string()||!text(item.value().get_ref<const std::string&>(),512))return false;
 }
 if(state.contains("channel_edits")) {
  const auto& sources=state["channel_edits"];
  if(!sources.is_object()||sources.size()>16+size_t(sources.contains(free_tv::source_id)))return false;
  std::size_t total=0;
  for(const auto& source:sources.items()) {
   if(!source_id(source.key())||!source.value().is_object()||source.value().size()>max_edits-total)return false;
   total+=source.value().size();
   for(const auto& row:source.value().items()) {
    if(!channel_id(row.key())||!row.value().is_object()||row.value().size()>2)return false;
    for(const auto& field:row.value().items()) {
     if(field.key()=="name") {if(!field.value().is_string()||!text(field.value().get_ref<const std::string&>(),max_name))return false;}
     else if(field.key()=="hidden") {if(!field.value().is_boolean())return false;}
     else return false;
    }
   }
  }
 }
 return true;
}
inline void normalise(Json& state) {
 if(!valid(state)){state.erase("last_groups");state.erase("channel_edits");}
}
inline std::string name(const Json& state,const std::string& source,const live_tv::Channel& c) {
 const auto id=key(source,c);const auto& row=edit(state,source,id);auto it=row.find("name");
 return it!=row.end()&&it->is_string()&&!it->get_ref<const std::string&>().empty()?it->get<std::string>():c.name;
}
inline bool hidden(const Json& state,const std::string& source,const live_tv::Channel& c) {
 const auto id=key(source,c);const auto& row=edit(state,source,id);auto it=row.find("hidden");return it!=row.end()&&it->is_boolean()&&it->get<bool>();
}
inline bool set(Json& state,const std::string& source,const std::string& id,const std::string* name,const bool* hidden) {
 if(!source_id(source)||!channel_id(id)||!valid(state)||(name&&!text(*name,max_name)))return false;
 Json row=edit(state,source,id);
 if(name){if(name->empty())row.erase("name");else row["name"]=*name;}
 if(hidden){if(!*hidden)row.erase("hidden");else row["hidden"]=true;}
 const auto& all=object_at(state,"channel_edits");
 std::size_t count=0;for(const auto& s:all.items())count+=s.value().size();
 if(!row.empty()&&!object_at(all,source).contains(id)&&(count>=max_edits||(!all.contains(source)&&source!=free_tv::source_id&&all.size()-size_t(all.contains(free_tv::source_id))>=16)))return false;
 if(row.empty()) {
  if(state.contains("channel_edits")&&state["channel_edits"].contains(source)) {
   state["channel_edits"][source].erase(id);
   if(state["channel_edits"][source].empty())state["channel_edits"].erase(source);
  }
 } else state["channel_edits"][source][id]=std::move(row);
 return true;
}
inline std::string last_group(const Json& state,const std::string& source) {
 const auto& groups=object_at(state,"last_groups");auto it=groups.find(source);
 return it!=groups.end()&&it->is_string()?it->get<std::string>():"";
}
inline void remember_group(Json& state,const std::string& source,const std::string& group) {
 const auto& groups=object_at(state,"last_groups");
 if(source_id(source)&&text(group,512)&&(groups.contains(source)||source==free_tv::source_id||groups.size()-size_t(groups.contains(free_tv::source_id))<16))state["last_groups"][source]=group;
}
// Conservative presentation matching; no URL guesses and no automatic tuning.
// Preserve channel numbers, country/language tags and +1 timeshift variants.
inline std::string match_name(const std::string& value) {
 if(value.size()>256||!is_valid_utf8(value))return {};
 static constexpr std::array<std::string_view,18> tags={"sd","hd","fhd","uhd","4k","8k","720p","1080p","2160p","hevc","h264","h265","avc","50fps","60fps","5.1","2.0","backup"};
 std::string token,out;
 const auto flush=[&] {if(!token.empty()&&std::find(tags.begin(),tags.end(),token)==tags.end()){if(!out.empty())out+=' ';out+=token;}token.clear();};
 for(unsigned char c:value) {
  if(c>='A'&&c<='Z')c+=32;
  if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c>=128||c=='+'||c=='.')token+=char(c);
  else flush();
 }
 flush();return out;
}
inline std::vector<std::size_t> backups(const std::vector<live_tv::Channel>& channels,std::size_t selected) {
 std::vector<std::size_t> result;
 const auto count=std::min(channels.size(),live_tv::max_channels);
 if(selected>=count)return result;
 const auto& current=channels[selected];const auto normal=match_name(current.name);
 std::set<std::pair<std::string,std::vector<std::string>>> seen{{current.url,current.headers}};
 for(std::size_t n=0;n<count&&result.size()<max_backups;++n) {
  const auto& candidate=channels[n];
  if(n==selected||candidate.url.empty()||!((!current.epg_id.empty()&&current.epg_id==candidate.epg_id)||(!normal.empty()&&normal==match_name(candidate.name))))continue;
  if(seen.emplace(candidate.url,candidate.headers).second)result.push_back(n);
 }
 return result;
}
}
