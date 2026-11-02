// Conservative, timing-only alignment to already decoded embedded subtitles.
// No extra stream reads, speech upload or service credentials.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <vector>

namespace subtitle_sync {
struct Timing { double start=0, end=0; };
struct Match { bool confident=false; double scale=1, offset=0, score=0; };
inline std::vector<Timing> clean(std::vector<Timing> input) {
    input.erase(std::remove_if(input.begin(),input.end(),[](const auto& c){
        return !std::isfinite(c.start) || !std::isfinite(c.end) || c.start<0 ||
               c.end<=c.start || c.end-c.start>20 || c.end>28800;
    }),input.end());
    std::sort(input.begin(),input.end(),[](const auto& a,const auto& b){return a.start<b.start;});
    input.erase(std::unique(input.begin(),input.end(),[](const auto& a,const auto& b){return std::abs(a.start-b.start)<.01;}),input.end());
    if(input.size()>4096)input.resize(4096);
    return input;
}
inline Match estimate(std::vector<Timing> reference, std::vector<Timing> target,
                      const std::atomic<bool>* cancel=nullptr) {
    reference=clean(std::move(reference));target=clean(std::move(target));
    if(reference.size()<20 || target.size()<20 || reference.back().start-reference.front().start<45)return {};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    std::vector<Match> ranked;
    // Common PAL/film rate changes; avoid an unconstrained affine fit on a
    // short reference window, which could retime an unrelated subtitle file.
    const std::array<double,7> scales{1,25./23.976,23.976/25.,25./24.,24./25.,24./23.976,23.976/24.};
    const auto stopped=[&]{return (cancel && cancel->load()) || std::chrono::steady_clock::now()>deadline;};
    const bool allow_drift=reference.back().start-reference.front().start>=600 &&
        reference.back().start-reference.front().start >= .75*(target.back().start-target.front().start);
    for(double scale:scales) {
        if(scale!=1 && !allow_drift)continue;
        std::map<int,int> votes;
        for(size_t i=0;i<reference.size();i+=std::max<size_t>(1,reference.size()/48)) {
            const auto& r=reference[i];
            for(const auto& t:target) {
                const double offset=r.start-t.start*scale;
                if(std::abs(offset)<=180)++votes[int(std::lround(offset*5))];
            }
            if(stopped())return {};
        }
        std::vector<std::pair<int,int>> bins(votes.begin(),votes.end());
        std::stable_sort(bins.begin(),bins.end(),[](auto a,auto b){return a.second>b.second;});
        if(bins.size()>16)bins.resize(16);
        for(const auto& [bin,count]:bins) {
            if(count<4)continue;
            for(int fine=-4;fine<=4;++fine) {
                if(stopped())return {};
                const double offset=bin/5.0+fine*.05;
                if(std::abs(offset)>180)continue;
                int eligible=0,matched=0;double cost=0;
                std::array<int,3> part_total{},part_match{};
                std::vector<bool> used(reference.size(),false);
                for(const auto& t:target) {
                    const double at=t.start*scale+offset;
                    if(at<reference.front().start-.3 || at>reference.back().start+.3)continue;
                    ++eligible;
                    const int part=std::clamp(int(3*(at-reference.front().start)/(reference.back().start-reference.front().start+.001)),0,2);
                    ++part_total[part];
                    auto it=std::lower_bound(reference.begin(),reference.end(),at,[](const auto& r,double s){return r.start<s;});
                    size_t best=reference.size();double distance=1e9;
                    for(int d=-1;d<=0;++d) {
                        auto idx=it-reference.begin()+d;
                        if(idx<0 || size_t(idx)>=reference.size())continue;
                        const auto& r=reference[size_t(idx)];
                        const double start=std::abs(r.start-at),end=std::abs(r.end-(t.end*scale+offset));
                        if(start<=.35 && end<=.75 && start+end<distance){best=size_t(idx);distance=start+end;}
                    }
                    if(best<reference.size() && !used[best]){used[best]=true;++matched;++part_match[part];cost+=distance;}
                }
                if(matched<20 || eligible<20 || double(matched)/eligible<.9 ||
                   double(matched)/reference.size()<.75 || cost/matched>.55)continue;
                bool balanced=true;
                for(int p=0;p<3;++p)if(part_match[p]<4 || part_total[p]<4 || double(part_match[p])/part_total[p]<.8)balanced=false;
                if(!balanced)continue;
                ranked.push_back({true,scale,offset,double(matched)/eligible-cost/matched*.1});
            }
        }
    }
    if(ranked.empty())return {};
    std::sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.score>b.score;});
    const auto best=ranked.front();
    for(const auto& other:ranked) {
        const double a=std::abs((best.scale-other.scale)*target.front().start+best.offset-other.offset);
        const double b=std::abs((best.scale-other.scale)*target.back().start+best.offset-other.offset);
        if(std::max(a,b)>1.5 && best.score-other.score<.06)return {};
    }
    return best;
}
} // namespace subtitle_sync
