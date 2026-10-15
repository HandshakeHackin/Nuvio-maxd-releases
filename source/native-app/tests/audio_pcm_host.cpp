#include "audio_pcm.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
void require(bool b,const char* why) { if(!b) throw std::runtime_error(why); }
std::vector<int16_t> convert(const AVChannelLayout& in, const std::vector<float>& samples) {
    AVChannelLayout out = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext* swr=nullptr;
    require(swr_alloc_set_opts2(&swr,&out,AV_SAMPLE_FMT_S16,48000,&in,AV_SAMPLE_FMT_FLT,48000,0,nullptr)>=0,"allocate resampler");
    require(configure_pcm_mix(swr) && swr_init(swr)>=0,"configure downmix");
    int frames=int(samples.size())/in.nb_channels;
    std::vector<int16_t> result(size_t(frames)*2);
    uint8_t* dst[]={reinterpret_cast<uint8_t*>(result.data())};
    const uint8_t* src[]={reinterpret_cast<const uint8_t*>(samples.data())};
    require(swr_convert(swr,dst,frames,src,frames)==frames,"convert samples");
    swr_free(&swr);return result;
}
int main() try {
    AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO, mono=AV_CHANNEL_LAYOUT_MONO, surround=AV_CHANNEL_LAYOUT_5POINT1;
    auto s=convert(stereo,{.25f,-.5f,.5f,.125f});
    require(s[0]==8192 && s[1]==-16384 && s[2]==16384 && s[3]==4096,"stereo levels/polarity must be unchanged");
    auto m=convert(mono,{.5f});require(m[0]>0 && m[0]==m[1],"mono reaches both speakers");
    for(int c=0;c<6;++c) {
        std::vector<float> input(6,0);input[c]=.5f;auto v=convert(surround,input);
        require(v[0]!=0 || v[1]!=0,"surround channel lost in stereo");
        if(c==2 || c==3) require(v[0]>0 && v[0]==v[1],"center/bass must reach both speakers");
        if(c==0 || c==4) require(v[0]>0 && v[1]==0,"left speaker routed incorrectly");
        if(c==1 || c==5) require(v[1]>0 && v[0]==0,"right speaker routed incorrectly");
    }
    auto loud=convert(surround,std::vector<float>(6,1));
    require(loud[0]>0 && loud[1]>0,"full-scale mix inverted or overflowed");
    require(pcm_audible_seconds(0,48000,256,false)==0,"clock starts negative");
    require(std::abs(pcm_audible_seconds(1,48000,256,false)-(1-512.0/48000))<1e-9,"PCM clock estimate wrong");
    require(pcm_audible_seconds(1,192000,1024,true)==1,"bitstream clock altered");
    PcmLevels levels;levels.add(s.data(),s.size(),false);
    require(std::abs(levels.peak_db()+6.0206)<.001 && levels.rms_db()<levels.peak_db(),"level diagnostics wrong");
    std::puts("Audio PCM: stereo identity, mono, isolated dialogue/surround/bass, headroom, clock and level diagnostics passed");
    return 0;
} catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
