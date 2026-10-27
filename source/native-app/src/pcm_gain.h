// PCM-only gain; encoded bitstream bytes must never pass through this helper.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
inline void pcm_apply_gain(void* data, size_t samples, bool floating, int percent) {
    percent=std::clamp(percent,0,300);
    if(percent==100)return;
    const float gain=percent/100.f;
    if(floating) {
        auto* p=static_cast<float*>(data);
        for(size_t i=0;i<samples;++i)p[i]=std::isfinite(p[i])?std::clamp(p[i]*gain,-1.f,1.f):0.f;
    } else {
        auto* p=static_cast<int16_t*>(data);
        for(size_t i=0;i<samples;++i)p[i]=int16_t(std::clamp(std::lround(p[i]*gain),-32768L,32767L));
    }
}
