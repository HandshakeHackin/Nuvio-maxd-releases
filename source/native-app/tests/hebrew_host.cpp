#include "dynamic_font.hpp"
#include "bidi.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) try {
    if(argc!=2)throw std::runtime_error("font directory required");
    for(const char* weight:{"Regular","Bold"}) {
        sx::TtfChain old;
        if(!old.load(argv[1],{{std::string("NotoSans-")+weight+".ttf"}}))throw std::runtime_error("base font missing");
        hui::gfx::GlyphBitmap g;
        if(old.glyph(0x5e9,g))throw std::runtime_error("fixture no longer reproduces missing Hebrew");
        sx::TtfChain chain;
        if(!chain.load(argv[1],{{std::string("NotoSans-")+weight+".ttf"},{std::string("NotoSansHebrew-")+weight+".ttf"}}))throw std::runtime_error("fallback load failed");
        for(unsigned cp=0x5d0;cp<=0x5ea;++cp)
            if(!chain.glyph(cp,g)||g.pixels.empty()||g.advance<=0)throw std::runtime_error("Hebrew letter did not rasterize");
        if(!chain.glyph('A',g)||g.pixels.empty())throw std::runtime_error("Latin fallback regression");
    }
    if(bidi_visual("שלום")!="םולש")throw std::runtime_error("Hebrew visual order incorrect");
    std::cout<<"Hebrew: missing-font reproduction, both fallback weights, 27 letter rasters and RTL order passed\n";
    return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
