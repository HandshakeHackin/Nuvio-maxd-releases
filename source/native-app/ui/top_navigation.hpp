// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.hpp"
#include <algorithm>
#include <vector>
namespace sx::top_navigation {
struct Tab {Page page;const char* label;};
inline std::vector<Tab> tabs(bool iptv,bool servers,bool torrents,bool sports) {
 std::vector<Tab> out={{Page::board,"Board"},{Page::search,"Search"},{Page::discover,"Discover"},{Page::library,"Library"},{Page::livetv,"Live TV"}};
 if(iptv)out.push_back({Page::iptv,"IPTV Library"});
 if(servers)out.push_back({Page::servers,"Media Servers"});
 if(torrents)out.push_back({Page::torrents,"Torrents"});
 if(sports)out.push_back({Page::sports,"Sports Hub"});
 out.push_back({Page::addons,"Addons"});out.push_back({Page::settings,"Settings"});return out;
}
inline int index(const std::vector<Tab>& list,Page page){for(int n=0;n<int(list.size());++n)if(list[n].page==page)return n;return 0;}
// Shoulder shortcuts are navigation-only. Live TV and torrent pages already own them.
inline bool shoulders(bool top,bool browsing,bool focused,Page page) {
 return top&&browsing&&(focused||(page!=Page::livetv&&page!=Page::torrents));
}
struct Window {int first=0,count=0;};
inline Window window(int count,int cursor) {const int visible=std::min(7,count);return {std::clamp(cursor-visible/2,0,std::max(0,count-visible)),visible};}
} // namespace sx::top_navigation
