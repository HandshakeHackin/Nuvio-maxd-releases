#pragma once
#include <string_view>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <initializer_list>

namespace bt {
// Head-only startup is appropriate for EBML and packetized TS. MP4 can need
// its tail moov atom, so never disable seeking solely because the input is P2P.
inline bool head_only_start(std::string_view header, double resume) {
    if (!(resume <= 0)) return false;
    if (header.starts_with(std::string_view("\x1a\x45\xdf\xa3",4))) return true;
    for (const auto stride : {188u,192u}) {
        const unsigned offset = stride == 192 ? 4 : 0;
        if (header.size() > offset+2*stride &&
            static_cast<unsigned char>(header[offset]) == 0x47 &&
            static_cast<unsigned char>(header[offset+stride]) == 0x47 &&
            static_cast<unsigned char>(header[offset+2*stride]) == 0x47) return true;
    }
    return false;
}
}

namespace bt {
inline std::string readable_bytes(int64_t bytes) {
    char out[48];const double n=bytes>0?static_cast<double>(bytes):0;
    if(n>=1073741824)std::snprintf(out,sizeof(out),"%.1f GB",n/1073741824);
    else if(n>=1048576)std::snprintf(out,sizeof(out),"%.1f MB",n/1048576);
    else std::snprintf(out,sizeof(out),"%.0f KB",n/1024);
    return out;
}
inline std::string buffer_summary(int64_t ready,double seconds) {
    char out[48];seconds=std::isfinite(seconds)?std::min(86400.0,std::max(0.0,seconds)):0;
    std::snprintf(out,sizeof(out),"%.1f s buffered",seconds);
    return readable_bytes(ready)+" ready ahead · "+out;
}
}
