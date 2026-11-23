// Profile-local aggregation. Channel origins stay separate from provider IDs.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "free_tv.h"
#include <chrono>
#include <map>
namespace unified_tv {
inline constexpr const char* source_id="builtin-all-tv";
inline constexpr size_t max_sources=17, retained_bytes=32u<<20;
inline constexpr auto load_timeout=std::chrono::seconds(120);
inline std::string owner(const live_tv::Channel& c,const std::string& fallback) {return c.source_id.empty()?fallback:c.source_id;}
inline std::string guide_key(const live_tv::Channel& c,const std::string& view) {return view==source_id?owner(c,view)+":"+c.id:c.id;}
inline json source(const json& state,const live_tv::Channel& c,const std::string& fallback) {
 const auto id=owner(c,fallback);
 for(const auto& row:free_tv::sources(state))if(jstr(row,"id")==id)return row;
 return {};
}
struct Result {
 live_tv::Result playlist;
 std::map<std::string,std::string> guide_urls;
};
using Fetch=std::function<live_tv::Result(const json&,const std::atomic<bool>*)>;
Result fetch(const json&,const std::atomic<bool>*,const Fetch&,std::chrono::milliseconds timeout=load_timeout);
}
