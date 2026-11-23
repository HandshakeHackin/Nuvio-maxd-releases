// SPDX-License-Identifier: GPL-3.0-or-later
#include "search_keyboard_draw.hpp"
#include "i18n.hpp"
namespace sx {
void draw_search_keyboard(hui::gfx::DrawList& list,const Env& env,const search_keyboard::State& state) {
 using hui::gfx::Color;using hui::gfx::Align;using hui::ui::text;
 list.rounded_rect({0,0,1920,1080},0,Color::rgb(0x03040b,.94f));
 text(list,env.fonts.semibold,state.title,80,88,36,theme::ink);
 text(list,env.fonts.regular,search_keyboard::languages[state.language].name,1840,88,28,theme::accent_soft,Align::right);
 list.bordered_rect({80,124,1760,88},18,theme::surface,2,theme::accent_soft);
 text(list,env.fonts.semibold,env.fonts.semibold.font->fit(state.text.empty()?i18n::tr("Type a title"):state.text,34,1700),110,180,34,state.text.empty()?theme::ink_3:theme::ink);
 const auto rows=state.rows();
 auto key=[&](const std::string& label,int row,int col,float width){
  const float x=80+col*(width+10),y=260+row*78;
  const bool selected=state.suggestion<0&&state.row==row&&state.col==col;
  list.bordered_rect({x,y,width,64},12,selected?theme::accent:theme::surface_hi,selected?3:1,selected?theme::ink:theme::hairline);
  text(list,env.fonts.semibold,env.fonts.semibold.font->fit(label,28,width-12),x+width/2,y+42,28,theme::ink,Align::center);
 };
 for(int r=0;r<int(rows.size());++r)for(int c=0;c<int(rows[r].size());++c)key(search_keyboard::utf8(rows[r][c]),r,c,84);
 constexpr const char* actions[]={"Delete","Space","Clear","Language","PS5 keyboard","Search"};
 for(int n=0;n<6;++n)key(i18n::tr(actions[n]),int(rows.size()),n,178);
 text(list,env.fonts.semibold,i18n::tr("Title suggestions"),1250,274,30,theme::ink);
 text(list,env.fonts.regular,env.fonts.regular.font->fit(state.status,22,580),1250,312,22,theme::ink_3);
 for(int n=0;n<int(state.suggestions.size());++n){
  const auto& s=state.suggestions[n];const float y=340+n*82;
  list.bordered_rect({1230,y,610,72},12,state.suggestion==n?theme::accent:theme::surface, state.suggestion==n?3:0,theme::ink);
  text(list,env.fonts.semibold,env.fonts.semibold.font->fit(s.name,27,565),1250,y+31,27,theme::ink);
  text(list,env.fonts.regular,env.fonts.regular.font->fit(i18n::media_type(s.type)+(s.year.empty()?"":" · "+s.year),20,565),1250,y+58,20,theme::ink_2);
 }
 text(list,env.fonts.regular,i18n::tr("L1 / R1: language · Square: shift · Options: PS5 keyboard"),80,956,24,theme::ink_2);
 text(list,env.fonts.regular,i18n::tr("Cross: choose · Triangle: search · Circle: cancel"),80,998,24,theme::ink_2);
 if(state.language==8||state.language==10)text(list,env.fonts.regular,i18n::tr("Use the PS5 keyboard for kanji or Chinese characters."),80,1038,22,theme::ink_3);
}
}
