// SPDX-License-Identifier: GPL-3.0-or-later
#include "person.hpp"
#include "i18n.hpp"
#include <algorithm>
namespace sx {
using hui::gfx::Align;using hui::gfx::Rect;using hui::ui::text;
void PersonPage::refresh(){if(!content_)return;const auto& p=content_->data;
 focus_.movie=std::clamp(focus_.movie,0,std::max(0,int(p.movies.size())-1));focus_.series=std::clamp(focus_.series,0,std::max(0,int(p.series.size())-1));
 if(focus_.row==0&&p.movies.empty()&&!p.series.empty())focus_.row=1;
 if(focus_.row==1&&p.series.empty())focus_.row=0;
}
const people::Credit* PersonPage::selected()const{if(!content_||content_->loading||content_->resolving)return nullptr;const auto& rows=focus_.row?content_->data.series:content_->data.movies;int n=focus_.row?focus_.series:focus_.movie;return n>=0&&n<int(rows.size())?&rows[n]:nullptr;}
void PersonPage::update(const hui::InputFrame& in,float dt){if(!content_)return;refresh();
 if(!content_->loading&&!content_->resolving){if(in.nav==hui::Direction::up&&!content_->data.movies.empty())focus_.row=0;if(in.nav==hui::Direction::down&&!content_->data.series.empty())focus_.row=1;
 auto& n=focus_.row?focus_.series:focus_.movie;const auto& rows=focus_.row?content_->data.series:content_->data.movies;
 if(in.nav==hui::Direction::left)n=std::max(0,n-1);if(in.nav==hui::Direction::right)n=std::min(std::max(0,int(rows.size())-1),n+1);}
 for(int row=0;row<2;++row){int n=row?focus_.series:focus_.movie;const auto count=row?content_->data.series.size():content_->data.movies.size();scroll_[row].reveal(n*206.0f,n*206.0f+180,1160,12,std::max(0.0f,count*206.0f-26));scroll_[row].update(dt,18);}
}
void PersonPage::draw(hui::gfx::DrawList& list,hui::gfx::BackdropSpec& backdrop)const{
 backdrop.mode=hui::gfx::BackdropMode::none;list.rounded_rect({0,0,1920,1080},0,theme::ground);if(!content_)return;const auto& p=content_->data;
 text(list,env_.fonts.regular,i18n::tr("Person details"),100,90,24,theme::ink_3);
 text(list,env_.fonts.regular,i18n::tr("TMDB data and images"),1810,90,20,theme::ink_3,Align::right);
 text(list,env_.fonts.display,env_.fonts.display.font->fit(p.name,48,1680),100,156,48,theme::ink);
 Rect portrait{100,212,180,270};list.rounded_rect(portrait,18,theme::surface);if(content_->photo)list.image(content_->photo,portrait,hui::gfx::kFullUv,hui::gfx::Color::rgb(0xffffff));
 else text(list,env_.fonts.semibold,"?",portrait.cx(),portrait.cy(),60,theme::ink_3,Align::center);
 float y=236;const auto fact=[&](const char* label,const std::string& value,int lines=1){text(list,env_.fonts.regular,i18n::tr(label),312,y,20,theme::ink_3);hui::ui::paragraph(list,env_.fonts.semibold,value.empty()?i18n::tr("Unknown"):value,312,y+27,22,282,28,theme::ink,lines);y+=lines==2?90:62;};
 fact("Born",p.birthday);fact("Birthplace",p.birthplace,2);fact("Known for",i18n::tr(p.known_for));
 if(!p.deathday.empty())fact("Died",p.deathday);
 text(list,env_.fonts.semibold,i18n::tr("Biography"),100,552,27,theme::ink);
 hui::ui::paragraph(list,env_.fonts.regular,p.biography.empty()?i18n::tr("Biography unavailable."):p.biography,100,600,23,490,34,theme::ink_2,10);
 for(int row=0;row<2;++row){const auto& rows=row?p.series:p.movies;const auto& textures=row?content_->series:content_->movies;const float top=row?634:218;
  text(list,env_.fonts.semibold,i18n::tr(row?"TV shows":"Movies"),650,top-40,28,theme::ink);
  list.push_clip({638,top-10,1190,385});const int selected=row?focus_.series:focus_.movie;
  for(int i=std::max(0,selected-6);i<int(rows.size())&&i<selected+8;++i){float x=650+i*206.0f-scroll_[row].offset();Rect r{x,top,180,270};
   list.rounded_rect(r,16,theme::surface);if(i<int(textures.size())&&textures[i])list.image(textures[i],r,hui::gfx::kFullUv,hui::gfx::Color::rgb(0xffffff));else text(list,env_.fonts.regular,"?",r.cx(),r.cy(),44,theme::ink_3,Align::center);
   if(focus_.row==row&&i==selected)list.bordered_rect({x-5,top-5,190,280},18,hui::gfx::Color::rgb(0xffffff,0),3,theme::accent_soft);
   hui::ui::paragraph(list,env_.fonts.semibold,rows[i].name,x,top+304,22,180,27,theme::ink,2);if(env_.fonts.semibold.font->wrap(rows[i].name,22,180).size()==1)text(list,env_.fonts.regular,rows[i].year,x,top+340,19,theme::ink_3);
  }list.pop_clip();if(rows.empty())text(list,env_.fonts.regular,i18n::tr(content_->loading?"Loading...":"No credits available."),650,top+120,24,theme::ink_2);
 }
 if(!content_->status.empty())text(list,env_.fonts.regular,env_.fonts.regular.font->fit(content_->status,22,1700),100,1008,22,theme::accent_soft);
 hui::ui::HintLayout layout;layout.size=36;layout.text_size=22;layout.cy=1050;layout.item_gap=24;layout.font=&env_.fonts.regular;
 const hui::ui::Hint hints[]={{hui::ui::Button::dpad,i18n::tr("Browse")},{hui::ui::Button::cross,i18n::tr("Open title")},
    {hui::ui::Button::triangle,i18n::tr("Retry")},{hui::ui::Button::circle,i18n::tr("Back")}};
 hui::ui::draw_hints(list,env_.fonts,hui::ui::GlyphStyle::dark(),hints,4,theme::kRight,true,layout);
}
}
