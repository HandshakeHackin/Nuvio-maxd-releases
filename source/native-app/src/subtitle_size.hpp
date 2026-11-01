// One 1080-line size scale for subtitle preview, plain text and libass.
#pragma once
#include <array>
#include <string_view>
namespace subtitle_size {
inline constexpr std::array<const char*,5> ids{"sub-s","sub-m","sub-l","sub-xl","sub-xxl"};
inline constexpr std::array<const char*,5> labels{"Small","Medium","Large","Extra large (150%)","Largest (200%)"};
inline constexpr std::array<int,5> pixels{42,50,60,75,100};
inline int index(std::string_view id) {
    for (int i=0;i<int(ids.size());++i) if(id==ids[i]) return i;
    return 1;
}
inline int height(int size) { return pixels[size>=0 && size<int(pixels.size()) ? size : 1]; }
}
