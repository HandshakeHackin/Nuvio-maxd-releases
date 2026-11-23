// A title's page: a full-width season and episode carousel for series,
// or artwork and metadata beside the available streams.

#pragma once

#include "cards.hpp"
#include <algorithm>
#include "content.hpp"
#include "core/input.hpp"
#include "gfx/backdrop_spec.hpp"

namespace sx
{

class Detail
{
  public:
    enum class Mode
    {
        episodes,
        streams,
        people,
    };

    explicit Detail(const Env &env) : env_(env)
    {
    }

    void set_content(const DetailContent *content);
    // The content changed in place (details, episodes or streams arrived).
    void refresh();
    int episode_index() const
    {
        return seasons_focused_ ? -1 : episode_;
    }
    bool seasons_focused() const { return seasons_focused_; }
    int season_index() const { return seasons_focused_ ? season_cursor_ : -1; }
    int stream_index() const
    {
        return filters_focused_ || stream_ < 0 || stream_ >= static_cast<int>(visible_.size()) ? -1 : visible_[stream_];
    }
    bool source_options_focused() const { return filters_focused_ && filter_cursor_ == -2; }
    bool reload_focused() const { return filters_focused_ && filter_cursor_ == -1; }
    bool filters_focused() const { return filters_focused_; }
    int addon_filter() const { return filter_; }
    std::string stream_addon() const { return filter_ > 0 && filter_ < static_cast<int>(filters_.size()) ? filters_[filter_] : ""; }
    void enter();
    void update(const hui::InputFrame &input, float dt);
    void draw(hui::gfx::DrawList &list, hui::gfx::BackdropSpec &backdrop) const;

    Mode mode() const
    {
        return mode_;
    }
    void set_mode(Mode mode);
    struct Focus {Mode mode=Mode::streams,previous=Mode::streams;int cast=0,episode=0,season=0;};
    Focus focus()const{return {mode_,cast_previous_,cast_,episode_,season_cursor_};}
    void restore(Focus f){cast_=f.cast;episode_=f.episode;season_cursor_=f.season;cast_previous_=f.previous;cast_scroll_.position.snap(std::max(0,cast_*220-1500));set_mode(f.mode);}
    int cast_index()const{return cast_;}
    void toggle_cast(){if(mode_==Mode::people)set_mode(cast_previous_);else{cast_previous_=mode_;set_mode(Mode::people);}}

  private:
    friend struct DetailTestAccess;
    hui::gfx::Rect episode_rect(int index, bool final_position) const;
    hui::gfx::Rect season_rect(int index, bool final_position) const;
    float season_width(int index) const;
    hui::gfx::Rect stream_rect(int index, bool final_position) const;
    void draw_info(hui::gfx::DrawList &list) const;
    void draw_people(hui::gfx::DrawList&)const;
    void draw_episodes(hui::gfx::DrawList &list) const;
    void draw_streams(hui::gfx::DrawList &list) const;
    void draw_hints_row(hui::gfx::DrawList &list) const;
    void rebuild_streams();
    void select_filter(int index);
    hui::gfx::Rect filter_rect(int index, bool final_position) const;
    float filter_width(int index) const;

    const Env &env_;
    const DetailContent *content_ = nullptr;
    Mode mode_ = Mode::streams;
    Mode cast_previous_=Mode::streams;
    int cast_=0;
    hui::ui::Scroller cast_scroll_;
    int streams_serial_ = 0;
    bool streams_wanted_ = false; // the user asked for an episode's streams
    int hint_serial_ = 0;
    int episode_ = 0;
    int season_cursor_ = 0;
    int shown_season_ = 0;
    bool seasons_focused_ = false;
    hui::ui::Scroller season_scroll_;
    int stream_ = 0;
    std::vector<std::string> filters_; // All, then actual addon names
    std::vector<int> visible_;        // visible row -> original playable stream
    int filter_ = 0;
    int filter_cursor_ = 0;           // -1 is Reload
    bool filters_focused_ = false;
    std::string selected_identity_;
    hui::ui::Scroller filter_scroll_;
    hui::ui::Scroller episode_scroll_;
    hui::ui::Scroller stream_scroll_;
    hui::ui::SpringRect ring_;
    hui::tween::Timer swap_; // the panel's content changing
    float age_ = 0.0f;
    float clock_ = 0.0f;
};

} // namespace sx
