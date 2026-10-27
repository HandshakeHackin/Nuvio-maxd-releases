// Exercise production page input with catalogs still loading or absent.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "board.hpp"
#include "addons.hpp"
#include <cstdio>
#include <stdexcept>

namespace {
struct Fixture {
    hui::ui::Fonts fonts;
    sx::Icons icons;
    sx::Env env{fonts, icons};
};
void expect(bool ok, const char* reason) {
    if (!ok) throw std::runtime_error(reason);
}
template <typename Page> void move(Page& page, hui::Direction direction) {
    hui::InputFrame input;
    input.connected = true; input.nav = direction;
    page.update(input, 1.0f / 60.0f);
}
void home_without_catalogs() {
    Fixture f; sx::Board board(f.env); sx::BoardContent content;
    content.status = "Loading your catalogs...";
    board.set_content(&content); board.enter();
    move(board, hui::Direction::left);
    expect(board.rail_focused(), "empty home ignores the navigation menu");
    for (int i = 0; i < 4; ++i) move(board, hui::Direction::down);
    expect(board.rail_cursor() == 4, "cannot reach Settings while catalogs load");
}
void home_reload_and_arrival() {
    Fixture f; sx::Board board(f.env); sx::BoardContent content;
    sx::Row row; row.key = "catalog"; row.items.resize(3);
    content.rows.push_back(row); board.set_content(&content); board.enter();
    content.rows.clear(); board.refresh();
    move(board, hui::Direction::left);
    expect(board.rail_focused(), "catalog reload traps the cursor");
    move(board, hui::Direction::down);
    content.rows.push_back(row); board.refresh();
    move(board, hui::Direction::none);
    expect(board.rail_focused() && board.rail_cursor() == 1, "catalog arrival steals menu focus");
    move(board, hui::Direction::right);
    expect(!board.rail_focused(), "cannot return to loaded cards");
    move(board, hui::Direction::right);
    expect(board.cursor_col() == 1, "loaded cards cannot be selected");
}
void home_empty_frames_keep_menu_selection() {
    Fixture f; sx::Board board(f.env); sx::BoardContent content;
    board.set_content(&content); board.enter();
    move(board, hui::Direction::left); move(board, hui::Direction::down);
    move(board, hui::Direction::down);
    for (int i = 0; i < 600; ++i) move(board, hui::Direction::none);
    expect(board.rail_focused() && board.rail_cursor() == 2, "empty frames reset or lose menu focus");
    move(board, hui::Direction::right);
    expect(board.rail_focused() && board.rail_cursor() == 2, "empty content traps right navigation");
}
void empty_addons_can_reach_settings() {
    Fixture f; sx::Addons addons(f.env); sx::AddonsContent content;
    addons.set_content(&content); addons.enter();
    move(addons, hui::Direction::left); move(addons, hui::Direction::down);
    expect(addons.rail_focused() && addons.rail_cursor() == 4, "empty Addons page traps navigation");
}
void addons_arrival_keeps_menu_selection() {
    Fixture f; sx::Addons addons(f.env); sx::AddonsContent content;
    addons.set_content(&content); addons.enter();
    move(addons, hui::Direction::left); move(addons, hui::Direction::down);
    for (int i = 0; i < 600; ++i) move(addons, hui::Direction::none);
    content.items.resize(4); addons.refresh();
    move(addons, hui::Direction::none);
    expect(addons.rail_focused() && addons.rail_cursor() == 4, "addon arrival steals menu focus");
    move(addons, hui::Direction::right); move(addons, hui::Direction::right);
    expect(!addons.rail_focused() && addons.cursor_index() == 1, "loaded addons cannot be selected");
}
void populated_pages_keep_normal_navigation() {
    Fixture f; sx::Board board(f.env); sx::BoardContent content;
    sx::Row row; row.key = "catalog"; row.items.resize(3);
    content.rows.push_back(row); board.set_content(&content); board.enter();
    move(board, hui::Direction::right);
    expect(!board.rail_focused() && board.cursor_col() == 1, "home card navigation changed");
    sx::Addons addons(f.env); sx::AddonsContent installed; installed.items.resize(4);
    addons.set_content(&installed); addons.enter();
    move(addons, hui::Direction::right);
    expect(!addons.rail_focused() && addons.cursor_index() == 1, "addon tile navigation changed");
}
void live_tv_rail_and_channel_arrival() {
    Fixture f; sx::Board board(f.env); sx::BoardContent home;
    board.set_content(&home); board.enter(); move(board,hui::Direction::left);
    for(int i=0;i<5;++i)move(board,hui::Direction::down);
    expect(board.rail_cursor()==5,"Live TV tab cannot be reached from Home");
    sx::Addons tv(f.env,sx::Page::livetv); sx::AddonsContent content;
    content.items.resize(1); tv.set_content(&content); tv.enter();
    expect(!tv.rail_focused() && tv.cursor_index()==0,"source management unavailable without channels");
    content.items.resize(10);tv.refresh();move(tv,hui::Direction::right);
    expect(tv.cursor_index()==1,"cannot select an arriving live channel");
    content.items.resize(1);tv.refresh();
    expect(tv.cursor_index()==0,"source/profile change leaves an invalid channel selected");
    move(tv,hui::Direction::left);move(tv,hui::Direction::up);
    expect(tv.rail_focused() && tv.rail_cursor()==4,"cannot return to Settings from Live TV");
}

void iptv_visibility_changes() {
    Fixture f;bool visible=false;f.env.iptv_visible=&visible;
    sx::Board b(f.env);sx::BoardContent empty;b.set_content(&empty);b.enter();move(b,hui::Direction::left);
    for(int i=0;i<10;++i)move(b,hui::Direction::down);
    expect(b.rail_cursor()==5,"hidden IPTV tab receives focus");
    visible=true;move(b,hui::Direction::down);expect(b.rail_cursor()==6,"enabled IPTV tab unreachable");
    visible=false;move(b,hui::Direction::none);expect(b.rail_cursor()==0,"removed IPTV tab retains invalid focus");
    move(b,hui::Direction::down);expect(b.rail_cursor()==1,"existing rail changed after hiding IPTV tab");
}

}
int main() {
    const struct { const char* name; void (*run)(); } tests[] = {
        {"iptv_visibility_changes",iptv_visibility_changes},
        {"live_tv_rail_and_channel_arrival",live_tv_rail_and_channel_arrival},
        {"home_without_catalogs", home_without_catalogs},
        {"home_reload_and_arrival", home_reload_and_arrival},
        {"home_empty_frames_keep_menu_selection", home_empty_frames_keep_menu_selection},
        {"empty_addons_can_reach_settings", empty_addons_can_reach_settings},
        {"addons_arrival_keeps_menu_selection", addons_arrival_keeps_menu_selection},
        {"populated_pages_keep_normal_navigation", populated_pages_keep_normal_navigation},
    };
    int failed = 0;
    for (const auto& test : tests) {
        try { test.run(); std::printf("%s: PASS\n", test.name); }
        catch (const std::exception& e) { ++failed; std::printf("%s: FAIL (%s)\n", test.name, e.what()); }
    }
    std::printf("UI navigation: 8 tests, %d failures\n", failed);
    return failed ? 1 : 0;
}
