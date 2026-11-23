#include "audio/cues.hpp"
#include "navigation_shell_boundary.hpp"
#include <cassert>
#include <iostream>
using hui::Action;using hui::Direction;
static hui::InputFrame press(Action a){hui::InputFrame f;f.pressed=hui::action_bit(a);return f;}
int main(){
 sx::Shell shell;shell.top_focused_=true;
 for(int n=0;n<11;++n)assert(shell.route_top_navigation(press(Action::page_next)));
 assert(shell.page_==0); // visits every enabled section exactly once
 for(auto page:{sx::Page::livetv,sx::Page::torrents}){shell.go_page(int(page));assert(!shell.route_top_navigation(press(Action::page_prev)));shell.top_focused_=true;shell.top_page_=int(page);assert(shell.route_top_navigation(press(Action::page_prev))&&shell.top_focused_);}
 for(auto screen:{sx::Shell::Screen::detail,sx::Shell::Screen::person}){shell.screen_=screen;assert(!shell.route_top_navigation(press(Action::page_next)));}
 shell.go_page(0);shell.top_focused_=true;hui::InputFrame right;right.nav=Direction::right;assert(shell.route_top_navigation(right)&&shell.top_page_==10);
 assert(shell.route_top_navigation(press(Action::confirm))&&shell.screen_==sx::Shell::Screen::search&&shell.s_.searches==1);
 assert(shell.route_top_navigation(press(Action::page_next))&&shell.page_==1&&!shell.s_.search_open&&shell.s_.closes>0);
 shell.s_.iptv_visible=shell.s_.media_visible=shell.s_.torrents_visible=shell.s_.sports_visible=false;shell.go_page(5);
 shell.top_focused_=true;shell.top_page_=5;shell.route_top_navigation(press(Action::page_next));assert(shell.page_==3); // disabled tabs skipped
 shell.s_.top_navigation=false;assert(!shell.route_top_navigation(press(Action::page_next))&&!shell.top_focused_);
 shell.s_.top_navigation=true;shell.go_page(10);shell.route_search(press(Action::back),.016f);assert(shell.page_==0&&!shell.s_.search_open);
 shell.go_page(2);shell.screen_=sx::Shell::Screen::search;shell.s_.search_open=true;shell.route_search(press(Action::back),.016f);assert(shell.page_==2&&!shell.s_.search_open);
 shell.go_page(10);shell.top_focused_=true;shell.top_page_=10;shell.route_top_navigation(press(Action::back));assert(shell.page_==0&&!shell.s_.search_open);
 std::cout<<"Production shell navigation: complete tab traversal, Search activation, hidden sections, result cleanup and Live TV/torrent/detail shortcut preservation passed\n";
}
