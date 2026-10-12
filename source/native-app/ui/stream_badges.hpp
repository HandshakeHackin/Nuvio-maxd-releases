// Colored vector badges stay sharp at 1080p and 4K, without image downloads.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cards.hpp"

namespace sx
{
inline void stream_file_icon(hui::gfx::DrawList &list, float x, float y, hui::gfx::Color ink)
{
    list.bordered_rect({x,y,18,20}, 2, hui::gfx::Color::rgb(0xffffff,0), 1.8f, ink);
    list.line(x+5,y+5,x+13,y+5,1.5f,ink); list.line(x+5,y+9,x+13,y+9,1.5f,ink);
    list.line(x+5,y+13,x+10,y+13,1.5f,ink);
}
inline void stream_globe_icon(hui::gfx::DrawList &list, float x, float cy, hui::gfx::Color ink)
{
    list.ring(x+10,cy,9,1.6f,ink);
    list.bordered_rect({x+6,cy-9,8,18},4,hui::gfx::Color::rgb(0xffffff,0),1.3f,ink);
    list.line(x+1,cy,x+19,cy,1.3f,ink);
}
inline float draw_stream_badge(hui::gfx::DrawList &list, const Env &env, StreamBadge badge, float x, float cy,
                               bool measure_only = false)
{
    using hui::gfx::Color; using hui::gfx::Align; using hui::ui::text;
    using B = StreamBadge;
    const char *label = ""; Color ink = theme::ink;
    bool quality = false, speaker = false, globe = false, dolby = false;
    switch (badge)
    {
    case B::web_dl: label="WEB-DL"; ink=Color::rgb(0x43adf5); globe=true; break;
    case B::web_rip: label="WEBRip"; ink=Color::rgb(0x43adf5); globe=true; break;
    case B::bluray: label="BluRay"; ink=Color::rgb(0x86baff); break;
    case B::remux: label="REMUX"; ink=Color::rgb(0x86baff); break;
    case B::uhd: label="4K"; ink=Color::rgb(0xf5c65d); quality=true; break;
    case B::full_hd: label="1080p"; ink=Color::rgb(0xf5c65d); quality=true; break;
    case B::hd: label="720p"; ink=Color::rgb(0xf5c65d); quality=true; break;
    case B::dolby_vision: label="Vision"; dolby=true; break;
    case B::hdr10_plus: label="HDR10+"; ink=Color::rgb(0xf5c65d); break;
    case B::hdr: label="HDR"; ink=Color::rgb(0xf5c65d); break;
    case B::sdr: label="SDR"; break;
    case B::hevc: label="HEVC"; ink=Color::rgb(0x84c94b); break;
    case B::avc: label="H.264"; ink=Color::rgb(0x84c94b); break;
    case B::av1: label="AV1"; ink=Color::rgb(0x84c94b); break;
    case B::atmos: label="Atmos"; break;
    case B::truehd: label="TrueHD"; break;
    case B::dolby_plus: label="DD+"; dolby=true; break;
    case B::dolby: label="Dolby"; dolby=true; break;
    case B::dts_hd: label="DTS-HD"; break;
    case B::dts: label="DTS"; break;
    case B::channels_71: label="7.1"; speaker=true; break;
    case B::channels_51: label="5.1"; speaker=true; break;
    case B::stereo: label="2.0"; speaker=true; break;
    }
    const float label_w = env.fonts.semibold.measure(label,22.0f);
    const float icon_w = dolby ? 37.0f : (speaker || globe) ? 27.0f : 0.0f;
    const float w = label_w + icon_w + (quality ? 18.0f : 0.0f);
    if (measure_only) return w;
    if (quality)
    {
        list.bordered_rect({x,cy-17,w,34},6,Color::rgb(0xf5c65d,0.09f),1.5f,ink);
        text(list,env.fonts.semibold,label,x+w/2,cy+7,22,ink,Align::center);
    }
    else
    {
        if (globe) stream_globe_icon(list,x,cy,ink);
        if (dolby)
        {
            list.rounded_rect({x,cy-11,29,22},2,ink);
            const Color cut = Color::rgb(0x1b1b1b);
            list.arc(x+2,cy,8,5,0,3.14159f,cut,false);
            list.arc(x+27,cy,8,5,3.14159f,3.14159f,cut,false);
        }
        if (speaker)
        {
            const auto blue = Color::rgb(0x43adf5);
            const float p[]{x,cy-4,x+5,cy-4,x+12,cy-10,x+12,cy+10,x+5,cy+4,x,cy+4};
            list.polygon(p,6,blue);
            list.arc(x+10,cy,9,1.6f,0.65f,1.8f,blue);
            list.arc(x+10,cy,14,1.6f,0.65f,1.8f,blue);
        }
        text(list,env.fonts.semibold,label,x+icon_w,cy+7,22,ink);
    }
    return w;
}
inline void draw_stream_badges(hui::gfx::DrawList &list, const Env &env, const Stream &stream,
                               float x, float cy, float width)
{
    const float start = x;
    for (StreamBadge badge : stream.badges)
    {
        // The extra line is available for unusually long combinations.
        const float w = draw_stream_badge(list,env,badge,0,0,true);
        if (x + w > start + width) { x = start; cy += 40; }
        x += draw_stream_badge(list,env,badge,x,cy) + 20;
    }
}
} // namespace sx
