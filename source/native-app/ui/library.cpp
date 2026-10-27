#include "library.hpp"

#include <algorithm>

namespace sx
{

using hui::Direction;
using hui::gfx::Align;
using hui::ui::text;
namespace tween = hui::tween;

namespace
{

GridLayout layout()
{
    GridLayout l;
    l.y = 232.0f;
    l.pitch_y = 432.0f;
    l.view_top = 196.0f;
    return l;
}

} // namespace

void Library::set_content(const LibraryContent *content)
{
    content_ = content;
    in_filters_ = false;
    filter_ = 0;
    auto l=layout();if(page_==Page::iptv){l.y+=90;l.view_top+=90;}
    grid_.set_items(content ? &content->items : nullptr, l);
}

void Library::update(const hui::InputFrame &input, float dt)
{
    tick(dt);
    if (!content_)
        return;
    if (!rail_input(input))
    {
        const int filters = static_cast<int>(content_->filters.size());
        if (in_filters_)
        {
            if (input.nav == Direction::left)
            {
                if (filter_ == 0)
                    to_rail();
                else
                    --filter_;
            }
            else if (input.nav == Direction::right)
            {
                filter_ = std::min(filter_ + 1, std::max(0, filters - 1));
            }
            else if (input.nav == Direction::down && !content_->items.empty())
            {
                in_filters_ = false;
            }
        }
        else
        {
            const PosterGrid::Edge edge = grid_.move(input.nav);
            if (edge == PosterGrid::Edge::left)
                to_rail();
            else if (edge == PosterGrid::Edge::up && filters > 0)
                in_filters_ = true;
        }
    }
    grid_.update(dt, !in_filters_ && !rail_.focused());
}

void Library::draw(hui::gfx::DrawList &list, hui::gfx::BackdropSpec &backdrop) const
{
    apply_page_backdrop(backdrop, clock_);
    if (!content_)
        return;
    const float in = tween::stagger(age_, 0, 0.0f, 0.45f);
    list.push_opacity(in);
    float x = theme::kLeft;
    const float shift=page_==Page::iptv?90.0f:0.0f;
    if(page_==Page::iptv){
        text(list,env_.fonts.semibold,env_.fonts.semibold.font->fit(content_->heading,32,1500),x,144,32,theme::ink);
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(content_->notice,20,1500),x,178,20,theme::ink_2);
    }
    for (std::size_t i = 0; i < content_->filters.size(); ++i)
        x += draw_filter_chip(list, env_, content_->filters[i], x, 148.0f+shift,
                              in_filters_ && static_cast<int>(i) == filter_ && !rail_.focused()) +
             16.0f;
    text(list, env_.fonts.regular, content_->count, theme::kRight - 6.0f, 157.0f+shift, theme::kBody, theme::ink_3,
         Align::right);
    list.pop_opacity();

    grid_.draw(list, env_, age_, true, !in_filters_ && !rail_.focused());
    if (content_->items.empty())
        draw_status(list, env_, content_->status, 400.0f);

    fade_down(list, theme::kRailWidth, 0.0f, theme::kScreenW, 118.0f, 0.62f);
    fade_up(list, theme::kRailWidth, 990.0f, theme::kScreenW, 90.0f, 0.94f);
    draw_topbar(list, env_, false, age_,nullptr,0,page_==Page::iptv?"Search this IPTV category":nullptr);
    rail_.draw(list, env_, age_);
}

} // namespace sx
