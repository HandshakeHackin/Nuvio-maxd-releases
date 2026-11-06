// Time-grid geometry uses clipped intervals, never untrusted programme lengths.
#pragma once
#include <algorithm>
#include <cstdint>
namespace guide_grid {
struct Span { bool visible=false; double left=0,width=0; };
inline Span clip(int64_t begin,int64_t end,int64_t window,int64_t duration){
    if(duration<=0 || end<=begin)return {};
    const auto left=std::max(begin,window), right=std::min(end,window+duration);
    if(right<=left)return {};
    return {true,double(left-window)/duration,double(right-left)/duration};
}
}
