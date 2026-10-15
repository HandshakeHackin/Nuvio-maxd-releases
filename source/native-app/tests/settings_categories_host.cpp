#include "settings.hpp"
#include <cassert>
#include <cstdio>
namespace {
void press(sx::Settings& page,hui::Action action) {
    hui::InputFrame in;in.connected=true;in.pressed=hui::action_bit(action);page.update(in,1.0f/60);
}
void move(sx::Settings& page,hui::Direction direction) {
    hui::InputFrame in;in.connected=true;in.nav=direction;page.update(in,1.0f/60);
}
}
int main() {
    sx::SettingsContent content;
    for(const auto* group : {"Video","Audio","Video","Appearance & Lighting","Audio"}) {
        sx::SettingRow row;row.category=group;row.label="Setting "+std::to_string(content.rows.size());content.rows.push_back(row);
    }
    sx::SettingsNavigation nav;nav.refresh(content);
    assert(nav.categories.size()==3 && nav.source_row()==-1);
    assert(nav.open(content) && nav.category=="Video" && nav.rows==std::vector<int>({0,2}));
    nav.index=1;nav.refresh(content);assert(nav.source_row()==2);
    assert(nav.back(content) && nav.index==0 && nav.root());
    nav.index=1;assert(nav.open(content));assert(nav.source_row()==1);
    nav.index=1;assert(nav.source_row()==4);
    assert(nav.category_row(content,1).value=="2 settings");
    assert(nav.back(content) && nav.index==1);
    nav.open(content);content.rows[4].on=true;nav.refresh(content);assert(nav.source_row()==1);
    sx::SettingsContent empty;nav.refresh(empty);assert(nav.root() && nav.count()==0);

    hui::ui::Fonts fonts;sx::Icons icons;sx::Env env{fonts,icons};
    sx::Settings page(env);page.set_content(&content);page.enter();
    assert(page.cursor_row()<0);
    press(page,hui::Action::confirm);assert(page.navigation_consumed() && page.cursor_row()==0);
    move(page,hui::Direction::none);assert(page.cursor_row()==0);
    move(page,hui::Direction::down);assert(page.cursor_row()==2);
    page.refresh();assert(page.cursor_row()==2); // async settings refresh must not move focus
    press(page,hui::Action::back);assert(page.navigation_consumed() && page.cursor_row()<0);
    move(page,hui::Direction::down);press(page,hui::Action::confirm);
    move(page,hui::Direction::none);assert(page.cursor_row()==1);
    move(page,hui::Direction::left);assert(page.rail_focused() && page.cursor_row()<0);
    move(page,hui::Direction::right);assert(!page.rail_focused() && page.cursor_row()==1);
    std::puts("Settings categories: source IDs, Cross/Circle navigation, async refresh, parent focus and rail access passed");
}
