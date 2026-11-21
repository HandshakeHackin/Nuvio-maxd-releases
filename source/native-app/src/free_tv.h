// Optional public Live TV catalogue. Independent client; no account credentials.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "live_tv.h"
#include "live_guide.h"
#include "feature_http.h"
#include "util.h"
#include <algorithm>

namespace free_tv {
inline constexpr const char* source_id="builtin-free-tv";
inline constexpr const char* data_root="https://raw.githubusercontent.com/jyotiprakash101/nuvio-livetv/data/";
inline constexpr const char* disclaimer="These are free IPTV channels provided by third parties. Nuvio Max'd does not own, host or control them in any way. Availability and playback quality vary (YMMV). Please do not report dead channels or buffering to the Nuvio Max'd developer. This optional feature is intended to help users who do not want to pay for an IPTV subscription.";
inline constexpr size_t catalogue_limit=8u<<20, country_limit=256u<<10, channel_limit=10000;
inline bool valid_country(const std::string& value) {
 return value.size()==2 && std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');});
}
inline std::string normal_country(std::string value) {
 if(!valid_country(value))return "us";
 for(auto& c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
 return value=="gb"?"uk":value;
}
inline std::string country(const json& state) {return normal_country(jstr(state,"free_tv_country","us"));}
inline bool enabled(const json& state) {
 return jbool(state,"free_tv_enabled",false) && jobj(state,"free_tv_disclaimer_version").is_number_integer() && jnum(state,"free_tv_disclaimer_version")==1;
}
inline json sources(const json& state) {
 auto rows=jobj(state,"sources");if(!rows.is_array())rows=json::array();
 // A saved/custom source cannot impersonate the reserved built-in source.
 rows.erase(std::remove_if(rows.begin(),rows.end(),[](const auto& r){return jstr(r,"id")==source_id||jstr(r,"type")=="free-tv";}),rows.end());
 if(enabled(state))rows.push_back({{"id",source_id},{"type","free-tv"},{"name","Free live TV"},
  {"country",country(state)},
  {"guide_offset_minutes",jnum(state,"free_tv_guide_offset_minutes")}});
 return rows;
}
inline bool valid_settings(const json& state) {
 if(state.contains("free_tv_enabled")&&!state["free_tv_enabled"].is_boolean())return false;
 if(state.contains("free_tv_country")&&(!state["free_tv_country"].is_string()||!valid_country(state["free_tv_country"].get<std::string>())))return false;
 if(state.contains("free_tv_disclaimer_version")&&(!state["free_tv_disclaimer_version"].is_number_integer()||state["free_tv_disclaimer_version"]!=1))return false;
 if(jbool(state,"free_tv_enabled",false)&&!enabled(state))return false;
 if(state.contains("free_tv_guide_offset_minutes")) {
  const auto& n=state["free_tv_guide_offset_minutes"];
  if(!n.is_number_integer()||jnum(state,"free_tv_guide_offset_minutes") < -720||jnum(state,"free_tv_guide_offset_minutes")>720)return false;
 }
 return true;
}
inline void normalise(json& state) {
 const bool on=valid_settings(state)&&enabled(state);
 state["free_tv_enabled"]=on;state["free_tv_country"]=country(state);
 if(!jobj(state,"free_tv_guide_offset_minutes").is_number_integer() || jnum(state,"free_tv_guide_offset_minutes") < -720 || jnum(state,"free_tv_guide_offset_minutes")>720)state.erase("free_tv_guide_offset_minutes");
 if(!jobj(state,"free_tv_disclaimer_version").is_number_integer()||jnum(state,"free_tv_disclaimer_version")!=1)state.erase("free_tv_disclaimer_version");
}
struct Country {std::string code,name;};
struct Countries {std::vector<Country> rows;std::string error;};
Countries parse_countries(const std::string&);
live_tv::Result parse_channels(const std::string&,const std::string&,const std::atomic<bool>* =nullptr);
Countries countries(const std::atomic<bool>*,const feature_http::Request& ={});
live_tv::Result fetch(const std::string&,const std::atomic<bool>*,const feature_http::Request& ={});
live_guide::Result parse_guide(const std::string&,const std::string&,int64_t,const std::atomic<bool>* =nullptr);
live_guide::Result guide(const std::string&,const std::string&,int64_t,const std::atomic<bool>*,const feature_http::Request& ={});
} // namespace free_tv
