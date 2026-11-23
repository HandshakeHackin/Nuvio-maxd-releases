// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "search_keyboard.hpp"
#include "feature_http.h"
#include "util.h"
#include <set>
namespace sx::search_suggestions {
constexpr std::size_t max_catalogs=8,max_response=512u<<10;
inline std::vector<search_keyboard::Suggestion> parse(const std::string& body) {
 std::vector<search_keyboard::Suggestion> out;
 if(body.size()>max_response)return out;
 bool excessive=false;std::size_t nodes=0;
 auto value=json::parse(body,[&](int depth,json::parse_event_t,json&){if(depth>20||++nodes>10000){excessive=true;return false;}return true;},false);
 if(excessive||value.is_discarded())return out;
 const auto& metas=jobj(value,"metas");if(!metas.is_array())return out;
 std::set<std::string> seen;std::size_t scanned=0;
 for(const auto& m:metas){
  if(++scanned>50||out.size()==search_keyboard::max_suggestions)break;
  search_keyboard::Suggestion s{jstr(m,"id"),jstr(m,"type"),jstr(m,"name"),jstr(m,"releaseInfo")};
  if(s.id.empty()||s.id.size()>256||s.type.empty()||s.type.size()>32||s.name.empty()||s.name.size()>256||s.year.size()>64||search_keyboard::bounded(s.name)!=s.name)continue;
  if(!seen.insert(s.type+"\n"+s.id).second)continue;
  out.push_back(std::move(s));
 }
 return out;
}
struct Result {std::vector<search_keyboard::Suggestion> items;bool failed=false;};
inline Result fetch(const std::string& url,const std::atomic<bool>* cancel,const feature_http::Request& request=feature_http::perform) {
 if(cancel&&*cancel)return {};
 feature_http::Query q;q.url=url;q.limit=max_response;q.timeout_ms=4000;q.https_only=false;
 const auto response=request(q,cancel);
 if(cancel&&*cancel)return {};
 if(!response.ok())return {{},true};
 return {parse(response.body),false};
}
inline std::vector<search_keyboard::Suggestion> merge(const std::vector<std::vector<search_keyboard::Suggestion>>& rows) {
 std::vector<search_keyboard::Suggestion> out;std::set<std::string> seen;
 // Round-robin gives each configured source a place in the short list.
 for(std::size_t n=0;n<search_keyboard::max_suggestions;++n)for(const auto& row:rows){
  if(n<row.size()&&seen.insert(row[n].type+"\n"+row[n].id).second)out.push_back(row[n]);
  if(out.size()==search_keyboard::max_suggestions)return out;
 }
 return out;
}
}
