#include "live_channel_prefs.hpp"
#include "../ui/start_page.hpp"
#include <cassert>
#include <iostream>
int main() {
 using namespace live_channel_prefs;
 live_tv::Channel primary;primary.id="10";primary.name="UK: News 24 HD";primary.epg_id="news.uk";primary.url="https://first.invalid/private";
 primary.headers={"Authorization: fixture-primary"};
 const auto id=key("a",primary);Json a=Json::object(),b=Json::object();
 const std::string rename="أخبار اليوم";const bool yes=true,no=false;
 assert(set(a,"a",id,&rename,&yes));assert(name(a,"a",primary)==rename&&hidden(a,"a",primary));
 assert(name(b,"a",primary)==primary.name&&!hidden(b,"a",primary));
 assert(name(a,"b",primary)==primary.name&&!hidden(a,"b",primary));
 assert(key("a",primary)==std::to_string(fnv1a("a\n10\nUK: News 24 HD")));
 assert(primary.name=="UK: News 24 HD"&&primary.epg_id=="news.uk"&&primary.headers[0]=="Authorization: fixture-primary");
 const std::string empty;assert(set(a,"a",id,&empty,nullptr)&&name(a,"a",primary)==primary.name&&hidden(a,"a",primary));
 assert(set(a,"a",id,nullptr,&no)&&object_at(a,"channel_edits").empty());
 for(const auto& invalid:{std::string("a\nb"),std::string(161,'x'),std::string("\xc0\xaf"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80")}) {
  const auto before=a;assert(!set(a,"a",id,&invalid,nullptr)&&a==before);
 }
 assert(!set(a,"../other",id,&rename,nullptr));assert(!set(a,"a","../id",&rename,nullptr));
 for(int n=0;n<1000;++n)assert(set(a,"a",std::to_string(n),nullptr,&yes));
 assert(!set(a,"b","1001",nullptr,&yes));assert(set(a,"a","0",&rename,nullptr));
 assert(set(a,"a","1",nullptr,&no)&&set(a,"b","1001",nullptr,&yes));
 Json bad=a;bad["channel_edits"]["a"]["0"]["hidden"]="yes";assert(!valid(bad));
 bad=a;bad["channel_edits"]["a"]["0"]["url"]="unexpected";assert(!valid(bad));
 bad=a;bad["last_groups"]=Json::array();assert(!valid(bad));bad["sources"]=Json::array();normalise(bad);assert(bad["sources"].is_array()&&valid(bad));
 remember_group(b,"a","News");remember_group(b,"b","Sports");assert(last_group(b,"a")=="News"&&last_group(b,"b")=="Sports");
 remember_group(b,"a","");assert(last_group(b,"a").empty());
 std::vector<live_tv::Channel> channels{primary};
 auto copy=primary;copy.id="11";copy.name="UK News 24 FHD 5.1";copy.url="https://second.invalid/secret";copy.headers={"Cookie: fixture-second"};channels.push_back(copy);
 copy.id="12";copy.epg_id.clear();copy.name="UK News 24 SD";copy.url="https://third.invalid/secret";channels.push_back(copy);
 channels.push_back(copy); // duplicate endpoint/headers appears only once
 copy.id="13";copy.name="US News 24 HD";copy.url="https://region.invalid";channels.push_back(copy);
 copy.id="14";copy.name="UK News 24 +1 HD";copy.url="https://timeshift.invalid";channels.push_back(copy);
 copy.id="15";copy.name="UK News 2 HD";copy.url="https://different.invalid";channels.push_back(copy);
 assert((backups(channels,0)==std::vector<std::size_t>{1,2}));
 assert(channels[1].headers==std::vector<std::string>{"Cookie: fixture-second"});
 assert(match_name("ESPN2 HD")!=match_name("ESPN FHD"));
 assert(match_name("HD UHD 4K").empty());assert(match_name("BBC +1 HD")!=match_name("BBC HD"));
 assert(match_name("日本テレビ HD")==match_name("日本テレビ FHD"));
 assert(backups(channels,channels.size()).empty());
 for(int n=0;n<100;++n){auto extra=primary;extra.id=std::to_string(n);extra.url="https://copies.invalid/"+std::to_string(n);channels.push_back(extra);}
 assert(backups(channels,0).size()==32);
 std::vector<live_tv::Channel> over_limit(live_tv::max_channels+1);
 over_limit[0]=primary;over_limit.back()=channels[1];
 assert(backups(over_limit,0).empty());
 for(const auto& choice:sx::start_page::choices){assert(sx::start_page::valid(choice.id));assert(sx::start_page::page(choice.id,true)==choice.page);}
 assert(!sx::start_page::valid("settings"));assert(!sx::start_page::valid("http://private.invalid"));
 assert(sx::start_page::page("unknown",true)==sx::Page::board);
 assert(sx::start_page::page("sports",false)==sx::Page::board);
 std::cout<<"Launcher feature cores: stable identities, UTF-8 edits, source/profile isolation, limits, remembered groups, conservative backups and start-page fallbacks passed\n";
}
