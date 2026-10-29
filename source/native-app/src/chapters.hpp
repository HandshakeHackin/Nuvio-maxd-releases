#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace playback {
struct Chapter { double start = 0; std::string title; };
constexpr size_t kMaxChapters = 512;
inline std::vector<Chapter> clean_chapters(std::vector<Chapter> input, double duration) {
    std::vector<Chapter> out;
    for (auto& c : input) {
        if (out.size() == kMaxChapters) break;
        if (!std::isfinite(c.start) || c.start < 0 || (duration > 0 && c.start >= duration)) continue;
        if (c.title.size() > 256) {
            size_t n = 256;
            while (n && (static_cast<unsigned char>(c.title[n]) & 0xc0) == 0x80) --n;
            c.title.resize(n);
        }
        for (char& ch : c.title) if (static_cast<unsigned char>(ch) < 32) ch = ' ';
        const auto first=c.title.find_first_not_of(' ');
        if(first==std::string::npos)c.title.clear();
        else {c.title.erase(0,first);c.title.resize(c.title.find_last_not_of(' ')+1);}
        out.push_back(std::move(c));
    }
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    out.erase(std::unique(out.begin(), out.end(), [](const auto& a, const auto& b) { return std::abs(a.start-b.start)<0.001; }), out.end());
    for (size_t i=0; i<out.size(); ++i)
        if (out[i].title.find_first_not_of(' ') == std::string::npos) out[i].title = "Chapter " + std::to_string(i+1);
    return out;
}
inline int current_chapter(const std::vector<Chapter>& chapters, double position) {
    if (!std::isfinite(position)) return -1;
    const auto it=std::upper_bound(chapters.begin(), chapters.end(), position,
        [](double p,const Chapter& c){return p<c.start;});
    return it==chapters.begin() ? -1 : int(it-chapters.begin())-1;
}
} // namespace playback
