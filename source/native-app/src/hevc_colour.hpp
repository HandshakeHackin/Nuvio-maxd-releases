// Bounded HEVC SPS/VUI reader. Only fills missing container colour fields;
// neither a Main10 profile nor a filename is evidence of HDR.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace hevc_colour {
struct Description { int primaries=2, transfer=2, matrix=2; bool full=false; };
class Bits {
    const std::vector<uint8_t>& data_; size_t at_=0;
public:
    bool ok=true;
    explicit Bits(const std::vector<uint8_t>& data):data_(data){}
    uint32_t read(unsigned n) {
        if (!ok || n>32 || n>data_.size()*8-at_) {ok=false;return 0;}
        uint32_t out=0;
        while(n--) {out=(out<<1)|((data_[at_/8]>>(7-at_%8))&1);++at_;}
        return out;
    }
    void skip(unsigned n) {while(n && ok){const unsigned k=std::min(n,32u);read(k);n-=k;}}
    uint32_t ue() {
        unsigned zeros=0;
        while(ok && !read(1)) if(++zeros>30){ok=false;return 0;}
        return ok ? ((1u<<zeros)-1)+read(zeros) : 0;
    }
    unsigned bounded(unsigned limit) {const unsigned n=ue();if(n>limit)ok=false;return ok?n:0;}
};
inline std::optional<Description> sps(const uint8_t* nal,size_t size) {
    if(!nal || size<3 || size>65536 || (nal[0]&0x80) || ((nal[0]>>1)&63)!=33 ||
       (nal[0]&1) || (nal[1]>>3) || !(nal[1]&7)) return {};
    std::vector<uint8_t> rbsp;rbsp.reserve(size);
    unsigned zeros=0;
    for(size_t i=2;i<size;++i) {
        if(zeros>=2 && nal[i]==3) {
            if(i+1==size || nal[i+1]>3)return {};
            zeros=0;continue;
        }
        rbsp.push_back(nal[i]);zeros=nal[i]==0?zeros+1:0;
    }
    Bits b(rbsp);b.skip(4);const unsigned layers=b.read(3);b.skip(1);
    if(layers>6)return {};
    b.skip(96);
    bool profiles[7]{},levels[7]{};
    for(unsigned i=0;i<layers;++i){profiles[i]=b.read(1);levels[i]=b.read(1);}
    if(layers) b.skip((8-layers)*2);
    for(unsigned i=0;i<layers;++i){if(profiles[i])b.skip(88);if(levels[i])b.skip(8);}
    b.bounded(15);const unsigned chroma=b.bounded(3);if(chroma==3)b.skip(1);
    b.bounded(65535);b.bounded(65535);
    if(b.read(1))for(int i=0;i<4;++i)b.ue();
    b.bounded(8);b.bounded(8);const unsigned poc=b.bounded(12)+4;
    const bool ordering=b.read(1);
    for(unsigned i=ordering?0:layers;i<=layers;++i){b.ue();b.ue();b.ue();}
    for(int i=0;i<6;++i)b.ue();
    if(b.read(1) && b.read(1)) {
        for(unsigned sizeId=0;sizeId<4;++sizeId)for(unsigned matrixId=0;matrixId<6;matrixId+=(sizeId==3?3:1)) {
            if(!b.read(1))b.ue();
            else {
                if(sizeId>1)b.ue(); // signed Exp-Golomb has the same bit length
                const unsigned count=std::min(64u,1u<<(4+(sizeId<<1)));
                for(unsigned j=0;j<count;++j)b.ue();
            }
        }
    }
    b.skip(2);
    if(b.read(1)){b.skip(8);b.ue();b.ue();b.skip(1);}
    const unsigned sets=b.bounded(64);unsigned previous=0;
    for(unsigned i=0;i<sets && b.ok;++i) {
        unsigned count=0;
        if(i && b.read(1)) {
            b.skip(1);b.ue();
            for(unsigned j=0;j<=previous;++j) {const bool used=b.read(1);if(used || b.read(1))++count;}
        } else {
            const unsigned neg=b.bounded(64),pos=b.bounded(64);
            if(neg+pos>64)return {};
            count=neg+pos;
            for(unsigned j=0;j<count;++j){b.ue();b.skip(1);}
        }
        if(count>64)return {};
        previous=count;
    }
    if(b.read(1)){const unsigned n=b.bounded(32);for(unsigned i=0;i<n;++i)b.skip(poc+1);}
    b.skip(2);
    if(!b.read(1))return {};
    if(b.read(1)){if(b.read(8)==255)b.skip(32);}
    if(b.read(1))b.skip(1);
    if(!b.read(1))return {};
    b.skip(3);Description out;out.full=b.read(1);
    if(b.read(1)){out.primaries=b.read(8);out.transfer=b.read(8);out.matrix=b.read(8);}
    return b.ok?std::optional<Description>(out):std::nullopt;
}
inline std::optional<Description> extradata(const uint8_t* data,size_t size) {
    if(!data || size<3 || size>(2u<<20))return {};
    if(data[0]==1) { // HEVCDecoderConfigurationRecord (hvcC)
        if(size<23)return {};
        size_t p=23;unsigned seen=0;
        for(unsigned a=0;a<data[22];++a) {
            if(p+3>size)return {};
            const unsigned type=data[p++]&63;
            const unsigned n=(unsigned(data[p])<<8)|data[p+1];p+=2;
            for(unsigned i=0;i<n;++i) {
                if(++seen>256 || p+2>size)return {};
                const size_t len=(size_t(data[p])<<8)|data[p+1];p+=2;
                if(len>size-p)return {};
                if(type==33)if(auto colour=sps(data+p,len))return colour;
                p+=len;
            }
        }
        return {};
    }
    // Annex B, including both three- and four-byte start codes.
    size_t start=size;unsigned count=0;
    for(size_t i=0;i+3<=size;++i) {
        if(data[i] || data[i+1] || data[i+2]!=1)continue;
        size_t end=i;if(end && !data[end-1])--end;
        if(start<end)if(auto colour=sps(data+start,end-start))return colour;
        if(++count>256)return {};
        start=i+3;i+=2;
    }
    return start<size?sps(data+start,size-start):std::nullopt;
}
} // namespace hevc_colour
