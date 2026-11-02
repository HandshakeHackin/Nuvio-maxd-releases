#include "pcm_night_mode.h"
#include "subtitle_sync.hpp"
#include "episode_playback.hpp"
#include "experience_preferences.hpp"
#include <cassert>
#include <iostream>
#include <limits>
int main() {
    PcmNightMode night;
    std::vector<float> quiet(48000*2,.0316227766f),identity=quiet;
    night.process(quiet.data(),48000,2,true,48000,0);assert(quiet==identity);
    night.process(quiet.data(),48000,2,true,48000,2);
    assert(std::abs(20*std::log10(quiet.back()/.0316227766)-10)<.03);
    std::vector<float> linked(48000*2);
    for(size_t i=0;i<linked.size();i+=2){linked[i]=.02;linked[i+1]=-.01;}
    night.reset();night.process(linked.data(),48000,2,true,48000,3);
    assert(std::abs(linked[linked.size()-2]/linked.back()+2)<1e-6);
    float full[]={1,-1,.9f,-.9f};night.process(full,2,2,true,48000,3);
    for(float v:full)assert(std::abs(v)<=.891251);
    float noise[]={1e-6f,-1e-6f};night.reset();night.process(noise,1,2,true,48000,3);assert(noise[0]==1e-6f);
    float invalid[]={std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()};night.process(invalid,1,2,true,48000,3);assert(invalid[0]==0 && invalid[1]==0);
    int16_t pcm[]={1000,-500,32767,-32768};night.reset();night.process(pcm,2,2,false,48000,3);assert(std::abs(int(pcm[0])+2*int(pcm[1]))<=1 && pcm[2]>0 && pcm[3]<0);
    sx::ExperiencePreferences defaults;assert(defaults.night_mode==0 && defaults.lfe_mix_percent==50 && !defaults.subtitle_auto_sync);
    sx::load_experience({{"night_mode",99},{"lfe_mix_percent",-10},{"chapter_skip_segments",false},{"subtitle_auto_sync",true},{"trailer_language","es"}},defaults);
    assert(defaults.night_mode==3 && defaults.lfe_mix_percent==0 && !defaults.chapter_skip_segments && defaults.subtitle_auto_sync && defaults.trailer_language=="es");
    nlohmann::json saved;sx::save_experience(saved,defaults);sx::ExperiencePreferences loaded;sx::load_experience(saved,loaded);assert(loaded.night_mode==3 && loaded.lfe_mix_percent==0 && loaded.subtitle_auto_sync && loaded.trailer_language=="es");
    sx::load_experience({{"trailer_language","en&key=bad"}},loaded);assert(loaded.trailer_language=="es");
    using namespace sx::episode_playback;
    auto chapters=chapter_segments({{0,"Recap"},{25,"Intro"},{50,"OP 1"},{125,"Part A"},{900,"Ending Theme"},{990,"Post-Credits Scene"},{1020,"Credits"}},1100);
    assert(chapters.size()==4);
    bool opening=false,scene=false;for(const auto& c:chapters){if(c.kind==Kind::intro){assert(c.start==50 && c.end==125);opening=true;}if(c.kind==Kind::post_credits){assert(c.start==990 && c.end==1020);scene=true;}}
    assert(opening && scene);
    auto ordinary=chapter_segments({{0,"Introduction to a murder"},{90,"End of the road"},{180,"Main story"}},500);assert(ordinary.empty());
    assert(chapter_segments({{0,"Intro"},{300,"Story"}},1000).empty());
    Flow flow;flow.segments={{Kind::intro,10,60}};flow.update(20,1000,.1,true,false,false,false);flow.skipped();
    flow.replace_segments({{Kind::recap,0,10},{Kind::intro,10,60}});flow.update(20,1000,.1,true,false,false,false);assert(flow.active<0);
    std::vector<subtitle_sync::Timing> reference,target;double at=0;
    for(int i=0;i<120;++i){at+=4+(i*7%23)*.27;const double duration=1+(i*11%9)*.21;reference.push_back({at,at+duration});target.push_back({at-3,at+duration-3});}
    auto result=subtitle_sync::estimate(reference,target);assert(result.confident && std::abs(result.offset-3)<.1 && result.scale==1);
    std::vector<subtitle_sync::Timing> scaled;
    for(const auto& c:reference)scaled.push_back({c.start/(25./24.),c.end/(25./24.)});
    auto drift=subtitle_sync::estimate(reference,scaled);assert(drift.confident && std::abs(drift.scale-25./24.)<1e-8);
    std::atomic<bool> cancel{true};assert(!subtitle_sync::estimate(reference,target,&cancel).confident);
    assert(!subtitle_sync::estimate({{0,1}},target).confident);
    auto wrong=target;for(size_t i=0;i<wrong.size();++i){wrong[i].start+=i%5*2;wrong[i].end+=i%5*2;}assert(!subtitle_sync::estimate(reference,wrong).confident);
    std::vector<subtitle_sync::Timing> periodic;for(int i=0;i<100;++i)periodic.push_back({double(i*10),double(i*10+2)});assert(!subtitle_sync::estimate(periodic,periodic).confident);
    auto partial=std::vector<subtitle_sync::Timing>(reference.begin(),reference.begin()+22);auto p=subtitle_sync::estimate(partial,target);assert(p.confident && p.scale==1 && std::abs(p.offset-3)<.1);
    std::cout<<"Upstream adaptations: Night Mode, linked channels/limiter, preference defaults and persistence, chapter skip protection, stable suppression and confident subtitle alignment passed\n";
}
