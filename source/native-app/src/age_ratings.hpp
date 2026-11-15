// Display-only TMDB certifications. Never infer ages or filter playback.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "age_rating_types.hpp"
#include "json.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace age_ratings {
struct Country { const char* code; const char* name; };
inline constexpr std::array<Country,32> countries{{
 {"US","United States"},{"GB","United Kingdom"},{"ES","Spain"},
 {"DE","Germany"},{"FR","France"},{"IT","Italy"},{"PT","Portugal"},
 {"NL","Netherlands"},{"BE","Belgium"},{"SE","Sweden"},{"NO","Norway"},
 {"DK","Denmark"},{"FI","Finland"},{"PL","Poland"},{"RU","Russia"},
 {"CA","Canada"},{"AU","Australia"},{"NZ","New Zealand"},{"BR","Brazil"},
 {"MX","Mexico"},{"AR","Argentina"},{"IN","India"},{"JP","Japan"},
 {"KR","South Korea"},{"CN","China"},{"TW","Taiwan"},{"HK","Hong Kong"},
 {"SG","Singapore"},{"SA","Saudi Arabia"},{"AE","United Arab Emirates"},
 {"EG","Egypt"},{"ZA","South Africa"}
}};
inline constexpr std::size_t max_countries=256, max_releases=256, max_label_bytes=48;
inline std::string country_code(std::string_view value) {
 if(value.size()!=2)return {};
 std::string code(value);
 for(char& c:code){if(c>='a'&&c<='z')c-=32;if(c<'A'||c>'Z')return {};}
 return code;
}
inline std::size_t country_index(std::string_view value) {
 const auto code=country_code(value);
 for(std::size_t n=0;n<countries.size();++n)if(code==countries[n].code)return n;
 return 0;
}
inline std::string preference(std::string_view value) {return countries[country_index(value)].code;}
inline std::string load_preference(const nlohmann::json& j) {
 const auto it=j.find("age_rating_country");
 return it!=j.end()&&it->is_string()?preference(it->get<std::string>()):"US";
}
inline void save_preference(nlohmann::json& j,std::string_view value) {j["age_rating_country"]=preference(value);}

inline std::string label(std::string value) {
 // Bound bytes before trimming/decoding. Keep published labels, including CJK,
 // verbatim; reject controls, malformed UTF-8 and URL/markup-like values.
 if(value.size()>max_label_bytes)return {};
 auto first=value.find_first_not_of(" \t\r\n"),last=value.find_last_not_of(" \t\r\n");
 if(first==std::string::npos)return {};
 value=value.substr(first,last-first+1);
 for(std::size_t i=0;i<value.size();) {
  auto c=static_cast<unsigned char>(value[i++]);unsigned cp=c;int extra=0;
  if(c<0x80){if(c<32||c==127||std::string_view("<>/:?&#\\").find(char(c))!=std::string_view::npos)return {};continue;}
  if(c>=0xc2&&c<=0xdf){extra=1;cp=c&31;}
  else if(c>=0xe0&&c<=0xef){extra=2;cp=c&15;}
  else if(c>=0xf0&&c<=0xf4){extra=3;cp=c&7;}else return {};
  if(i+extra>value.size())return {};
  for(int n=0;n<extra;++n){auto d=static_cast<unsigned char>(value[i++]);if((d&0xc0)!=0x80)return {};cp=(cp<<6)|(d&63);}
  if((extra==2&&cp<0x800)||(extra==3&&cp<0x10000)||cp>0x10ffff||
     (cp>=0xd800&&cp<=0xdfff)||cp<0xa0||(cp>=0x2000&&cp<=0x206f)||cp==0xfeff)return {};
 }
 if(value=="-"||value=="?")return {};
 return value;
}
inline std::string field(const nlohmann::json& j,const char* key) {
 const auto it=j.find(key);return it!=j.end()&&it->is_string()?it->get<std::string>():"";
}
inline std::vector<Rating> parse(const nlohmann::json& title,bool movie) {
 std::vector<Rating> out;std::vector<int> ranks;
 const auto block=title.find(movie?"release_dates":"content_ratings");
 if(block==title.end()||!block->is_object())return out;
 const auto rows=block->find("results");if(rows==block->end()||!rows->is_array())return out;
 for(std::size_t n=0;n<std::min(rows->size(),max_countries);++n) {
  const auto& row=(*rows)[n];if(!row.is_object())continue;
  auto country=country_code(field(row,"iso_3166_1"));if(country.empty())continue;
  std::string chosen;int best=99;
  if(movie) {
   const auto releases=row.find("release_dates");if(releases==row.end()||!releases->is_array())continue;
   // Prefer theatrical, limited theatrical, digital, physical, TV, premiere.
   // Preserve the first valid published label within the same release type.
   constexpr int rank[]={99,5,1,0,2,3,4};
   for(std::size_t k=0;k<std::min(releases->size(),max_releases);++k) {
    const auto& release=(*releases)[k];if(!release.is_object())continue;
    const auto type=release.find("type");if(type==release.end()||!type->is_number_integer()||*type<1||*type>6)continue;
    const auto rating=label(field(release,"certification"));const int priority=rank[type->get<int>()];
    if(!rating.empty()&&priority<best){chosen=rating;best=priority;}
   }
  } else {chosen=label(field(row,"rating"));best=0;}
  if(chosen.empty())continue;
  auto prior=std::find_if(out.begin(),out.end(),[&](const auto& r){return r.country==country;});
  if(prior==out.end()){out.push_back({country,chosen});ranks.push_back(best);}
  else {const auto i=std::size_t(prior-out.begin());if(best<ranks[i]){prior->label=chosen;ranks[i]=best;}}
 }
 return out;
}
inline std::string badge(const std::vector<Rating>& ratings,std::string_view country) {
 const auto wanted=preference(country);
 for(const auto& code:{wanted,std::string("US")})
  for(std::size_t n=0;n<std::min(ratings.size(),max_countries);++n)
   if(ratings[n].country==code){auto value=label(ratings[n].label);if(!value.empty())return code+" "+value;}
 return {};
}
}
