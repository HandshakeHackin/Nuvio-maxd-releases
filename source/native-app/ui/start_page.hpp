// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.hpp"
#include <array>
#include <string_view>
namespace sx::start_page {
struct Choice { const char* id; const char* label; Page page; };
inline constexpr std::array<Choice,5> choices{{
 {"home","Home",Page::board},{"discover","Discover",Page::discover},
 {"library","Library",Page::library},{"livetv","Live TV",Page::livetv},
 {"sports","Sports",Page::sports}
}};
inline std::size_t index(std::string_view id) {
 for(std::size_t n=0;n<choices.size();++n)if(id==choices[n].id)return n;
 return 0;
}
inline bool valid(std::string_view id) { return id==choices[index(id)].id; }
inline Page page(std::string_view id,bool sports_enabled) {
 const auto selected=choices[index(id)].page;
 return selected==Page::sports&&!sports_enabled?Page::board:selected;
}
}
