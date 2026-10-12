// What is drawn over the video: the controls, the paused and buffering
// states, subtitles and the track menu. The picture itself is the player's.

#pragma once

#include "chrome.hpp"
#include "content.hpp"

namespace sx
{

class PlayerUi
{
  public:
    explicit PlayerUi(const Env &env) : env_(env)
    {
    }

    PlayerState state; // the app keeps this up to date

    void snap();       // jump every animation to where the state wants it
    void update(float dt);
    void draw(hui::gfx::DrawList &list) const;

  private:
    void draw_controls(hui::gfx::DrawList &list, float amount) const;
    void draw_menu(hui::gfx::DrawList &list, float amount) const;
    void draw_buffering(hui::gfx::DrawList &list, float amount) const;
    void draw_subtitle(hui::gfx::DrawList &list) const;

    const Env &env_;
    hui::tween::Spring controls_;
    hui::tween::Spring menu_;
    hui::tween::Spring paused_;
    hui::tween::Spring buffering_;
    float clock_ = 0.0f;
};

// Shared renderer for playback and the live settings preview.
void draw_subtitle_block(hui::gfx::DrawList&, const hui::ui::Fonts&, const std::string&,
                         const SubtitleStyle&, float size, float centre, float bottom, float width);

// Text with a dark outline, for subtitles over any picture.
void draw_outlined_text(hui::gfx::DrawList &list, const hui::ui::FontRef &font, const std::string &value, float x,
                        float baseline, float size, hui::gfx::Color colour, hui::gfx::Align align);

} // namespace sx
