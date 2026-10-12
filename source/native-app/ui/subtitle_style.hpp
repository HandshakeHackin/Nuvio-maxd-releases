#pragma once
namespace sx {
struct SubtitleStyle {
    int font = 1; // 0 regular, 1 semibold, 2 display; all retain Unicode fallbacks
    int colour = 0; // white, yellow, cyan
    int effect = 1; // plain, outline, shadow
    int background = 0; // opacity percent
};
}
