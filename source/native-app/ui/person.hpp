// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "page.hpp"
#include <algorithm>
#include "../src/people.h"
namespace sx {
struct PersonContent {
    people::Person data;
    std::uint32_t photo=0;
    std::vector<std::uint32_t> movies,series;
    bool loading=false,resolving=false;
    std::string status;
};
class PersonPage {
public:
    struct Focus {int row=0,movie=0,series=0;};
    explicit PersonPage(const Env& env):env_(env){}
    void set_content(const PersonContent* c){content_=c;focus_={};scroll_[0].position.snap(0);scroll_[1].position.snap(0);refresh();}
    void refresh();
    void update(const hui::InputFrame&,float);
    void draw(hui::gfx::DrawList&,hui::gfx::BackdropSpec&)const;
    Focus focus()const{return focus_;}
    void restore(Focus f){focus_=f;refresh();scroll_[0].position.snap(std::max(0,focus_.movie*206-980));scroll_[1].position.snap(std::max(0,focus_.series*206-980));}
    const people::Credit* selected()const;
private:
    const Env& env_;const PersonContent* content_=nullptr;Focus focus_;
    hui::ui::Scroller scroll_[2];
};
}
