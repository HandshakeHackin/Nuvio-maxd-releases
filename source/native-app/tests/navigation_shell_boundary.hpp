#pragma once
#include "top_navigation.hpp"
#include "core/input.hpp"
namespace sx {
struct NavSession {
 bool top_navigation=true,iptv_visible=true,media_visible=true,torrents_visible=true,sports_visible=true,search_open=false;
 int searches=0,closes=0;struct {std::vector<int> rows;}search;
 void close_search(){search_open=false;++closes;}void begin_search(){++searches;}
};
class Shell {
public:
 enum class Screen {page,search,detail,person};
 Screen screen_=Screen::page;NavSession s_;int page_=0,top_page_=0;bool top_focused_=false;
 struct {void play(hui::audio::Cue){}}sound_;
 void go_page(int page){page_=page;screen_=page==10?Screen::search:Screen::page;top_focused_=false;top_page_=page;if(page==10)s_.search_open=true;}
 bool route_top_navigation(const hui::InputFrame&);
 void route_search(const hui::InputFrame&,float);
 struct SearchBoard {
   bool focused=false;
   void update(const hui::InputFrame&,float){}
   bool rail_focused()const{return focused;}
   int rail_cursor()const{return 0;}
   const Title* focused_title()const{return nullptr;}
 }search_board_;
 void open_title(const Title&){}
};
}
