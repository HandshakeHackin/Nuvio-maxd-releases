// Nuvio on PS5: the design tokens every screen draws with.
//
// The identity is Nuvio's: near-black ground with violet light, the violet
// accent, white focus. Everything is laid out on the toolkit's 1920 x 1080
// virtual canvas (third_party/hui), which it scales to 1080p, 1440p or 4K.

#pragma once

#include "gfx/draw_list.hpp"

namespace sx::theme
{

using hui::gfx::Color;

inline Color ground = Color::rgb(0x0d0d0d);       // the page
inline Color accent = Color::rgb(0x78a7ff);       // Nuvio violet
inline Color accent_soft = Color::rgb(0xa4c4ff);  // violet text on dark
inline Color accent_deep = Color::rgb(0x243957);  // the glow behind the page
inline Color ink = Color::rgb(0xffffff, 0.94f);   // titles
inline Color ink_2 = Color::rgb(0xffffff, 0.72f); // body text
inline Color ink_3 = Color::rgb(0xffffff, 0.52f); // labels, 5.5:1 on the ground
inline Color surface = Color::rgb(0xffffff, 0.07f);
inline Color surface_hi = Color::rgb(0xffffff, 0.13f);
inline Color hairline = Color::rgb(0xffffff, 0.10f);
inline Color watched = Color::rgb(0x22b365);
inline Color imdb = Color::rgb(0xf5c518);

inline bool high_contrast = false;
inline void set_high_contrast(bool on) {
    high_contrast=on;
    ink=Color::rgb(0xffffff,on ? 1.0f : 0.94f);
    ink_2=Color::rgb(0xffffff,on ? 1.0f : 0.72f);
    ink_3=Color::rgb(0xffffff,on ? 0.92f : 0.52f);
    surface=Color::rgb(on ? 0x202020 : 0xffffff,on ? 1.0f : 0.07f);
    surface_hi=Color::rgb(on ? 0x444444 : 0xffffff,on ? 1.0f : 0.13f);
    hairline=Color::rgb(0xffffff,on ? 0.80f : 0.10f);
}

// Layout (virtual pixels).
constexpr float kRailWidth = 140.0f;  // the left navigation
constexpr float kLeft = 190.0f;       // where page content starts
constexpr float kRight = 1860.0f;     // where it ends
constexpr float kScreenW = 1920.0f;
constexpr float kScreenH = 1080.0f;

// Type sizes: nothing a person has to read is under 20.
constexpr float kLabel = 20.0f;
constexpr float kBody = 24.0f;
constexpr float kSubtitle = 28.0f;
constexpr float kHeading = 32.0f;

} // namespace sx::theme
