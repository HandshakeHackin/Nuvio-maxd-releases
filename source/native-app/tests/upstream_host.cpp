// User-facing upstream adaptations. GPL-3.0-or-later.
#include "buffered_log.h"
#include "frame_selection.h"
#include "gfx/font.hpp"
#include <cassert>
#include <future>
#include <iostream>

struct Glyphs : hui::gfx::GlyphSource {
    bool glyph(uint32_t, hui::gfx::GlyphBitmap& out) override { out.advance=10; return true; }
};
struct Frame { double pts; };
int main() {
    Glyphs source; hui::gfx::Font font;
    font.init_dynamic(&source,10,2,8,-2,0,128);
    for (const auto& text : {std::string("verylongstreamnamewithoutspaces"), std::string("你好世界你好世界"),std::string("مرحباالعالممرحباالعالم")}) {
        const auto lines=font.wrap(text,10,30);
        std::string joined;
        for(const auto& l:lines) { assert(font.measure(l,10)<=30); joined+=l; }
        assert(joined==text);
    }
    assert(font.wrap("a\n\nb",10,30).size()==3);
    assert(font.wrap("abcd",10,1).size()==4); // one oversize glyph per line, no loop
    std::vector<Frame> frames={{0},{1.0/24},{2.0/24},{3.0/24}};
    assert(select_video_frame(frames,0.006,true,false,24)==0);
    assert(select_video_frame(frames,0.050,true,false,24)==1);
    assert(select_video_frame(frames,0.125,true,false,24)>=2);
    assert(select_video_frame(frames,0.125,true,true,24)==-1);
    assert(select_video_frame(frames,0.125,false,true,24)==0);
    frames={{1}}; assert(select_video_frame(frames,0.995,true,false,24)==-1);
    for(int hz : {60,120}) {
        int shown=0;
        std::vector<Frame> queue;
        for(int i=0;i<240;i++) queue.push_back({double(i)/24});
        for(int tick=0;tick<hz*10;tick++) {
            int pick=select_video_frame(queue,double(tick)/hz,shown!=0,false,24);
            if(pick>=0) { assert(pick==0); queue.erase(queue.begin()); ++shown; }
        }
        assert(shown==240);
    }
    std::promise<void> entered,release;
    auto unblock=release.get_future().share();
    std::vector<std::string> written;
    BufferedLog log([&](const auto& batch) {
        if(written.empty()) { entered.set_value(); unblock.wait(); }
        written.insert(written.end(),batch.begin(),batch.end());
    },2);
    assert(log.push("first")); entered.get_future().wait();
    assert(log.push("second") && log.push("third"));
    assert(!log.push("overflow")); // producer returned while sink is still blocked
    release.set_value(); log.close();
    assert(written.front()=="first" && written.back()=="third");
    assert(written.size()==4 && written[1].find("1 messages omitted")!=std::string::npos);
    assert(!log.push("after close"));
    std::cout<<"Upstream refresh: Unicode wrapping, 24/60 and 24/120 frame order, bounded catch-up, holds and nonblocking logging passed\n";
}
