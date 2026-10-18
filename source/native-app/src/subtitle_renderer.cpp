// SPDX-License-Identifier: GPL-3.0-or-later
#include "subtitle_renderer.h"
#include "util.h"
#include "video_frame.h"
#include "subtitles.h"
extern "C" {
#include <ass/ass.h>
#include <libavcodec/avcodec.h>
}
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>

namespace {
constexpr int W=1920, H=1080;
constexpr size_t kBitmapBudget=32u<<20;
struct Tile { int x=0,y=0,w=0,h=0; std::vector<uint8_t> rgba; };
struct Event { double start=0,end=0; std::vector<Tile> tiles; size_t bytes=0; };
void ass_message(int level,const char* fmt,va_list args,void*) {
    if(level>2) return;
    char b[384]; vsnprintf(b,sizeof(b),fmt,args); dlog("subtitles: %s",b);
}
void blend(uint8_t* dst,const uint8_t* src) {
    const unsigned a=src[3], da=dst[3], oa=a+(da*(255-a)+127)/255;
    if(!oa) return;
    for(int c=0;c<3;++c) dst[c]=uint8_t((unsigned(src[c])*a+unsigned(dst[c])*da*(255-a)/255+oa/2)/oa);
    dst[3]=uint8_t(oa);
}
const char* family(const std::string& lang) {
    const auto l=lower(lang);
    if(starts_with(l,"ar")||starts_with(l,"fa")||starts_with(l,"per")||starts_with(l,"ur")) return "Noto Naskh Arabic UI";
    if(starts_with(l,"ja")||starts_with(l,"jp")||starts_with(l,"ko")||starts_with(l,"zh")||starts_with(l,"chi")||starts_with(l,"cmn")||starts_with(l,"yue")) return "Noto Sans CJK SC";
    if(l=="he"||l=="heb"||l=="iw"||starts_with(l,"he-")) return "Noto Sans Hebrew";
    if(starts_with(l,"th")) return "Noto Sans Thai";
    return "Inter";
}
ASS_Track* text_track(ASS_Library* library,const std::string& lang) {
    const std::string script="[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n"
      "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
      "Style: Default,"+std::string(family(lang))+",38,&H00FFFFFF,&H00FFFFFF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,2,0,2,120,120,110,1\n"
      "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    return ass_read_memory(library,const_cast<char*>(script.data()),script.size(),nullptr);
}
std::string cue_ass(std::string s) {
    s=replace_all(s,"<br/>","\\N"); s=replace_all(s,"<strong>","{\\b1}"); s=replace_all(s,"</strong>","{\\b0}");
    s=replace_all(s,"<em>","{\\i1}"); s=replace_all(s,"</em>","{\\i0}");
    s=replace_all(s,"&lt;","<"); s=replace_all(s,"&gt;",">"); s=replace_all(s,"&quot;","\""); s=replace_all(s,"&amp;","&");
    return s;
}

}
struct SubtitleRenderer::Impl {
    struct Track { ASS_Track* ass=nullptr; bool authored=false, bitmap=false; std::string language; int w=0,h=0; size_t bytes=0; std::deque<Event> events; };
    std::mutex mutex;
    ASS_Library* library=nullptr;
    ASS_Renderer* renderer=nullptr;
    std::map<int,Track> tracks;
    std::string fonts;
    int last_id=-1;
    double last_time=-1;
    float last_lift=-1;
    int last_style=-1;
    uint64_t generation=0,last_generation=0;
    bool last_had_pixels=false;
    void clear() { for(auto& [id,t]:tracks) { (void)id; if(t.ass) ass_free_track(t.ass); } tracks.clear(); last_id=-1; ++generation; }
    ~Impl() { clear(); if(renderer) ass_renderer_done(renderer); if(library) ass_library_done(library); }
};
SubtitleRenderer::SubtitleRenderer():d_(new Impl) {}
SubtitleRenderer::~SubtitleRenderer()=default;
void SubtitleRenderer::configure(const std::string& fonts) {
    std::lock_guard<std::mutex> lock(d_->mutex);
    if(d_->library) return;
    d_->fonts=fonts;
    d_->library=ass_library_init(); if(!d_->library) return;
    ass_set_message_cb(d_->library,ass_message,nullptr);
    // Bundle fonts: no system provider or redistributed console fonts required.
    for(const char* name:{"Inter-Regular.ttf","Inter-SemiBold.ttf","NotoSans-Regular.ttf","NotoSans-Bold.ttf",
        "NotoSansHebrew-Regular.ttf","NotoSansHebrew-Bold.ttf",
        "NotoNaskhArabicUI-Regular.ttf","NotoNaskhArabicUI-Bold.ttf","NotoSansCJKsc-Regular.ttf","NotoSansThai-Regular.ttf"}) {
        std::ifstream f(fonts+"/"+name,std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(f)),{});
        if(!data.empty() && data.size()<24u<<20) ass_add_font(d_->library,name,data.data(),int(data.size()));
    }
    d_->renderer=ass_renderer_init(d_->library); if(!d_->renderer) return;
    ass_set_fonts(d_->renderer,(fonts+"/Inter-Regular.ttf").c_str(),"Inter",ASS_FONTPROVIDER_NONE,nullptr,1);
    ass_set_frame_size(d_->renderer,W,H);
    ass_set_shaper(d_->renderer,ASS_SHAPING_COMPLEX);
    ass_set_cache_limits(d_->renderer,6000,32);
}
void SubtitleRenderer::clear() { std::lock_guard<std::mutex> lock(d_->mutex); d_->clear(); }
void SubtitleRenderer::add_track(int id,const AVCodecParameters* par,const std::string& lang) {
    std::lock_guard<std::mutex> lock(d_->mutex);
    if(!d_->library || d_->tracks.count(id)) return;
    auto& t=d_->tracks[id]; t.w=par->width; t.h=par->height; t.language=lang;
    const auto* descriptor=avcodec_descriptor_get(par->codec_id);
    t.bitmap=descriptor && (descriptor->props & AV_CODEC_PROP_BITMAP_SUB);
    t.authored=par->codec_id==AV_CODEC_ID_ASS || par->codec_id==AV_CODEC_ID_SSA;
    if(t.bitmap) return;
    t.ass=t.authored ? ass_new_track(d_->library) : text_track(d_->library,lang); if(!t.ass) return;
    if(t.authored && par->extradata && par->extradata_size>0)
        ass_process_codec_private(t.ass,reinterpret_cast<char*>(par->extradata),par->extradata_size);
    // Replace missing generic default families only; preserve authored positioning/style.
    for(int i=0;i<t.ass->n_styles;++i) if(t.ass->styles[i].FontName &&
        (std::strcmp(family(lang),"Inter")!=0 || std::strcmp(t.ass->styles[i].FontName,"Arial")==0 || std::strcmp(t.ass->styles[i].FontName,"Roboto")==0)) {
        free(t.ass->styles[i].FontName); t.ass->styles[i].FontName=strdup(family(lang));
    }
    ++d_->generation;
}
void SubtitleRenderer::packet(int id,const AVPacket* p,double pts,double duration,const AVSubtitle* sub) {
    std::lock_guard<std::mutex> lock(d_->mutex);
    auto it=d_->tracks.find(id); if(it==d_->tracks.end() || pts<0) return;
    auto& t=it->second;
    if(t.ass) {
        if(t.ass->n_events>10000) ass_flush_events(t.ass);
        if(t.authored) ass_process_chunk(t.ass,reinterpret_cast<char*>(p->data),p->size,llround(pts*1000),llround(std::max(0.001,duration)*1000));
        else if(sub) for(unsigned i=0;i<sub->num_rects;++i) {
            auto* rect=sub->rects[i];
            std::string chunk=rect->ass ? rect->ass : "0,0,Default,,0,0,0,,"+std::string(rect->text?rect->text:"");
            const auto start=llround((pts+sub->start_display_time/1000.0)*1000);
            const auto dur=sub->end_display_time>sub->start_display_time ? sub->end_display_time-sub->start_display_time : llround(std::max(0.001,duration)*1000);
            ass_process_chunk(t.ass,chunk.data(),int(chunk.size()),start,dur);
        }
        ++d_->generation; return;
    }
    if(!sub) return;
    Event e; e.start=pts+sub->start_display_time/1000.0;
    e.end=sub->end_display_time>sub->start_display_time ? pts+sub->end_display_time/1000.0 : INFINITY;
    for(unsigned i=0;i<sub->num_rects;++i) {
        const auto* r=sub->rects[i];
        if(r->type!=SUBTITLE_BITMAP || !r->data[0] || !r->data[1] || r->w<=0 || r->h<=0 ||
           r->w>4096 || r->h>4096 || r->linesize[0]<r->w) continue;
        const size_t bytes=size_t(r->w)*r->h*4;
        if(bytes>kBitmapBudget-e.bytes) continue;
        Tile tile; tile.x=r->x; tile.y=r->y; tile.w=r->w; tile.h=r->h; tile.rgba.resize(bytes);
        const auto* palette=reinterpret_cast<const uint32_t*>(r->data[1]);
        for(int y=0;y<r->h;++y) for(int x=0;x<r->w;++x) {
            const unsigned index=r->data[0][size_t(y)*r->linesize[0]+x];
            const uint32_t c=index<unsigned(r->nb_colors) ? palette[index] : 0;
            auto* dst=tile.rgba.data()+(size_t(y)*r->w+x)*4;
            dst[0]=uint8_t(c>>16); dst[1]=uint8_t(c>>8); dst[2]=uint8_t(c); dst[3]=uint8_t(c>>24);
        }
        e.bytes+=bytes; e.tiles.push_back(std::move(tile));
    }
    // PGS clear packets close the previous display even when there are no rectangles.
    if(!t.events.empty() && t.events.back().end>e.start) t.events.back().end=e.start;
    if(!e.tiles.empty()) {
        while(!t.events.empty() && (t.events.size()>=120 || t.bytes+e.bytes>kBitmapBudget)) {
            t.bytes-=t.events.front().bytes; t.events.pop_front();
        }
        t.bytes+=e.bytes; t.events.push_back(std::move(e));
    }
    ++d_->generation;
}
bool SubtitleRenderer::external(int id,const std::string& utf8,const std::string& lang) {
    if(utf8.size()>8u<<20) return false;
    std::lock_guard<std::mutex> lock(d_->mutex);
    if(!d_->library) return false;
    const bool authored=utf8.find("[Script Info]")!=std::string::npos;
    auto* track=authored ? ass_read_memory(d_->library,const_cast<char*>(utf8.data()),utf8.size(),nullptr) : text_track(d_->library,lang);
    if(!track) return false;
    if(!authored) {
        const auto cues=parse_subtitle_file(utf8,false);
        if(cues.empty() || cues.size()>20000) { ass_free_track(track); return false; }
        int order=0;
        for(const auto& cue:cues) {
            std::string chunk=std::to_string(order++)+",0,Default,,0,0,0,,"+cue_ass(cue.rml);
            ass_process_chunk(track,chunk.data(),int(chunk.size()),llround(cue.start*1000),llround((cue.end-cue.start)*1000));
        }
    }
    if(authored && std::strcmp(family(lang),"Inter")!=0) for(int i=0;i<track->n_styles;++i) {
        free(track->styles[i].FontName); track->styles[i].FontName=strdup(family(lang));
    }
    auto& t=d_->tracks[id]; if(t.ass) ass_free_track(t.ass);
    t.ass=track; t.authored=authored; t.language=lang; ++d_->generation; return true;
}

void SubtitleRenderer::seek() {
    std::lock_guard<std::mutex> lock(d_->mutex);
    for(auto& [id,t]:d_->tracks) {
        if(id>=0 && t.ass) ass_flush_events(t.ass);
        t.events.clear(); t.bytes=0;
    }
    d_->last_id=-1; ++d_->generation;
}
SubtitleOverlay SubtitleRenderer::render(int id,double seconds,int vw,int vh,double sar,float lift,int font,int colour,int effect,int background,int size) {
    std::lock_guard<std::mutex> lock(d_->mutex);
    SubtitleOverlay out;
    auto it=d_->tracks.find(id);
    if(it==d_->tracks.end() || !d_->renderer) { out.changed=d_->last_id!=id || d_->last_had_pixels; d_->last_id=id; d_->last_had_pixels=false; return out; }
    auto& t=it->second; out.supported=t.ass || t.bitmap;
    const int style=font+colour*4+effect*16+background*64+size*8192;
    if(!out.supported) return out;
    if(id==d_->last_id && seconds==d_->last_time && lift==d_->last_lift && d_->generation==d_->last_generation && style==d_->last_style) return out;
    std::vector<Tile> tiles;
    int changed=0;
    if(t.ass) {
        if(!t.authored && (style!=d_->last_style || id!=d_->last_id)) for(int i=0;i<t.ass->n_styles;++i) {
            auto& st=t.ass->styles[i];
            const char* chosen=family(t.language);
            if(std::strcmp(chosen,"Inter")==0) chosen=font==2 ? "Montserrat" : "Inter";
            free(st.FontName); st.FontName=strdup(chosen);
            st.FontSize=size==0?30:size==2?48:38; st.Bold=font==1?1:0;
            st.PrimaryColour=colour==1?0xFFFF0000u:colour==2?0x00FFFF00u:0xFFFFFF00u;
            st.Outline=effect==1?2:0; st.Shadow=effect==2?2:0; st.BorderStyle=background?3:1;
            st.OutlineColour=background?unsigned(255-background*255/100):0; st.BackColour=0; st.MarginV=110;
            ass_set_fonts(d_->renderer,(d_->fonts+"/Inter-Regular.ttf").c_str(),chosen,ASS_FONTPROVIDER_NONE,nullptr,1);
        }
        ass_set_storage_size(d_->renderer,vw>0?vw:W,vh>0?vh:H);
        ASS_Image* images=ass_render_frame(d_->renderer,t.ass,llround(seconds*1000),&changed);
        if(!changed && id==d_->last_id && lift==d_->last_lift && style==d_->last_style) { d_->last_time=seconds; d_->last_generation=d_->generation; return out; }
        for(auto* p=images;p;p=p->next) {
            if(p->w<=0 || p->h<=0 || p->w>W*2 || p->h>H*2) continue;
            Tile tile; tile.x=p->dst_x; tile.y=p->dst_y-(p->dst_y>H/2?int(lift):0); tile.w=p->w; tile.h=p->h; tile.rgba.resize(size_t(p->w)*p->h*4);
            const auto c=p->color;
            for(int y=0;y<p->h;++y) for(int x=0;x<p->w;++x) {
                auto* dst=tile.rgba.data()+(size_t(y)*p->w+x)*4;
                dst[0]=uint8_t(c>>24); dst[1]=uint8_t(c>>16); dst[2]=uint8_t(c>>8);
                dst[3]=uint8_t(unsigned(p->bitmap[size_t(y)*p->stride+x])*(255-(c&255))/255);
            }
            tiles.push_back(std::move(tile));
        }
    } else {
        const auto rect=video_fit(vw,vh,sar,W,H);
        const int sw=t.w>0?t.w:vw, sh=t.h>0?t.h:vh;
        if(sw>0 && sh>0) for(const auto& e:t.events) if(seconds>=e.start && seconds<e.end) for(const auto& src:e.tiles) {
            Tile tile; tile.x=int(rect.x+src.x*rect.w/sw); tile.y=int(rect.y+src.y*rect.h/sh-lift);
            tile.w=std::clamp(int(std::round(src.w*rect.w/sw)),1,W); tile.h=std::clamp(int(std::round(src.h*rect.h/sh)),1,H);
            tile.rgba.resize(size_t(tile.w)*tile.h*4);
            for(int y=0;y<tile.h;++y) for(int x=0;x<tile.w;++x)
                memcpy(tile.rgba.data()+(size_t(y)*tile.w+x)*4,src.rgba.data()+(size_t(y*src.h/tile.h)*src.w+x*src.w/tile.w)*4,4);
            tiles.push_back(std::move(tile));
        }
    }
    int x0=W,y0=H,x1=0,y1=0;
    for(const auto& t:tiles) { x0=std::min(x0,std::max(0,t.x)); y0=std::min(y0,std::max(0,t.y)); x1=std::max(x1,std::min(W,t.x+t.w)); y1=std::max(y1,std::min(H,t.y+t.h)); }
    out.changed=true; out.x=x0; out.y=y0; out.w=std::max(0,x1-x0); out.h=std::max(0,y1-y0);
    if(out.w && out.h) {
        out.rgba.assign(size_t(out.w)*out.h*4,0);
        for(const auto& t:tiles) for(int y=std::max(y0,t.y);y<std::min(y1,t.y+t.h);++y)
            for(int x=std::max(x0,t.x);x<std::min(x1,t.x+t.w);++x)
                blend(out.rgba.data()+(size_t(y-y0)*out.w+x-x0)*4,t.rgba.data()+(size_t(y-t.y)*t.w+x-t.x)*4);
    }
    d_->last_style=style; d_->last_id=id; d_->last_time=seconds; d_->last_lift=lift; d_->last_generation=d_->generation; d_->last_had_pixels=!out.rgba.empty();
    return out;
}
