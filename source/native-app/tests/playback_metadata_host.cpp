#include "chapters.hpp"
#include "stream_colour.hpp"
#include "../gl/artwork_limits.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>

int main(int argc,char** argv) {
    assert(argc==3);
    std::ifstream f(argv[1],std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
    auto colour=hevc_colour::extradata(bytes.data(),bytes.size());
    assert(colour && colour->primaries==9 && colour->matrix==9 && colour->transfer==16);
    std::ifstream sdr_file(argv[2],std::ios::binary);
    const std::vector<uint8_t> sdr((std::istreambuf_iterator<char>(sdr_file)),{});
    const auto sdr_colour=hevc_colour::extradata(sdr.data(),sdr.size());
    assert(sdr_colour && sdr_colour->transfer==1 && sdr_colour->primaries==1 && sdr_colour->matrix==1);
    // Every truncation, including a cut through an emulation-prevention byte,
    // must be safe and must not fabricate a different colour description.
    for(size_t n=0;n<bytes.size();++n)if(auto c=hevc_colour::extradata(bytes.data(),n))
        assert(c->transfer==16 && c->primaries==9 && c->matrix==9);
    // Convert the same real SPS to hvcC; both input representations must agree.
    std::vector<uint8_t> sps;
    for(size_t i=0;i+5<bytes.size();++i)if(bytes[i]==0 && bytes[i+1]==0 && bytes[i+2]==1 && ((bytes[i+3]>>1)&63)==33) {
        size_t end=i+3;
        for(end=i+5;end+3<bytes.size();++end)if(!bytes[end]&&!bytes[end+1]&&bytes[end+2]==1)break;
        if(end+3>=bytes.size())end=bytes.size();
        else if(!bytes[end-1])--end;
        sps.assign(bytes.begin()+i+3,bytes.begin()+end);break;
    }
    assert(!sps.empty());
    std::vector<uint8_t> hvcc(23);hvcc[0]=1;hvcc[22]=1;
    hvcc.insert(hvcc.end(),{33,0,1,uint8_t(sps.size()>>8),uint8_t(sps.size())});
    hvcc.insert(hvcc.end(),sps.begin(),sps.end());
    auto h=hevc_colour::extradata(hvcc.data(),hvcc.size());assert(h && h->transfer==16);
    AVCodecParameters p{};p.codec_id=AV_CODEC_ID_HEVC;p.extradata=hvcc.data();p.extradata_size=int(hvcc.size());
    p.color_trc=AVCOL_TRC_UNSPECIFIED;p.color_space=AVCOL_SPC_UNSPECIFIED;p.color_primaries=AVCOL_PRI_UNSPECIFIED;
    assert(recover_hevc_colour(&p));assert(p.color_trc==AVCOL_TRC_SMPTE2084 && p.color_space==AVCOL_SPC_BT2020_NCL);
    p.color_trc=AVCOL_TRC_BT709;p.color_space=AVCOL_SPC_BT709;p.color_primaries=AVCOL_PRI_BT709;
    assert(!recover_hevc_colour(&p));assert(p.color_trc==AVCOL_TRC_BT709);
    p.codec_id=AV_CODEC_ID_H264;p.color_trc=AVCOL_TRC_UNSPECIFIED;assert(!recover_hevc_colour(&p));
    auto invalid=hvcc;invalid[26]=255;invalid[27]=255;assert(!hevc_colour::extradata(invalid.data(),invalid.size()));
    std::mt19937 rng(44);for(int i=0;i<10000;++i) {
        std::vector<uint8_t> malformed(1+rng()%300);
        for(auto& b:malformed)b=uint8_t(rng());
        (void)hevc_colour::extradata(malformed.data(),malformed.size());
    }
    for(int i=0;i<5000;++i) {
        auto mutated=hvcc;mutated[rng()%mutated.size()]^=uint8_t(1u<<(rng()%8));
        (void)hevc_colour::extradata(mutated.data(),mutated.size());
    }
    const auto chapters=playback::clean_chapters({{5,"Middle"},{0,""},{5,"Duplicate"},{-1,"Bad"},
        {std::numeric_limits<double>::quiet_NaN(),"NaN"},{10,"End"},{8,"Later\nchapter"}},10);
    assert(chapters.size()==3 && chapters[0].title=="Chapter 1" && chapters[2].title=="Later chapter");
    assert(playback::current_chapter(chapters,-1)==-1 && playback::current_chapter(chapters,5)==1 && playback::current_chapter(chapters,9)==2);
    assert(playback::clean_chapters(std::vector<playback::Chapter>(10000,{1,""}),10).size()==1);
    using namespace sx::artwork;
    assert(image_allowed(3840,2160) && !image_allowed(16384,16384) && !image_allowed(-1,3));
    assert(!can_publish(16,0,1) && !can_publish(0,kReadyBytes,1) && can_publish(0,0,kReadyBytes));
    std::cout<<"Real HEVC SPS (Annex B/hvcC), missing-only colour fallback, malformed input, chapters and artwork bounds passed\n";
}
