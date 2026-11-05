#pragma once
#include <string_view>
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
