// Public TMDB credits. No playback URLs or account/profile data enter this model.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "stremio.h"
#include "http.h"
#include <functional>
namespace people {
constexpr size_t max_members=80, max_titles=256, max_body=2u<<20;
struct Member {std::string id,name,role,photo;bool crew=false;std::string character{};};
struct Credit {std::string id,type,name,poster,overview,year,role;};
struct Cast {std::vector<Member> members;std::string error;};
struct Person {std::string id,name,biography,birthday,deathday,birthplace,known_for,photo;std::vector<Credit> movies,series;bool crew=false;std::string error;};
struct Resolved {std::string id,type,error;};
using Request=std::function<HttpResponse(const std::string&,const std::atomic<bool>*)>;
bool valid_id(const std::string&);
std::string image(const std::string&,bool portrait=true);
Cast fetch_cast(const Meta&,const std::string& key,const std::string& language="en",const std::atomic<bool>* cancel=nullptr,const Request& request={});
Person fetch_person(const Member&,const std::string& key,const std::string& language="en",const std::atomic<bool>* cancel=nullptr,const Request& request={});
Resolved resolve(const Credit&,const std::string& key,const std::atomic<bool>* cancel=nullptr,const Request& request={});
}
