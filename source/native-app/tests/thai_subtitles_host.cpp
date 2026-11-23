#include "subtitle_renderer.h"
#include "subtitle_fonts.hpp"
#include "util.h"
extern "C" {
#include <ass/ass.h>
#include <libavcodec/avcodec.h>
}
#include <hb.h>
#include <hb-ot.h>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct FontLog {
 char path[48]="/tmp/nuvio-thai-font-XXXXXX";
 FontLog(){int fd=mkstemp(path);require(fd>=0,"log unavailable");close(fd);log_open(path);}
 ~FontLog(){log_open("");std::remove(path);}
 std::string text()const{std::ifstream f(path);return {(std::istreambuf_iterator<char>(f)),{}};}
 void check()const{const auto s=text();require(s.find("not found")==std::string::npos&&s.find("failed to find any fallback")==std::string::npos,"Thai glyphs are missing (boxes can have pixels too)");}
};
static std::string ass(const std::string& text){return "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\nStyle: Default,Arial,40,&H00FFFFFF,&H00FFFFFF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,2,0,2,10,10,10,1\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\nDialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,"+text+"\n";}
static const std::string thai="ภาษาไทย ทดสอบ น้ำ";
static void message(int level,const char* fmt,va_list args,void* data){if(level>2)return;char b[1024];vsnprintf(b,sizeof(b),fmt,args);*static_cast<std::string*>(data)+=std::string(b)+"\n";}
static std::vector<std::array<int,9>> shape(const std::string& path,const std::string& text) {
 auto* blob=hb_blob_create_from_file(path.c_str());require(hb_blob_get_length(blob)>0,"font unreadable");
 auto* face=hb_face_create(blob,0);auto* font=hb_font_create(face);hb_ot_font_set_funcs(font);hb_font_set_scale(font,1000,1000);
 auto* buffer=hb_buffer_create();hb_buffer_add_utf8(buffer,text.data(),int(text.size()),0,int(text.size()));hb_buffer_guess_segment_properties(buffer);hb_buffer_set_language(buffer,hb_language_from_string("th",-1));hb_shape(font,buffer,nullptr,0);
 unsigned count=0;const auto* glyphs=hb_buffer_get_glyph_infos(buffer,&count);const auto* positions=hb_buffer_get_glyph_positions(buffer,nullptr);
 std::vector<std::array<int,9>> out;
 for(unsigned i=0;i<count;++i) {
  require(glyphs[i].codepoint!=0,"shaping produced .notdef");hb_glyph_extents_t e{};require(hb_font_get_glyph_extents(font,glyphs[i].codepoint,&e),"glyph extents absent");
  out.push_back({int(glyphs[i].cluster),positions[i].x_advance,positions[i].y_advance,positions[i].x_offset,positions[i].y_offset,e.x_bearing,e.y_bearing,e.width,e.height});
 }
 hb_buffer_destroy(buffer);hb_font_destroy(font);hb_face_destroy(face);hb_blob_destroy(blob);return out;
}
int main(int argc,char** argv) {
 if(argc==3&&std::string(argv[2])=="--cp874-vector") {std::string bytes;for(int n=0;n<256;++n)bytes+=char(n);const auto out=to_utf8(bytes,"tha");std::cout.write(out.data(),out.size());return 0;}
 if(argc!=2)return 2;int failed=0,passed=0;
 const auto test=[&](const std::string& name,const auto& fn){try{fn();++passed;std::cout<<"PASS: "<<name<<"\n";}catch(const std::exception& e){++failed;std::cerr<<"FAIL: "<<name<<": "<<e.what()<<"\n";}};
 for(const auto* language:{"und","eng","tha","th-TH"})test(std::string("Thai UTF-8 external SRT, tag=")+language,[&]{
  FontLog log;SubtitleRenderer r;r.configure(argv[1]);require(r.external(-2,"1\n00:00:01,000 --> 00:00:03,000\n"+thai+"\n",language),"SRT rejected");
  for(int font=0;font<3;++font){auto image=r.render(-2,2,1920,1080,1,0,font);require(image.supported&&image.w>50&&image.h>10,"empty overlay");}log.check();
 });
 test("untagged embedded SubRip with mixed Latin and Thai",[&]{
  FontLog log;SubtitleRenderer r;r.configure(argv[1]);auto* p=avcodec_parameters_alloc();p->codec_id=AV_CODEC_ID_SUBRIP;r.add_track(42,p,"und");avcodec_parameters_free(&p);
  std::string text="0,0,Default,,0,0,0,,Hello "+thai;AVSubtitleRect rect{};rect.type=SUBTITLE_ASS;rect.ass=text.data();AVSubtitleRect* rows[]={&rect};AVSubtitle sub{};sub.num_rects=1;sub.rects=rows;sub.end_display_time=2000;AVPacket packet{};r.packet(42,&packet,1,2,&sub);
  auto image=r.render(42,2,1920,1080,1,0);require(image.w>50&&image.h>10,"embedded overlay empty");log.check();
 });
 test("authored ASS generic/inline fonts retain positioning",[&]{
  FontLog log;SubtitleRenderer r;r.configure(argv[1]);require(r.external(-2,ass("{\\pos(400,200)\\fnArial}"+thai+" English Ελληνικά Русский"),"und"),"ASS rejected");
  auto image=r.render(-2,2,1920,1080,1,0);require(image.w>50&&image.y<250,"positioned ASS lost");log.check();
 });
 test("VLC font options render generic ASS without system font services",[&]{
  auto options=subtitle_fonts::vlc_options();const std::string prefix=":freetype-font=/app0/fonts/";require(options[0].starts_with(prefix),"unexpected font option");
  const auto path=std::string(argv[1])+"/"+options[0].substr(prefix.size());require(options[2]==":ssa-fontsdir=/app0/fonts","bundled ASS font directory missing");
  auto* library=ass_library_init();require(library,"libass init failed");std::string messages;ass_set_message_cb(library,message,&messages);ass_set_fonts_dir(library,argv[1]);auto* renderer=ass_renderer_init(library);require(renderer,"renderer init failed");
  ass_set_fonts(renderer,path.c_str(),"Arial",ASS_FONTPROVIDER_NONE,nullptr,1);ass_set_frame_size(renderer,1920,1080);ass_set_shaper(renderer,ASS_SHAPING_COMPLEX);
  bool pixels=false;
  for(const auto* family:{"Arial","Inter","Noto Sans","Noto Sans CJK SC"}) {
   auto text=ass("{\\fn"+std::string(family)+"}"+thai+" English Ελληνικά Русский");auto* track=ass_read_memory(library,text.data(),text.size(),nullptr);int changed=0;auto* image=ass_render_frame(renderer,track,2000,&changed);
   for(auto* row=image;row;row=row->next)if(row->w>0&&row->h>0)pixels=true;
   ass_free_track(track);
  }
  ass_renderer_done(renderer);ass_library_done(library);require(pixels,"no ASS pixels");require(messages.find("not found")==std::string::npos,"VLC-configured font lacks glyphs");
 });
 test("Thai shaping and combining-mark placement match the original face",[&]{
  for(const auto* text:{"ภาษาไทย","น้ำ","เก่ง","กำลังทดสอบ","กิ๊ กุ้ง"})require(shape(std::string(argv[1])+"/NotoSansThai-Regular.ttf",text)==shape(subtitle_fonts::fallback_path(argv[1]),text),"Thai shaping/layout changed during font preparation");
 });
 const std::string cp874="\xc0\xd2\xc9\xd2\xe4\xb7\xc2\x20\xb7\xb4\xca\xcd\xba\x20\xb9\xe9\xd3";
 for(const auto* hint:{"th","tha","thai","th-TH","th_TH"})test(std::string("legacy CP874/TIS-620 decoding, hint=")+hint,[&]{require(to_utf8(cp874,hint)==thai,"Thai decoded through wrong code page");});
 test("UTF-8 and BOM/UTF-16 Thai stay intact",[&]{
  require(to_utf8(thai,"tha")==thai&&to_utf8("\xef\xbb\xbf"+thai,"eng")==thai,"UTF-8 changed");
  require(to_utf8(std::string("\xff\xfe\x20\x0e\x32\x0e",6),"tha")=="ภา","UTF-16LE changed");
  require(to_utf8(std::string("\xfe\xff\x0e\x20\x0e\x32",6),"tha")=="ภา","UTF-16BE changed");
 });
 test("CP874 punctuation and undefined bytes are explicit",[&]{
  require(to_utf8("\x80\x85\x91\x92\x93\x94\x95\x96\x97\xa0","tha")=="€…‘’“”•–— ","punctuation corrupted");
  require(to_utf8("\x81\xdb\xfc","tha")=="���","undefined bytes were guessed");
 });
 test("non-Thai fallback decoding remains unchanged",[&]{require(to_utf8("caf\xe9","eng")=="café","Western decoding changed");});
 std::cout<<"Thai subtitle regressions: "<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
