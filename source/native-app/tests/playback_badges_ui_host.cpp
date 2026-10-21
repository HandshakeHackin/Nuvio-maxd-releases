#include "player.hpp"
#include <cassert>
#include <iostream>
struct Glyphs: hui::gfx::GlyphSource {
 bool glyph(uint32_t,hui::gfx::GlyphBitmap& b) override {b.advance=8;b.w=6;b.h=9;b.offset_y=-8;b.pixels.assign(54,255);return true;}
};
int main() {
 Glyphs glyphs;hui::gfx::Font font;font.init_dynamic(&glyphs,10,2,8,-2,0,256);
 hui::ui::Fonts fonts;fonts.regular={&font,1};fonts.semibold={&font,1};fonts.display={&font,1};
 sx::Icons icons;sx::Env env{fonts,icons};sx::PlayerUi ui(env);
 ui.state.title="A title";ui.state.subtitle="S1 E18 · An episode";
 ui.state.badges={"1080p","HDR10","5.1","WEB-DL","HEVC"};ui.snap();
 const auto check=[&](int expected,float top) {
  hui::gfx::DrawList list;ui.draw(list);int count=0;float right=0;
  for(const auto& item:list.instances())if(item.params[1]==1.5f && item.rect[3]==34.0f && item.rect[1]<320.0f) {
   assert(item.rect[0]>=110 && item.rect[0]>=right && item.rect[0]+item.rect[2]<=1810);
   assert(item.rect[1]>=top && item.rect[1]+item.rect[3]<240);
   right=item.rect[0]+item.rect[2];++count;
  }
  if(expected>=0)assert(count==expected);else assert(count>0 && count<100);
 };
 check(5,164);ui.state.subtitle.clear();check(5,120);
 ui.state.badges.assign(100,"Dolby Vision source");check(-1,120);
 ui.state.controls=false;ui.snap();check(0,120);
 std::cout<<"Playback badge rendering: outlined pills, title/episode spacing, row bounds, overflow and control visibility passed\n";
}
