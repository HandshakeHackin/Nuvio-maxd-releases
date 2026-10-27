#include "player.hpp"

#include "cards.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sx
{

using hui::gfx::Align;
using hui::gfx::Color;
using hui::gfx::Rect;
using hui::ui::text;
namespace tween = hui::tween;

namespace
{

std::string clock_text(double seconds)
{
    const long total = static_cast<long>(std::max(0.0, seconds));
    char out[24];
    if (total >= 3600)
        std::snprintf(out, sizeof(out), "%ld:%02ld:%02ld", total / 3600, total / 60 % 60, total % 60);
    else
        std::snprintf(out, sizeof(out), "%ld:%02ld", total / 60, total % 60);
    return out;
}

} // namespace

void draw_outlined_text(hui::gfx::DrawList &list, const hui::ui::FontRef &font, const std::string &value, float x,
                        float baseline, float size, Color colour, Align align)
{
    const float r = std::max(2.0f, size * 0.055f);
    static const float kDir[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0.7f, 0.7f}, {-0.7f, 0.7f}, {0.7f, -0.7f}, {-0.7f, -0.7f}};
    for (const auto &d : kDir)
        text(list, font, value, x + d[0] * r, baseline + d[1] * r, size, Color::rgb(0x000000, 0.92f * colour.a), align);
    text(list, font, value, x, baseline, size, colour, align);
}

void PlayerUi::snap()
{
    controls_.snap(state.controls ? 1.0f : 0.0f);
    menu_.snap(state.menu ? 1.0f : 0.0f);
    paused_.snap(state.paused ? 1.0f : 0.0f);
    buffering_.snap(state.buffering >= 0 ? 1.0f : 0.0f);
}

void PlayerUi::update(float dt)
{
    if (!tween::reduced_motion) clock_ += dt;
    controls_.target = state.controls || state.paused || state.menu ? 1.0f : 0.0f;
    controls_.update(dt, 16.0f);
    menu_.target = state.menu ? 1.0f : 0.0f;
    menu_.update(dt, 18.0f);
    paused_.target = state.paused ? 1.0f : 0.0f;
    paused_.update(dt, 20.0f);
    buffering_.target = state.buffering >= 0 ? 1.0f : 0.0f;
    buffering_.update(dt, 14.0f);
}

void draw_subtitle_block(hui::gfx::DrawList& list, const hui::ui::Fonts& fonts, const std::string& value,
                         const SubtitleStyle& style, float size, float centre, float bottom, float width)
{
    if(value.empty()) return;
    const auto& font = style.font == 0 ? fonts.regular : style.font == 2 ? fonts.display : fonts.semibold;
    const auto lines = font.font->wrap(value, size, width);
    const auto colour = Color::rgb(style.colour == 1 ? 0xffe85c : style.colour == 2 ? 0x80eaff : 0xffffff);
    const float leading=size*1.16f;
    float baseline=bottom-leading*float(lines.size()-1);
    for(const auto& line : lines) {
        if(style.background>0 && !line.empty()) {
            const float w=font.measure(line,size);
            list.rounded_rect({centre-w*0.5f-12,baseline-size-6,w+24,size*1.25f+12},8,
                              Color::rgb(0x000000,std::clamp(style.background/100.0f,0.0f,1.0f)));
        }
        if(style.effect==1) draw_outlined_text(list,font,line,centre,baseline,size,colour,Align::center);
        else {
            if(style.effect==2) text(list,font,line,centre+3,baseline+3,size,Color::rgb(0x000000,0.95f),Align::center);
            text(list,font,line,centre,baseline,size,colour,Align::center);
        }
        baseline+=leading;
    }
}
void PlayerUi::draw_subtitle(hui::gfx::DrawList &list) const
{
    if(state.subtitle_texture) { list.image(state.subtitle_texture,{state.subtitle_rect.x,state.subtitle_rect.y,state.subtitle_rect.w,state.subtitle_rect.h},{0,0,1,1},Color::rgb(0xffffff)); return; }
    const float size=state.sub_size==0 ? 42.0f : state.sub_size==2 ? 60.0f : 50.0f;
    const float raise=150.0f*tween::smoothstep(controls_.value);
    draw_subtitle_block(list,env_.fonts,state.line,state.subtitle_style,size,theme::kScreenW*0.5f,960.0f-raise,1640.0f);
}

void PlayerUi::draw_controls(hui::gfx::DrawList &list, float amount) const
{
    list.push_opacity(amount);
    list.gradient_rect({0, 0, theme::kScreenW, 320.0f}, 0, Color::rgb(0x000000, 0.72f), Color::rgb(0x000000, 0.0f));
    list.gradient_rect({0, 740.0f, theme::kScreenW, 340.0f}, 0, Color::rgb(0x000000, 0.0f), Color::rgb(0x000000, 0.45f));

    text(list, env_.fonts.semibold, env_.fonts.semibold.font->fit(state.title, 42.0f, 1500.0f), 110.0f, 108.0f, 42.0f,
         theme::ink, Align::left, -0.4f);
    if (!state.subtitle.empty())
        text(list, env_.fonts.regular, state.subtitle, 110.0f, 152.0f, 28.0f, theme::ink_2);
    const float info_y=state.subtitle.empty()?152.0f:192.0f;
    float badge_x=110.0f;
    for(const auto& badge:state.badges) {
        const float width=env_.fonts.semibold.measure(badge,22.0f)+24.0f;
        if(badge_x+width>1810.0f)break;
        list.bordered_rect({badge_x,info_y-21.0f,width,34.0f},8.0f,
                           Color::rgb(0x101723,theme::high_contrast?1.0f:0.8f),1.5f,Color::rgb(0xc6b7ff,theme::high_contrast?1.0f:0.9f));
        text(list,env_.fonts.semibold,badge,badge_x+12.0f,info_y+4.0f,22.0f,Color::rgb(0xe4dcff));
        badge_x+=width+10.0f;
    }
    const float badge_space=state.badges.empty()?0.0f:44.0f;
    // How it is being decoded: hardware or software, the frame rate, what was dropped.
    if (!state.stats.empty())
        text(list, env_.fonts.regular, env_.fonts.regular.font->fit(state.stats, 22.0f, 1700.0f), 110.0f,
             info_y+badge_space, 22.0f, theme::ink_3);
    if (!state.quality.empty())
        text(list, env_.fonts.regular, env_.fonts.regular.font->fit(state.quality, 22.0f, 1700.0f), 110.0f,
             info_y+badge_space+34.0f, 22.0f, theme::ink_2);

    // One translucent transport panel, composited over the original picture.
    // It uses ordinary alpha blending, so showing controls needs no extra video blur pass.
    list.push_transform(1,0,0,0,24*(1-tween::smoothstep(amount)));
    const Rect transport{84,812,1752,230};
    list.shadow({transport.x+8,transport.y+12,transport.w-16,transport.h-12},28,24,Color::rgb(0,0.28f));
    list.bordered_rect(transport,28,Color::rgb(0x101723,0.72f),1.5f,Color::rgb(0xffffff,0.16f));
    // The seek bar.
    const float x = 110.0f, w = 1700.0f, y = 862.0f;
    const double duration = std::max(1.0, state.duration);
    const float played = static_cast<float>(std::clamp(state.position / duration, 0.0, 1.0));
    const float ready = static_cast<float>(std::clamp((state.position + state.buffered) / duration, 0.0, 1.0));
    list.rounded_rect({x, y, w, 8.0f}, 4.0f, Color::rgb(0xffffff, 0.22f));
    list.rounded_rect({x, y, w * std::max(ready, played), 8.0f}, 4.0f, Color::rgb(0xffffff, 0.28f));
    list.glow({x, y, w * played, 8.0f}, 4.0f, 12.0f, theme::accent.with_alpha(0.55f));
    list.rounded_rect({x, y, w * played, 8.0f}, 4.0f, theme::accent_soft);
    list.circle(x + w * played, y + 4.0f, 14.0f, Color::rgb(0xffffff));
    text(list, env_.fonts.semibold, clock_text(state.position), x, y + 56.0f, 26.0f, theme::ink);
    text(list, env_.fonts.regular, "-" + clock_text(state.duration - state.position), x + w, y + 56.0f, 26.0f, theme::ink_2,
         Align::right);

    // Tracks on the left, buttons on the right.
    // Long track names share what room the buttons leave: the shorter one
    // keeps its full name, the longer one gives way.
    const std::string audio = "Audio  " + state.audio, subs = "Subtitles  " + state.subs;
    const float budget = 560.0f;
    const float na = env_.fonts.semibold.measure(audio.c_str(), theme::kBody);
    const float ns = env_.fonts.semibold.measure(subs.c_str(), theme::kBody);
    float ma = na, ms = ns;
    if (na + ns > budget)
    {
        if (na <= budget * 0.5f)
            ms = budget - na;
        else if (ns <= budget * 0.5f)
            ma = budget - ns;
        else
            ma = ms = budget * 0.5f;
    }
    float cx = x;
    cx += draw_chip(list, env_, hui::ui::Button::triangle, audio.c_str(), cx, 1010.0f, false, ma) + 14.0f;
    cx += draw_chip(list, env_, hui::ui::Button::square, subs.c_str(), cx, 1010.0f, false, ms) + 14.0f;
    draw_chip(list, env_, hui::ui::Button::options, "Tracks", cx, 1010.0f);
    hui::ui::HintLayout layout;
    layout.size = 36.0f;
    layout.text_size = 24.0f;
    layout.cy = 1010.0f;
    layout.item_gap = 34.0f;
    layout.font = &env_.fonts.regular;
    const hui::ui::Hint hints[] = {{hui::ui::Button::cross, state.paused ? "Play" : "Pause"},
                                   {hui::ui::Button::dpad, "10 s"},
                                   {hui::ui::Button::l1, "1 min", hui::ui::Button::r1},
                                   {hui::ui::Button::circle, "Stop"}};
    hui::ui::draw_hints(list, env_.fonts, hui::ui::GlyphStyle::dark(), hints, 4, x + w, true, layout);
    list.pop_transform();
    list.pop_opacity();
}

void PlayerUi::draw_buffering(hui::gfx::DrawList &list, float amount) const
{
    list.push_opacity(amount);
    list.rounded_rect({0, 0, theme::kScreenW, theme::kScreenH}, 0, Color::rgb(0x000000, 0.38f));
    const float cx = theme::kScreenW * 0.5f, cy = 470.0f;
    list.ring(cx, cy, 52.0f, 7.0f, Color::rgb(0xffffff, 0.18f));
    list.arc(cx, cy, 55.0f, 8.0f, clock_ * 5.2f, 1.7f, theme::accent_soft);
    char label[48];
    std::snprintf(label, sizeof(label), "Buffering  %d%%", state.buffering);
    text(list, env_.fonts.semibold, label, cx, cy + 128.0f, 36.0f, theme::ink, Align::center);
    if (!state.net.empty())
        text(list, env_.fonts.regular, state.net, cx, cy + 172.0f, 26.0f, theme::ink_2, Align::center);
    list.pop_opacity();
}

void PlayerUi::draw_menu(hui::gfx::DrawList &list, float amount) const
{
    list.push_opacity(amount);
    list.rounded_rect({0, 0, theme::kScreenW, theme::kScreenH}, 0, Color::rgb(0x07060c, 0.35f));
    const Rect panel{260.0f, 150.0f, 1400.0f, 780.0f};
    list.push_transform(0.97f + 0.03f * amount, panel.cx(), panel.cy(), 0.0f, 0.0f);
    list.shadow({panel.x + 10, panel.y + 30, panel.w - 20, panel.h - 20}, 38, 60, Color::rgb(0x000000, 0.55f));
    list.bordered_rect(panel, 38.0f, Color::rgb(0x101723, theme::high_contrast ? 1.0f : 0.88f), 1.5f, theme::high_contrast ? theme::hairline : Color::rgb(0xffffff,0.16f));
    struct Column
    {
        const char *title;
        const std::vector<std::string> *tracks;
        int active;
    };
    const Column columns[2] = {{"Audio", &state.audio_tracks, state.audio_active},
                               {"Subtitles", &state.subtitle_tracks, state.subtitle_active}};
    for (int c = 0; c < 2; ++c)
    {
        const bool focused = state.menu_column == c;
        const float x = panel.x + 50.0f + static_cast<float>(c) * 660.0f;
        list.push_opacity(focused || theme::high_contrast ? 1.0f : 0.55f);
        text(list, env_.fonts.semibold, columns[c].title, x, panel.y + 88.0f, 34.0f, theme::ink);
        // Long lists scroll: the window follows the cursor, or the ticked track.
        const int total = static_cast<int>(columns[c].tracks->size());
        const int anchor = focused ? state.menu_row : columns[c].active;
        const int first = std::max(0, std::min(anchor - 3, total - 7));
        for (int k = 0; k < 7 && first + k < total; ++k)
        {
            const int i = first + k;
            const Rect r{x, panel.y + 124.0f + static_cast<float>(k) * 76.0f, 600.0f, 66.0f};
            const bool selected = focused && state.menu_row == i;
            if (selected)
            {
                list.glow(r, 20.0f, 22.0f, theme::accent.with_alpha(0.32f));
                list.bordered_rect(r, 20.0f, theme::surface_hi, 3.0f, Color::rgb(0xffffff));
            }
            else
            {
                list.rounded_rect(r, 20.0f, Color::rgb(0xffffff, 0.05f));
            }
            const bool active = i == columns[c].active;
            if (active)
            {
                list.line(r.x + 26.0f, r.cy() + 1.0f, r.x + 34.0f, r.cy() + 10.0f, 3.6f, theme::accent_soft);
                list.line(r.x + 34.0f, r.cy() + 10.0f, r.x + 50.0f, r.cy() - 9.0f, 3.6f, theme::accent_soft);
            }
            const hui::ui::FontRef &face = active ? env_.fonts.semibold : env_.fonts.regular;
            text(list, face, face.font->fit((*columns[c].tracks)[i], 27.0f, r.w - 110.0f), r.x + 70.0f, r.cy() + 9.0f, 27.0f,
                 active || selected ? theme::ink : theme::ink_2);
        }
        list.pop_opacity();
    }
    text(list, env_.fonts.regular, "Audio  " + state.audio_adjustments, panel.x+50.0f,panel.y+panel.h-90.0f,22.0f,theme::ink_2);
    text(list, env_.fonts.regular, "L2/R2 sync · L1/R1 volume · Triangle reset",panel.x+50.0f,panel.y+panel.h-44.0f,20.0f,theme::ink_3);
    text(list, env_.fonts.regular, "Subtitle delay  " + (state.subtitle_delay.empty() ? std::string("0.00 s") : state.subtitle_delay) + "   (L2 / R2 · Triangle reset)",
         panel.x + 710.0f, panel.y + panel.h - 44.0f, 24.0f, theme::ink_3);
    draw_chip(list, env_, hui::ui::Button::circle, "Close", panel.x + panel.w - 200.0f, panel.y + 30.0f);
    list.pop_transform();
    list.pop_opacity();
}

void PlayerUi::draw(hui::gfx::DrawList &list) const
{
    const float transport = controls_.value * (1.0f - menu_.value);
    if (transport > 0.01f)
        draw_controls(list, transport);
    if (buffering_.value < 0.5f && menu_.value < 0.5f)
        draw_subtitle(list);
    if (paused_.value > 0.01f && menu_.value < 0.5f)
    {
        list.push_opacity(paused_.value * (1.0f - menu_.value));
        const float cx = theme::kScreenW * 0.5f, cy = 470.0f;
        list.shadow({cx - 90, cy - 70, 180, 180}, 90, 40, Color::rgb(0x000000, 0.4f));
        list.circle(cx, cy, 90.0f, Color::rgb(0x000000, 0.55f));
        list.ring(cx, cy, 89.0f, 2.0f, Color::rgb(0xffffff, 0.22f));
        list.rounded_rect({cx - 34.0f, cy - 36.0f, 20.0f, 72.0f}, 7.0f, Color::rgb(0xffffff, 0.96f));
        list.rounded_rect({cx + 14.0f, cy - 36.0f, 20.0f, 72.0f}, 7.0f, Color::rgb(0xffffff, 0.96f));
        list.pop_opacity();
    }
    if (buffering_.value > 0.01f)
        draw_buffering(list, buffering_.value);
    if (!state.menu && state.buffering < 0 && (!state.skip_label.empty() || !state.next_label.empty()))
    {
        float x = 1780.0f;
        const auto button = [&](const std::string& label, const std::string& status, int action, float width) {
            x -= width;
            const bool focused = state.episode_focused && state.episode_action == action;
            const Rect panel{x, 652.0f, width, status.empty() ? 94.0f : 136.0f};
            list.shadow(panel, 24, 20, Color::rgb(0, 0.3f));
            list.bordered_rect(panel, 22, Color::rgb(0x101723, 0.88f), focused ? 3.0f : 1.5f,
                               focused ? theme::accent_soft : Color::rgb(0xffffff, 0.22f));
            text(list, env_.fonts.semibold, env_.fonts.semibold.font->fit(label, 30.0f, width - 50),
                 x + 25, panel.y + 43, 30, theme::ink);
            if (!status.empty()) text(list, env_.fonts.regular, env_.fonts.regular.font->fit(status, 23.0f, width - 50),
                                      x + 25, panel.y + 78, 23, theme::ink_2);
            if (focused)
            {
                hui::ui::HintLayout hints;
                hints.size = 28.0f;
                hints.text_size = 20.0f;
                hints.cy = panel.y + panel.h - 25;
                hints.item_gap = 24.0f;
                hints.font = &env_.fonts.regular;
                const hui::ui::Hint actions[] = {
                    {hui::ui::Button::cross, action == 0 ? "Skip" : "Play"},
                    {hui::ui::Button::circle, state.next_label.empty() ? "Back" : "Cancel next"}};
                hui::ui::draw_hints(list, env_.fonts, hui::ui::GlyphStyle::dark(), actions, 2,
                                    x + 25, false, hints);
            }
            else
                text(list, env_.fonts.regular, "Press Down to select", x + 25,
                     panel.y + panel.h - 17, 20, theme::ink_3);
            x -= 18;
        };
        if (!state.next_label.empty()) button(state.next_label, state.next_status, 1, 640);
        if (!state.skip_label.empty()) button(state.skip_label, "", 0, 430);
    }
    if (menu_.value > 0.01f)
        draw_menu(list, menu_.value);
}

} // namespace sx
