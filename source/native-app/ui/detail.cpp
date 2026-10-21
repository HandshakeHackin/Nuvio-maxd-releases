#include "detail.hpp"
#include "stream_badges.hpp"

#include <algorithm>
#include <cstdio>

namespace sx
{

using hui::Action;
using hui::Direction;
using hui::gfx::Align;
using hui::gfx::Color;
using hui::gfx::kFullUv;
using hui::gfx::Rect;
using hui::ui::text;
namespace tween = hui::tween;

namespace
{

constexpr Rect kPanel{1040.0f, 84.0f, 820.0f, 912.0f};
constexpr float kListTop = 610.0f;
constexpr float kEpisodeH = 340.0f;
constexpr float kEpisodeW = 400.0f;
constexpr float kEpisodePitch = 424.0f;
constexpr float kStreamTop = 296.0f;
constexpr float kStreamH = 170.0f;
constexpr float kStreamPitch = 184.0f;
constexpr float kStreamViewH = 682.0f;
constexpr float kEpisodeViewW = 1750.0f;

} // namespace

Rect Detail::episode_rect(int index, bool final_position) const
{
    const float scroll = final_position ? episode_scroll_.position.target : episode_scroll_.offset();
    return {110.0f + static_cast<float>(index) * kEpisodePitch - scroll, kListTop, kEpisodeW,
            kEpisodeH};
}

float Detail::season_width(int index) const
{
    const auto &label = content_->seasons[index];
    const float width = env_.fonts.semibold.font ? env_.fonts.semibold.measure(label, 26) : label.size() * 14.0f;
    return std::clamp(width + 48, 130.0f, 260.0f);
}

Rect Detail::season_rect(int index, bool final_position) const
{
    float x = 110;
    for (int i = 0; i < index; ++i) x += season_width(i) + 14;
    x -= final_position ? season_scroll_.position.target : season_scroll_.offset();
    return {x, 510, season_width(index), 58};
}

Rect Detail::stream_rect(int index, bool final_position) const
{
    const float scroll = final_position ? stream_scroll_.position.target : stream_scroll_.offset();
    return {kPanel.x + 28.0f, kStreamTop + static_cast<float>(index) * kStreamPitch - scroll, kPanel.w - 56.0f,
            kStreamH};
}

float Detail::filter_width(int index) const
{
    if (index < 0) return 54;
    const float text = env_.fonts.semibold.font ? env_.fonts.semibold.measure(filters_[index],24) : filters_[index].size()*13.0f;
    return std::clamp(text+36,80.0f,220.0f);
}

Rect Detail::filter_rect(int index, bool final_position) const
{
    float x = kPanel.x+28;
    for (int i = -1; i < index; ++i) x += filter_width(i)+12;
    x -= final_position ? filter_scroll_.position.target : filter_scroll_.offset();
    return {x,168,filter_width(index),50};
}

void Detail::rebuild_streams()
{
    const std::string active = filter_ < static_cast<int>(filters_.size()) ? filters_[filter_] : "All";
    const std::string cursor = filter_cursor_ >= 0 && filter_cursor_ < static_cast<int>(filters_.size()) ? filters_[filter_cursor_] : "";
    filters_ = {"All"};
    if (content_)
    {
        const auto add = [&](const std::string &name) {
            if (!name.empty() && std::find(filters_.begin()+1,filters_.end(),name) == filters_.end()) filters_.push_back(name);
        };
        for (const auto &name : content_->stream_addons) add(name);
        for (const auto &s : content_->streams) add(s.addon);
    }
    filter_ = 0;
    for (int i = 1; i < static_cast<int>(filters_.size()); ++i) if (filters_[i] == active) filter_ = i;
    if (!cursor.empty())
    {
        filter_cursor_ = 0;
        for (int i = 1; i < static_cast<int>(filters_.size()); ++i) if (filters_[i] == cursor) filter_cursor_ = i;
    }
    visible_.clear();
    if (content_)
        for (int i = 0; i < static_cast<int>(content_->streams.size()); ++i)
            if (filter_ == 0 || content_->streams[i].addon == filters_[filter_]) visible_.push_back(i);
    stream_ = std::clamp(stream_,0,std::max(0,static_cast<int>(visible_.size())-1));
    bool selected_found = false;
    for (int i = 0; i < static_cast<int>(visible_.size()); ++i)
        if (!selected_identity_.empty() && content_->streams[visible_[i]].identity == selected_identity_)
        { stream_ = i; selected_found = true; }
    // A reload may answer one addon before the selected source's addon. Keep
    // its identity until that response arrives or the whole request completes.
    if (!visible_.empty() && (selected_found || !content_->streams_loading || selected_identity_.empty()))
        selected_identity_ = content_->streams[visible_[stream_]].identity;
}

void Detail::select_filter(int index)
{
    if (index < 0 || index >= static_cast<int>(filters_.size()) || filter_ == index) return;
    filter_ = filter_cursor_ = index;
    selected_identity_.clear(); stream_ = 0;
    stream_scroll_.position.snap(0);
    rebuild_streams();
    ring_.snap(filters_focused_ ? filter_rect(filter_cursor_,true) : stream_rect(stream_,true));
}

void Detail::set_content(const DetailContent *content)
{
    content_ = content;
    episode_ = content ? std::clamp(content->episode_hint,0,std::max(0,static_cast<int>(content->episodes.size())-1)) : 0;
    season_cursor_ = shown_season_ = content ? content->season : 0;
    seasons_focused_ = false;
    season_scroll_.position.snap(0);
    stream_ = 0;
    filter_ = filter_cursor_ = 0;
    filters_focused_ = false;
    selected_identity_.clear();
    filters_.clear();
    rebuild_streams();
    filter_scroll_.position.snap(0);
    streams_wanted_ = false;
    hint_serial_ = content ? content->hint_serial : 0;
    streams_serial_ = content ? content->streams_serial : 0;
    episode_scroll_.position.snap(0.0f);
    stream_scroll_.position.snap(0.0f);
    set_mode(content && content->series ? Mode::episodes : Mode::streams);
}

void Detail::set_mode(Mode mode)
{
    mode_ = mode;
    swap_.start(0.3f);
    ring_.snap(mode_ == Mode::episodes ? episode_rect(episode_, true) : stream_rect(stream_, true));
}

void Detail::enter()
{
    age_ = 0.0f;
}

void Detail::refresh()
{
    if (!content_)
        return;
    const int episodes = static_cast<int>(content_->episodes.size());
    rebuild_streams();
    if (content_->hint_serial != hint_serial_)
    {
        hint_serial_ = content_->hint_serial;
        episode_ = std::clamp(content_->episode_hint, 0, std::max(0, episodes - 1));
        episode_scroll_.position.snap(0.0f);
    }
    if (shown_season_ != content_->season)
    {
        shown_season_ = season_cursor_ = content_->season;
        swap_.start(0.3f);
    }
    season_cursor_ = std::clamp(season_cursor_, 0, std::max(0, static_cast<int>(content_->seasons.size()) - 1));
    if (content_->seasons.empty()) seasons_focused_ = false;
    episode_ = std::min(episode_, std::max(0, episodes - 1));

    // The streams of an episode were asked for from outside (Continue Watching): show them.
    if (content_->streams_serial != streams_serial_)
    {
        streams_serial_ = content_->streams_serial;
        if (content_->series)
        {
            set_mode(Mode::streams);
            streams_wanted_ = true;
        }
    }
    // The details arrived and it is a film after all: there are no episodes to show.
    if (!content_->series && mode_ == Mode::episodes)
        set_mode(Mode::streams);
    else if (content_->series && mode_ == Mode::streams && !streams_wanted_)
        set_mode(Mode::episodes);
}

void Detail::update(const hui::InputFrame &input, float dt)
{
    age_ += dt;
    clock_ += dt;
    if (!content_)
        return;
    if (mode_ == Mode::episodes)
    {
        const int count = static_cast<int>(content_->episodes.size());
        if (seasons_focused_)
        {
            if (input.nav == Direction::left) season_cursor_ = std::max(0, season_cursor_ - 1);
            else if (input.nav == Direction::right)
                season_cursor_ = std::min(static_cast<int>(content_->seasons.size()) - 1, season_cursor_ + 1);
            else if (input.nav == Direction::down && count > 0) seasons_focused_ = false;
        }
        else
        {
            if (input.nav == Direction::left) episode_ = std::max(0, episode_ - 1);
            else if (input.nav == Direction::right) episode_ = std::min(std::max(0, count - 1), episode_ + 1);
            else if (input.nav == Direction::up && !content_->seasons.empty())
            {
                seasons_focused_ = true;
                season_cursor_ = content_->season;
            }
            if (count == 0 && !content_->seasons.empty()) seasons_focused_ = true;
        }
        if (input.is_pressed(Action::confirm) && count > 0 && !seasons_focused_)
        {
            set_mode(Mode::streams);
            streams_wanted_ = true;
        }
    }
    else
    {
        if (input.is_pressed(Action::page_prev)) select_filter(std::max(0,filter_-1));
        else if (input.is_pressed(Action::page_next)) select_filter(std::min(static_cast<int>(filters_.size())-1,filter_+1));
        if (filters_focused_)
        {
            if (input.nav == Direction::left) filter_cursor_ = std::max(-1,filter_cursor_-1);
            else if (input.nav == Direction::right) filter_cursor_ = std::min(static_cast<int>(filters_.size())-1,filter_cursor_+1);
            if (input.is_pressed(Action::confirm) && filter_cursor_ >= 0) select_filter(filter_cursor_);
            if (input.nav == Direction::down && !visible_.empty()) filters_focused_ = false;
        }
        else
        {
            if (input.nav == Direction::up)
            {
                if (stream_ == 0) { filters_focused_ = true; filter_cursor_ = filter_; }
                else --stream_;
            }
            else if (input.nav == Direction::down) stream_ = std::min(std::max(0,static_cast<int>(visible_.size())-1),stream_+1);
            if (visible_.empty() && !content_->streams_loading) filters_focused_ = true;
        }
        if (stream_ < static_cast<int>(visible_.size()) &&
            (!content_->streams_loading || selected_identity_.empty() ||
             (!filters_focused_ && (input.nav == Direction::up || input.nav == Direction::down ||
                                   input.is_pressed(Action::confirm)))))
            selected_identity_ = content_->streams[visible_[stream_]].identity;
        if (input.is_pressed(Action::back) && content_->series)
        {
            set_mode(Mode::episodes);
            streams_wanted_ = false;
        }
    }
    const float e = static_cast<float>(episode_) * kEpisodePitch;
    episode_scroll_.reveal(e, e + kEpisodeW, kEpisodeViewW, 16.0f,
                          std::max(0.0f, content_->episodes.size() * kEpisodePitch - 24));
    if (!content_->seasons.empty())
    {
        const int selected = std::clamp(seasons_focused_ ? season_cursor_ : content_->season,
                                        0,static_cast<int>(content_->seasons.size())-1);
        const Rect r = season_rect(selected, true);
        season_scroll_.reveal(r.x - 110 + season_scroll_.position.target,
                             r.x - 110 + season_scroll_.position.target + r.w, kEpisodeViewW, 4);
    }
    season_scroll_.update(dt, 18);
    const float s = static_cast<float>(stream_) * kStreamPitch;
    stream_scroll_.reveal(s, s + kStreamH, kStreamViewH, 8.0f, static_cast<float>(visible_.size()) * kStreamPitch);
    const Rect f = filter_rect(filter_cursor_,true);
    filter_scroll_.reveal(f.x - kPanel.x - 28 + filter_scroll_.position.target,
                          f.x - kPanel.x - 28 + filter_scroll_.position.target + f.w, kPanel.w-56, 4);
    filter_scroll_.update(dt,18);
    episode_scroll_.update(dt, 18.0f);
    stream_scroll_.update(dt, 18.0f);
    ring_.target(mode_ == Mode::episodes ?
                 (seasons_focused_ && !content_->seasons.empty() ? season_rect(season_cursor_,true) : episode_rect(episode_, true)) :
                 filters_focused_ ? filter_rect(filter_cursor_,true) : stream_rect(stream_, true));
    ring_.update(dt, 22.0f);
    swap_.update(dt);
}

void Detail::draw_info(hui::gfx::DrawList &list) const
{
    const Title &t = content_->title;
    if (mode_ == Mode::episodes)
    {
        list.push_opacity(tween::stagger(age_, 0, 0, 0.6f));
        auto lines = env_.fonts.display.font->wrap(t.name, 66, 1500);
        if (lines.size() > 2) { lines.resize(2); lines.back() = env_.fonts.display.font->fit(lines.back()+" …",66,1500); }
        float baseline = 206;
        for (const auto &line : lines)
        {
            text(list, env_.fonts.display, env_.fonts.display.font->fit(line,66,1500),110,baseline,66,theme::ink);
            baseline += 74;
        }
        const std::string meta = t.year + (t.genres.empty() ? "" : "   ·   " + t.genres);
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(meta,26,1250),110,328,26,theme::ink_2);
        draw_imdb(list,env_,t.imdb,1430,328);
        const Episode *selected = content_->episodes.empty() ? nullptr : &content_->episodes[episode_];
        const std::string heading = selected ? "Episode " + std::to_string(selected->number) + "  ·  " + selected->title : "Episodes";
        text(list,env_.fonts.semibold,env_.fonts.semibold.font->fit(heading,32,1600),110,406,32,theme::ink);
        hui::ui::paragraph(list,env_.fonts.regular,selected && !selected->synopsis.empty() ? selected->synopsis : t.synopsis,
                           110,448,26,1560,36,theme::ink_2,2);
        list.pop_opacity();
        return;
    }
    if (!content_->series)
    {
        list.push_opacity(tween::stagger(age_, 0, 0, 0.6f));
        const Rect poster{110,160,240,360};
        list.shadow({poster.x+6,poster.y+16,poster.w-12,poster.h},20,30,Color::rgb(0,0.4f));
        if (t.poster) list.image(t.poster,poster,kFullUv,Color::rgb(0xffffff),20);
        else list.gradient_rect(poster,20,theme::accent_deep,theme::ground);
        list.bordered_rect(poster,20,Color::rgb(0xffffff,0),1,theme::hairline);
        auto lines = env_.fonts.display.font->wrap(t.name,58,588);
        if (lines.size() > 3) { lines.resize(3); lines.back() = env_.fonts.display.font->fit(lines.back()+" …",58,588); }
        float baseline = 218;
        for (const auto &line : lines)
        {
            text(list,env_.fonts.display,env_.fonts.display.font->fit(line,58,588),382,baseline,58,theme::ink);
            baseline += 64;
        }
        baseline += 10;
        text(list,env_.fonts.regular,t.year + (t.runtime.empty() ? "" : "   ·   " + t.runtime),382,baseline,26,theme::ink_2);
        baseline += 40;
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(t.genres,24,588),382,baseline,24,theme::ink_2);
        draw_imdb(list,env_,t.imdb,382,baseline+44);
        baseline = 580;
        if (!content_->resume_label.empty())
        {
            draw_tag(list,env_,content_->resume_label,110,558,theme::surface,theme::ink);
            baseline = 618;
        }
        baseline = hui::ui::paragraph(list,env_.fonts.regular,t.synopsis,110,baseline,27,840,39,theme::ink_2,4) + 32;
        if (!content_->directors.empty())
        {
            text(list,env_.fonts.regular,"Director",110,baseline,22,theme::ink_3);
            text(list,env_.fonts.regular,env_.fonts.regular.font->fit(content_->directors,24,690),260,baseline,24,theme::ink_2);
            baseline += 40;
        }
        if (!content_->cast.empty())
        {
            text(list,env_.fonts.regular,"Cast",110,baseline,22,theme::ink_3);
            baseline = hui::ui::paragraph(list,env_.fonts.regular,content_->cast,260,baseline,24,690,34,theme::ink_2,2) + 28;
        }
        const int selected = stream_index();
        if (selected >= 0 && !content_->streams[selected].detail.empty() && baseline < 900)
        {
            text(list,env_.fonts.regular,"Selected release",110,baseline,22,theme::ink_3);
            hui::ui::paragraph(list,env_.fonts.regular,content_->streams[selected].detail,110,baseline+34,23,840,32,theme::ink_2,2);
        }
        list.pop_opacity();
        return;
    }
    const float x = 110.0f;
    const float w = 840.0f;
    const float in = tween::stagger(age_, 0, 0.0f, 0.6f);
    list.push_opacity(in);
    const hui::ui::FontRef &display = env_.fonts.display;
    std::vector<std::string> lines = display.font->wrap(t.name, 66.0f, w);
    if (lines.size() > 2)
    {
        lines.resize(2);
        lines[1] = display.font->fit(lines[1] + " \xE2\x80\xA6", 66.0f, w);
    }
    float baseline = 206.0f;
    for (const std::string &line : lines)
    {
        text(list, display, display.font->fit(line, 66.0f, w), x, baseline, 66.0f, theme::ink, Align::left, -1.4f);
        baseline += 74.0f;
    }
    baseline += 2.0f;
    std::string meta = t.year;
    if (!t.runtime.empty())
        meta += "   \xC2\xB7   " + t.runtime;
    if (!t.genres.empty())
        meta += "   \xC2\xB7   " + t.genres;
    // The rating badge follows the line, so the line leaves room for it before the panel.
    meta = env_.fonts.regular.font->fit(meta, 26.0f, t.imdb.empty() ? w : w - 190.0f);
    const float meta_w = text(list, env_.fonts.regular, meta, x, baseline, 26.0f, theme::ink_2);
    draw_imdb(list, env_, t.imdb, x + meta_w + 30.0f, baseline);
    baseline += 58.0f;
    if (mode_ == Mode::streams && !content_->resume_label.empty())
    {
        draw_tag(list,env_,content_->resume_label,x,baseline-10,theme::surface,theme::ink);
        baseline += 58;
    }
    baseline = hui::ui::paragraph(list, env_.fonts.regular, t.synopsis, x, baseline, 27.0f, w - 40.0f, 41.0f,
                                  theme::ink_2, mode_ == Mode::streams ? 4 : 6);
    baseline += 18.0f;
    if (!content_->directors.empty())
    {
        text(list, env_.fonts.regular, content_->series ? "Creators" : "Director", x, baseline, 22.0f, theme::ink_3);
        text(list, env_.fonts.regular, env_.fonts.regular.font->fit(content_->directors, 24.0f, w - 190.0f), x + 150.0f,
             baseline, 24.0f, theme::ink_2);
        baseline += 42.0f;
    }
    if (!content_->cast.empty())
    {
        text(list, env_.fonts.regular, "Cast", x, baseline, 22.0f, theme::ink_3);
        baseline = hui::ui::paragraph(list, env_.fonts.regular, content_->cast, x + 150.0f, baseline, 24.0f, w - 190.0f, 36.0f,
                                      theme::ink_2, 2);
    }
    const int selected = stream_index();
    if (mode_ == Mode::streams && selected >= 0 && !content_->streams[selected].detail.empty())
    {
        baseline += 56;
        text(list,env_.fonts.regular,"Selected release",x,baseline,22,theme::ink_3);
        hui::ui::paragraph(list,env_.fonts.regular,content_->streams[selected].detail,x,baseline+38,24,w,34,theme::ink_2,3);
    }
    list.pop_opacity();
}

void Detail::draw_episodes(hui::gfx::DrawList &list) const
{
    list.push_clip({104,502,kEpisodeViewW+12,76});
    for (int i = 0; i < static_cast<int>(content_->seasons.size()); ++i)
    {
        const Rect r = season_rect(i,false);
        if (r.x > 1860 || r.x+r.w < 104) continue;
        const bool active = i == content_->season;
        list.rounded_rect(r,23,active ? Color::rgb(0xf0f0f0) : Color::rgb(0x161b25,0.86f));
        text(list,env_.fonts.semibold,env_.fonts.semibold.font->fit(content_->seasons[i],26,r.w-28),
             r.cx(),r.cy()+9,26,active ? Color::rgb(0x111111) : theme::ink,Align::center);
        if (seasons_focused_ && i == season_cursor_)
            list.bordered_rect(r,23,Color::rgb(0xffffff,0),3,theme::accent_soft);
    }
    list.pop_clip();
    char count[80];
    std::snprintf(count,sizeof(count),"%zu episodes  ·  %s",content_->episodes.size(),
                  seasons_focused_ ? "Choose a season, then press Cross" : "Up to choose a season");
    text(list,env_.fonts.regular,count,110,596,22,theme::ink_2);
    list.push_clip({94,kListTop-8,kEpisodeViewW+32,kEpisodeH+34});
    for (int i = 0; i < static_cast<int>(content_->episodes.size()); ++i)
    {
        const Rect r = episode_rect(i,false);
        if (r.x > 1860 || r.x+r.w < 94) continue;
        const Episode &e = content_->episodes[i];
        const bool focused = !seasons_focused_ && i == episode_;
        list.rounded_rect(r,22,focused ? Color::rgb(0x222735,0.96f) : Color::rgb(0x0d1118,0.88f));
        const Rect thumb{r.x+12,r.y+12,r.w-24,(r.w-24)*9/16};
        if (e.thumb) list.image(e.thumb,thumb,kFullUv,Color::rgb(0xffffff),14);
        else
        {
            list.gradient_rect(thumb,14,theme::accent_deep,theme::ground);
            text(list,env_.fonts.semibold,"Episode " + std::to_string(e.number),thumb.cx(),thumb.cy()+10,30,theme::ink_2,Align::center);
        }
        if (e.progress >= 0)
        {
            list.rounded_rect({thumb.x+10,thumb.y+thumb.h-14,thumb.w-20,5},2.5f,Color::rgb(0xffffff,0.30f));
            list.rounded_rect({thumb.x+10,thumb.y+thumb.h-14,(thumb.w-20)*std::clamp(e.progress,0.0f,1.0f),5},2.5f,theme::accent_soft);
        }
        auto lines = env_.fonts.semibold.font->wrap(std::to_string(e.number) + ". " + e.title,27,r.w-40);
        if (lines.size() > 2) lines.resize(2);
        float baseline = thumb.y+thumb.h+36;
        for (const auto &line : lines)
        {
            text(list,env_.fonts.semibold,env_.fonts.semibold.font->fit(line,27,r.w-40),r.x+20,baseline,27,e.watched ? theme::ink_2 : theme::ink);
            baseline += 32;
        }
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(e.info,22,r.w-40),r.x+20,r.y+r.h-19,22,theme::ink_3);
        if (e.watched)
        {
            const float cx = thumb.x+thumb.w-25, cy = thumb.y+25;
            list.circle(cx,cy,17,theme::watched);
            list.line(cx-7,cy+1,cx-2,cy+6.5f,3.4f,Color::rgb(0xffffff));
            list.line(cx-2,cy+6.5f,cx+8,cy-6,3.4f,Color::rgb(0xffffff));
        }
    }
    if (!seasons_focused_ && !content_->episodes.empty())
        list.bordered_rect(ring_.value(),22,Color::rgb(0xffffff,0),3,Color::rgb(0xffffff));
    list.pop_clip();
    if (content_->episodes.empty())
        hui::ui::paragraph(list,env_.fonts.regular,content_->episodes_status,110,kListTop+70,28,kEpisodeViewW,40,theme::ink_2,3);
}

void Detail::draw_streams(hui::gfx::DrawList &list) const
{
    const float hx = kPanel.x + 28;
    const std::string heading = content_->stream_heading.empty() ? "Streams" : content_->stream_heading;
    text(list,env_.fonts.semibold,env_.fonts.semibold.font->fit(heading,30,kPanel.w-210),hx,137,30,theme::ink);
    char count[48]; std::snprintf(count,sizeof(count),"%zu streams",visible_.size());
    text(list,env_.fonts.regular,count,kPanel.x+kPanel.w-28,137,22,theme::ink_3,Align::right);
    list.push_clip({hx-4,160,kPanel.w-48,66});
    for (int i = -1; i < static_cast<int>(filters_.size()); ++i)
    {
        const Rect r = filter_rect(i,false);
        const bool active = i == filter_;
        list.rounded_rect(r,24,active ? Color::rgb(0xf0f0f0) : Color::rgb(0x202020));
        const Color ink = active ? Color::rgb(0x111111) : theme::ink;
        if (i == -1)
        {
            list.arc(r.cx(),r.cy(),10,2.2f,0.7f,5.0f,ink);
            list.triangle({r.cx()+5,r.cy()-12,7,8},ink,0,1.3f);
        }
        else text(list,env_.fonts.semibold,env_.fonts.semibold.font->fit(filters_[i],24,r.w-30),r.cx(),r.cy()+8,24,ink,Align::center);
        if (filters_focused_ && i == filter_cursor_)
            list.bordered_rect(r,24,Color::rgb(0xffffff,0),3,theme::accent);
    }
    list.pop_clip();
    const std::string provider = filter_ > 0 ? filters_[filter_] : "All sources";
    text(list,env_.fonts.semibold,env_.fonts.semibold.font->fit(provider,26,280),hx,268,26,theme::ink_2);
    if(content_->streams_loading && !visible_.empty())
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(content_->streams_status,20,kPanel.w-350),hx+300,268,20,theme::ink_3);

    list.push_clip({kPanel.x,kStreamTop-4,kPanel.w,kStreamViewH+8});
    const Rect ring = ring_.value();
    for (int i = 0; i < static_cast<int>(visible_.size()); ++i)
    {
        const Rect r = stream_rect(i,false);
        if (r.y > kStreamTop+kStreamViewH || r.y+r.h < kStreamTop) continue;
        const Stream &s = content_->streams[visible_[i]];
        const bool focused = !filters_focused_ && i == stream_;
        list.rounded_rect(r,22,focused ? Color::rgb(0x262626) : Color::rgb(0x1b1b1b));
        const float x = r.x+24, room = r.w-48;
        text(list,env_.fonts.semibold,env_.fonts.semibold.font->fit(s.name,30,room),x,r.y+40,30,theme::ink);
        float dx = x;
        if (!s.size.empty())
        {
            stream_file_icon(list,dx,r.y+57,theme::ink_3);
            dx += 29 + text(list,env_.fonts.regular,s.size,dx+29,r.y+76,23,theme::ink_3);
            dx += 22;
        }
        if (!s.language.empty())
        {
            stream_globe_icon(list,dx,r.y+68,theme::ink_3);
            dx += 30 + text(list,env_.fonts.regular,env_.fonts.regular.font->fit(s.language,23,170),dx+30,r.y+76,23,theme::ink_3);
            dx += 22;
        }
        const std::string source = s.addon + (s.seeds.empty() ? "" : "  ·  " + s.seeds) + (s.prepared ? "  ·  Ready" : "");
        text(list,env_.fonts.regular,env_.fonts.regular.font->fit(source,21,std::max(0.0f,r.x+r.w-24-dx)),dx,r.y+76,21,theme::ink_3);
        draw_stream_badges(list,env_,s,x,r.y+109,room);
        if (s.badges.empty() && s.detail != s.name)
            text(list,env_.fonts.regular,env_.fonts.regular.font->fit(s.detail,22,room),x,r.y+117,22,theme::ink_2);
    }
    if (!visible_.empty() && !filters_focused_)
        list.bordered_rect(ring,22,Color::rgb(0xffffff,0),3,Color::rgb(0xffffff));
    list.pop_clip();
    if (visible_.empty())
    {
        const std::string status = !content_->streams_status.empty() ? content_->streams_status : "This addon returned no playable streams.";
        hui::ui::paragraph(list,env_.fonts.regular,status,hx+16,kStreamTop+52,26,kPanel.w-88,38,theme::ink_3,5);
    }
}

void Detail::draw_hints_row(hui::gfx::DrawList &list) const
{
    hui::ui::HintLayout layout;
    layout.size = 36.0f;
    layout.text_size = 22.0f;
    layout.cy = 1038.0f;
    layout.item_gap = 24.0f;
    layout.font = &env_.fonts.regular;
    if (mode_ == Mode::episodes)
    {
        const hui::ui::Hint hints[] = {{hui::ui::Button::cross, seasons_focused_ ? "Select season" : "Streams"},
                                       {hui::ui::Button::l1, "Season", hui::ui::Button::r1},
                                       {hui::ui::Button::options, "Watch trailer"},
                                       {hui::ui::Button::circle, "Back"}};
        hui::ui::draw_hints(list, env_.fonts, hui::ui::GlyphStyle::dark(), hints, 4, theme::kRight, true, layout);
    }
    else
    {
        const hui::ui::Hint hints[] = {{hui::ui::Button::cross, filters_focused_ ? "Select" : "Play"},
                                       {hui::ui::Button::l1, "Addon", hui::ui::Button::r1},
                                       {hui::ui::Button::triangle, "Reload"},
                                       {hui::ui::Button::options, "Watch trailer"},
                                       {hui::ui::Button::circle, "Back"}};
        hui::ui::draw_hints(list, env_.fonts, hui::ui::GlyphStyle::dark(), hints, 5, theme::kRight, true, layout);
    }
}

void Detail::draw(hui::gfx::DrawList &list, hui::gfx::BackdropSpec &backdrop) const
{
    backdrop.mode = hui::gfx::BackdropMode::none;
    list.rounded_rect({0, 0, theme::kScreenW, theme::kScreenH}, 0, theme::ground);
    if (!content_)
        return;
    const float intro = tween::stagger(age_, 0, 0.0f, 0.8f);
    // The artwork fills the screen, then the page closes over it where text goes.
    if (content_->title.backdrop != 0)
    {
        list.push_opacity(intro);
        list.image(content_->title.backdrop, {0, 0, theme::kScreenW, theme::kScreenH}, kFullUv,
                   Color::rgb(0xffffff, 0.82f));
        list.pop_opacity();
    }
    list.gradient_rect_h({0, 0, 1400.0f, theme::kScreenH}, 0, theme::ground.with_alpha(mode_ == Mode::episodes ? 0.76f : 0.88f),
                         theme::ground.with_alpha(0.10f));
    list.gradient_rect({0, 640.0f, theme::kScreenW, 440.0f}, 0, theme::ground.with_alpha(0.0f),
                       theme::ground.with_alpha(0.88f));
    list.gradient_rect({0, 0, theme::kScreenW, 200.0f}, 0, theme::ground.with_alpha(0.6f),
                       theme::ground.with_alpha(0.0f));

    list.push_opacity(intro);
    draw_logo(list, env_);
    list.pop_opacity();
    if (mode_ == Mode::streams) list.push_clip({104,144,916,852});
    draw_info(list);
    if (mode_ == Mode::streams) list.pop_clip();

    // The series carousel uses the full width; source lists keep their own panel.
    const float panel_in = tween::stagger(age_, 1, 0.08f, 0.5f);
    list.push_opacity(panel_in);
    if (mode_ == Mode::streams)
    {
        list.shadow({kPanel.x+8,kPanel.y+26,kPanel.w-16,kPanel.h-20},34,56,Color::rgb(0,0.45f));
        list.bordered_rect(kPanel,36,Color::rgb(0x0d0d0d,0.92f),1.5f,theme::hairline);
    }
    const float swap = tween::smoothstep(swap_.progress());
    list.push_opacity(swap);
    if (mode_ == Mode::episodes)
        draw_episodes(list);
    else
        draw_streams(list);
    list.pop_opacity();
    list.pop_opacity();
    draw_hints_row(list);
}

} // namespace sx
