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
}
int main() {
    const struct { const char* name; void (*run)(); } tests[] = {
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
    std::printf("UI navigation: 6 tests, %d failures\n", failed);
    return failed ? 1 : 0;
}
