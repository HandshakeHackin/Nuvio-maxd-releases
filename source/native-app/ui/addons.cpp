#include "../ui/i18n.hpp"
#include "addons.hpp"

#include <algorithm>
#include <ctime>
#include "../src/guide_grid.hpp"

namespace sx
{

using hui::Direction;
using hui::gfx::Align;
using hui::gfx::Color;
using hui::gfx::Rect;
using hui::ui::text;
namespace tween = hui::tween;

namespace
{

constexpr int kCols = 3;
constexpr float kTileW = 540.0f;
constexpr float kTileH = 248.0f;
constexpr float kGapX = 25.0f;
constexpr float kGapY = 24.0f;
constexpr float kTop = 214.0f;

} // namespace

Rect Addons::tile_rect(int index, bool final_position) const
{
    if(content_ && content_->guide_grid){
        const float scroll=final_position?scroll_.position.target:scroll_.offset();
        return {theme::kLeft,260.0f+index*86.0f-scroll,1670.0f,78.0f};
    }
    const int row = index / kCols;
    const int col = index % kCols;
    const float scroll = final_position ? scroll_.position.target : scroll_.offset();
    return {theme::kLeft + static_cast<float>(col) * (kTileW + kGapX),
            kTop + static_cast<float>(row) * (kTileH + kGapY) - scroll, kTileW, kTileH};
}

void Addons::refresh() {
    if(!content_)return;
    if(!selected_identity_.empty())for(size_t i=0;i<content_->items.size();++i)if(content_->items[i].identity==selected_identity_){index_=int(i);return;}
    index_=std::clamp(index_,0,std::max(0,int(content_->items.size())-1));
}
void Addons::set_content(const AddonsContent *content)
{
    content_ = content;
    index_ = 0;
    selected_identity_=content&&!content->items.empty()?content->items.front().identity:"";
    scroll_.position.snap(0.0f);
    ring_.snap(tile_rect(0, true));
    focus_on_.snap(1.0f);
}

void Addons::update(const hui::InputFrame &input, float dt)
{
    tick(dt);
    if (!content_ || content_->items.empty())
    {
        if (!rail_.focused())
            to_rail();
        if (input.nav != Direction::right)
            rail_input(input);
        focus_on_.target = 0.0f;
        focus_on_.update(dt, 16.0f);
        return;
    }
    const int count = static_cast<int>(content_->items.size());
    if(content_->guide_grid){
        if(!rail_input(input)){
            if(input.nav==Direction::up)index_=std::max(0,index_-1);
            else if(input.nav==Direction::down)index_=std::min(count-1,index_+1);
            else if(input.nav==Direction::right)guide_slot_=std::min(44,guide_slot_+1);
            else if(input.nav==Direction::left){if(guide_slot_>0)--guide_slot_;else to_rail();}
        }
        selected_identity_=content_->items[size_t(index_)].identity;
        scroll_.reveal(index_*86.0f,index_*86.0f+78,660,8);scroll_.update(dt,18);
        ring_.target(tile_rect(index_,true));ring_.update(dt,20);
        focus_on_.target=rail_.focused()?0.0f:1.0f;focus_on_.update(dt,16);
        return;
    }
    if (!rail_input(input))
    {
        const int col = index_ % kCols;
        switch (input.nav)
        {
        case Direction::left:
            if (col == 0)
                to_rail();
            else
                --index_;
            break;
        case Direction::right:
            if (col + 1 < kCols && index_ + 1 < count)
                ++index_;
            break;
        case Direction::up:
            if (index_ >= kCols)
                index_ -= kCols;
            break;
        case Direction::down:
            if (index_ + kCols < count)
                index_ += kCols;
            else if ((count - 1) / kCols > index_ / kCols)
                index_ = count - 1;
            break;
        default:
            break;
        }
    }
    if(index_>=0&&size_t(index_)<content_->items.size())selected_identity_=content_->items[size_t(index_)].identity;
    const float top = static_cast<float>(index_ / kCols) * (kTileH + kGapY);
    scroll_.reveal(top, top + kTileH, 1080.0f - kTop - 30.0f, 12.0f);
    scroll_.update(dt, 18.0f);
    ring_.target(tile_rect(index_, true));
    ring_.update(dt, 20.0f);
    focus_on_.target = rail_.focused() ? 0.0f : 1.0f;
    focus_on_.update(dt, 16.0f);
}


void Addons::draw_guide(hui::gfx::DrawList& list) const {
    const auto clock=[](int64_t epoch){
        const time_t t=time_t(epoch);tm parts{};localtime_r(&t,&parts);char out[48];
        std::strftime(out,sizeof(out),"%a %H:%M",&parts);return std::string(out);
    };
    const int64_t start=content_->guide_now/1800*1800+int64_t(guide_slot_)*1800;
    constexpr float channel_width=330,time_width=1340;
    const float x=theme::kLeft+channel_width;
    text(list,env_.fonts.semibold,sx::i18n::tr("Live TV programme guide"),theme::kLeft,158,32,theme::ink);
    text(list,env_.fonts.regular,sx::i18n::tr("Left/Right: time · Up/Down: channel · X: watch live · Square: favorite"),theme::kLeft,198,23,theme::ink_2);
    for(int i=0;i<4;++i)text(list,env_.fonts.semibold,clock(start+i*1800),x+i*time_width/4,242,22,theme::ink_2);
    list.push_clip({theme::kLeft,254,1670,666});
    for(size_t i=0;i<content_->items.size();++i){
        const auto row=tile_rect(int(i),false);if(row.y>920 || row.y+row.h<254)continue;
        const auto& channel=content_->items[i];const bool selected=int(i)==index_&&!rail_.focused();
        list.rounded_rect(row,10,selected?theme::surface_hi:theme::surface);
        text(list,env_.fonts.semibold,env_.fonts.semibold.font->fit(channel.name,24,channel_width-26),row.x+12,row.cy()+8,24,theme::ink);
        list.push_clip({x,row.y,time_width,row.h});
        bool available=false;
        for(const auto& programme:channel.programmes){
            const auto span=guide_grid::clip(programme.start,programme.end,start,7200);
            if(!span.visible)continue;
            available=true;const Rect cell{x+float(span.left)*time_width,row.y+3,std::max(1.0f,float(span.width)*time_width-3),row.h-6};
            const bool current=programme.start<=content_->guide_now&&programme.end>content_->guide_now;
            list.bordered_rect(cell,8,current?theme::accent.with_alpha(.24f):Color::rgb(0xffffff,.06f),1,theme::hairline);
            list.push_clip(cell);
            text(list,env_.fonts.regular,env_.fonts.regular.font->fit(programme.title,23,std::max(0.0f,cell.w-16)),cell.x+8,cell.cy()+8,23,theme::ink);
            list.pop_clip();
        }
        if(!available)text(list,env_.fonts.regular,i==0?sx::i18n::tr("X: sources and filters · Options: layout and guide settings"):sx::i18n::tr("No listings supplied for this time — channel still playable"),x+12,row.cy()+8,22,theme::ink_3);
        list.pop_clip();
        if(selected)list.bordered_rect(row,10,Color::rgb(0xffffff,0),3,theme::ink);
    }
    const auto now=content_->guide_now;
    if(now>=start&&now<start+7200){const float at=x+float(now-start)/7200*time_width;list.line(at,254,at,920,2,theme::accent_soft);}
    list.pop_clip();
    if(index_>0&&size_t(index_)<content_->items.size()){
        std::string selected=sx::i18n::tr("X tunes the live channel; future programmes are listings only.");
        for(const auto& programme:content_->items[size_t(index_)].programmes)
            if(programme.start<=start&&programme.end>start){selected=clock(programme.start)+" – "+clock(programme.end)+" · "+programme.title;break;}
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(selected,24,1660),theme::kLeft,965,24,theme::ink_2);
    }
    text(list,env_.fonts.regular,sx::i18n::tr("Options: sources, groups, guide layout and channel pages · L1: groups · R1: All / Favorites / Recent"),theme::kLeft,1010,21,theme::ink_3);
    draw_topbar(list,env_,false,age_);rail_.draw(list,env_,age_);
}

void Addons::draw_tile(hui::gfx::DrawList &list, const Addon &a, const Rect &r, float enter, bool focused) const
{
    list.push_opacity(enter);
    list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, 16.0f * (1.0f - enter));
    list.rounded_rect(r, 28.0f, focused ? theme::surface_hi : theme::surface);
    if(page_==Page::sports) {
        hui::ui::paragraph(list,env_.fonts.semibold,a.name,r.x+28,r.y+46,28,r.w-56,34,theme::ink,2);
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(a.version,21,r.w-56),r.x+28,r.y+112,21,theme::ink_3);
        if(!a.types.empty())draw_tag(list,env_,a.types,r.x+28,r.y+146,theme::accent.with_alpha(.22f),theme::accent_soft);
        hui::ui::paragraph(list,env_.fonts.regular,a.description,r.x+28,r.y+192,21,r.w-56,28,theme::ink_2,2);
        list.pop_transform();list.pop_opacity();return;
    }
    // The addon's logo, or its initial on its own colour.
    const Rect icon{r.x + 30.0f, r.y + 30.0f, 88.0f, 88.0f};
    if (a.logo != 0)
    {
        list.image(a.logo, icon, hui::gfx::kFullUv, Color::rgb(0xffffff), 20.0f);
    }
    else
    {
        list.gradient_rect(icon, 20.0f, hui::gfx::mix(Color::rgb(a.colour), Color::rgb(0xffffff), 0.18f),
                           hui::gfx::mix(Color::rgb(a.colour), Color::rgb(0x000000), 0.35f));
        text(list, env_.fonts.semibold, a.initial, icon.cx(), icon.cy() + 15.0f, 44.0f, Color::rgb(0xffffff, 0.95f),
             Align::center);
    }
    const float x = icon.x + icon.w + 24.0f;
    const float w = r.x + r.w - x - 28.0f;
    text(list, env_.fonts.semibold, env_.fonts.semibold.font->fit(a.name, 30.0f, w), x, r.y + 66.0f, 30.0f, theme::ink);
    // The version gives way to the "Local" tag, which keeps its place.
    const float local_w = a.local ? env_.fonts.semibold.measure(sx::i18n::tr("Local"), 20.0f) + 28.0f + 16.0f : 0.0f;
    const std::string version = env_.fonts.regular.font->fit(a.version, 22.0f, w - local_w);
    text(list, env_.fonts.regular, version, x, r.y + 102.0f, 22.0f, theme::ink_3);
    if (a.local)
        draw_tag(list, env_, sx::i18n::tr("Local"), x + env_.fonts.regular.measure(version, 22.0f) + 16.0f, r.y + 94.0f,
                 theme::accent.with_alpha(0.28f), theme::accent_soft);
    // What it provides. Tags that would run past the tile are counted instead: "+2".
    std::vector<std::string> kinds;
    for (std::size_t start = 0; start < a.types.size();)
    {
        std::size_t end = a.types.find(' ', start);
        if (end == std::string::npos)
            end = a.types.size();
        if (end > start)
            kinds.push_back(a.types.substr(start, end - start));
        start = end + 1;
    }
    const float tag_limit = r.x + r.w - 30.0f;
    float tx = r.x + 30.0f;
    for (std::size_t i = 0; i < kinds.size(); ++i)
    {
        const std::size_t left = kinds.size() - i;
        const auto width = [&](const std::string &s) { return env_.fonts.semibold.measure(s, 20.0f) + 28.0f; };
        const std::string more = "+" + std::to_string(left);
        // Room for this tag, and for "+n" after it if others remain.
        const float need = width(kinds[i]) + (left > 1 ? 10.0f + width("+" + std::to_string(left - 1)) : 0.0f);
        if (tx + need > tag_limit && tx + width(more) <= tag_limit)
        {
            draw_tag(list, env_, more, tx, r.y + 146.0f, Color::rgb(0xffffff, 0.09f), theme::ink_3);
            break;
        }
        tx += draw_tag(list, env_, kinds[i], tx, r.y + 146.0f, Color::rgb(0xffffff, 0.09f), theme::ink_2) + 10.0f;
    }
    if(page_==Page::livetv && !a.programme_now.empty()) {
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(a.programme_now,21,r.w-60),r.x+30,r.y+196,21,theme::ink);
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(a.programme_next,21,r.w-60),r.x+30,r.y+225,21,theme::ink_2);
    } else
        hui::ui::paragraph(list, env_.fonts.regular, a.description, r.x + 30.0f, r.y + 196.0f, 22.0f, r.w - 60.0f, 29.0f,
                           theme::ink_2, 2);
    list.pop_transform();
    list.pop_opacity();
}

void Addons::draw(hui::gfx::DrawList &list, hui::gfx::BackdropSpec &backdrop) const
{
    apply_page_backdrop(backdrop, clock_);
    if (!content_)
        return;
    if(content_->guide_grid){draw_guide(list);return;}
    const float in = tween::stagger(age_, 0, 0.0f, 0.45f);
    list.push_opacity(in);
    char count[96];
    if(page_ == Page::livetv)std::snprintf(count,sizeof(count),"Live TV · %zu channels shown",content_->items.empty()?0:content_->items.size()-1);
    else if(page_ == Page::sports)std::snprintf(count,sizeof(count),"Sports Hub · %zu events shown",content_->items.size()>3?content_->items.size()-3:0);
    else if(page_ == Page::torrents)std::snprintf(count,sizeof(count),"Torrents · %zu items",content_->items.size());
    else std::snprintf(count, sizeof(count), "%zu addons installed", content_->items.size());
    text(list, env_.fonts.semibold, count, theme::kLeft, 158.0f, theme::kSubtitle, theme::ink_2);
    list.pop_opacity();

    list.push_clip({theme::kLeft - 44.0f, 192.0f, theme::kScreenW, 1080.0f - 192.0f});
    const Rect ring = ring_.value();
    if (focus_on_.value > 0.01f)
    {
        list.push_opacity(focus_on_.value);
        list.glow(ring.inset(-3.0f), 30.0f, 30.0f, theme::accent.with_alpha(0.38f));
        list.pop_opacity();
    }
    for (int i = 0; i < static_cast<int>(content_->items.size()); ++i)
    {
        const Rect r = tile_rect(i, false);
        if (r.y > 1080.0f || r.y + r.h < 180.0f)
            continue;
        const float enter = tween::stagger(age_, (i / kCols) * 2 + i % kCols, 0.045f, 0.45f);
        draw_tile(list, content_->items[i], r, enter, i == index_ && !rail_.focused());
    }
    if (focus_on_.value > 0.01f)
    {
        list.push_opacity(focus_on_.value);
        list.bordered_rect(ring.inset(-3.0f), 30.0f, Color::rgb(0xffffff, 0.0f), 4.0f, Color::rgb(0xffffff));
        list.pop_opacity();
    }
    list.pop_clip();

    fade_down(list, theme::kRailWidth, 0.0f, theme::kScreenW, 118.0f, 0.62f);
    fade_up(list, theme::kRailWidth, 1000.0f, theme::kScreenW, 80.0f, 0.94f);
    draw_topbar(list, env_, false, age_,nullptr,0,page_==Page::sports?sx::i18n::tr("Search Sports Hub"):nullptr);
    // The only action here, where it can be seen.
    if(page_ == Page::livetv)draw_chip(list,env_,hui::ui::Button::square,sx::i18n::tr("Favorite"),theme::kRight-252.0f,148.0f);
    else if(page_ == Page::torrents)draw_chip(list,env_,hui::ui::Button::square,sx::i18n::tr("Files / Options"),theme::kRight-268.0f,148.0f);
    else if(page_ == Page::sports)draw_chip(list,env_,hui::ui::Button::square,sx::i18n::tr("Selections"),theme::kRight-252.0f,148.0f);
    else draw_chip(list, env_, hui::ui::Button::square, sx::i18n::tr("Reload addons"), theme::kRight - 252.0f, 148.0f);
    rail_.draw(list, env_, age_);
}

} // namespace sx
