#include "search_keyboard.hpp"
#include "top_navigation.hpp"
#include "search_suggestions.hpp"
#include <cassert>
#include <iostream>
using namespace sx;
static hui::InputFrame press(hui::Action a){hui::InputFrame f;f.pressed=hui::action_bit(a);return f;}
static void type(search_keyboard::State& s,std::u32string_view text){for(auto c:text)s.append(c);}
int main(){
 search_keyboard::State s;
 assert(s.input(press(hui::Action::confirm))==search_keyboard::Event::changed&&s.text=="1");
 s.row=int(s.rows().size());s.col=0;s.input(press(hui::Action::confirm));assert(s.text.empty());
 s.text="Dune";s.suggestions={{"tt1","movie","Dune: Part Two","2024"}};s.suggestion=0;
 assert(s.input(press(hui::Action::confirm))==search_keyboard::Event::submit&&s.text=="Dune: Part Two");
 for(const auto& lang:search_keyboard::languages){s={};s.language=search_keyboard::language_index(lang.code);type(s,lang.lower);assert(!s.text.empty());assert(search_keyboard::decode(s.text).size()<=128);for(int n=0;n<100;++n){s.cycle(1);s.clamp();}}
 s={};s.language=search_keyboard::language_index("ko");type(s,U"ㅎㅏㄴㄱㅡㄹ");assert(s.text=="한글");s.erase();assert(s.text=="한그");s.erase();assert(s.text=="한ㄱ");s.erase();assert(s.text=="한");
 s.text.clear();type(s,U"ㄱㅗㅏ");assert(s.text=="과");s.erase();assert(s.text=="고");
 s.text.clear();type(s,U"ㄱㅏㅂㅅㅏ");assert(s.text=="갑사");
 s.text.clear();type(s,U"ㄱㅗㅏㄴㄱㅖ");assert(s.text=="관계");
 for(auto text:{"שלום","العربية","ภาษาไทย","にほんご","中文","Amélie"})assert(search_keyboard::bounded(text)==text);
 for(auto bad:{std::string("bad\nquery"),std::string("\xc0\xaf"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),std::string("x\xe2\x80\xae")})assert(search_keyboard::bounded(bad).empty());
 s={};for(int n=0;n<129;++n)s.append(U'한');assert(s.text.size()==384);s.erase();assert(search_keyboard::decode(s.text).size()==127);
 s.text="A😀";s.erase();assert(s.text=="A");
 assert(s.input(press(hui::Action::menu))==search_keyboard::Event::system);assert(s.input(press(hui::Action::back))==search_keyboard::Event::cancel);
 const auto language=s.language;s.input(press(hui::Action::page_prev));s.input(press(hui::Action::page_next));assert(language==s.language);
 for(int flags=0;flags<16;++flags){const auto tabs=top_navigation::tabs(flags&1,flags&2,flags&4,flags&8);assert(tabs.front().page==Page::board&&tabs.back().page==Page::settings);assert(tabs[1].page==Page::search);for(int n=0;n<int(tabs.size());++n){auto w=top_navigation::window(int(tabs.size()),n);assert(n>=w.first&&n<w.first+w.count&&w.count<=7);assert(top_navigation::index(tabs,tabs[n].page)==n);}}
 assert(!top_navigation::shoulders(false,true,true,Page::board));assert(!top_navigation::shoulders(true,false,true,Page::board));
 for(auto page:{Page::livetv,Page::torrents}){assert(!top_navigation::shoulders(true,true,false,page));assert(top_navigation::shoulders(true,true,true,page));}
 assert(top_navigation::shoulders(true,true,false,Page::search));
 const auto items=search_suggestions::parse(R"({"metas":[{"id":"tt1","type":"movie","name":"שלום","releaseInfo":"2026"},{"id":"tt1","type":"movie","name":"duplicate"},{"id":"tt1","type":"series","name":"different type"},{"id":"x","type":"movie","name":"bad\nname"}]})");
 assert(items.size()==2&&items[0].name=="שלום"&&items[1].type=="series");
 assert(search_suggestions::parse("not json").empty());assert(search_suggestions::parse(std::string(search_suggestions::max_response+1,'x')).empty());
 std::string deep(50,'[');deep+=std::string(50,']');assert(search_suggestions::parse(deep).empty());
 std::vector<std::vector<search_keyboard::Suggestion>> sources(8);for(int i=0;i<8;++i)for(int n=0;n<10;++n)sources[i].push_back({std::to_string(i*10+n),"movie","fixture",""});
 auto merged=search_suggestions::merge(sources);assert(merged.size()==6&&merged[1].id=="10");
 bool requested=false;std::atomic<bool> cancel{false};
 auto request=[&](const feature_http::Query& q,const std::atomic<bool>* c){assert(q.limit==512u<<10&&q.timeout_ms==4000&&!q.https_only&&c==&cancel);assert(q.url=="https://configured.invalid/path/catalog/movie/top/search=hello.json?token=fixture");requested=true;feature_http::Response r;r.status=200;r.body=R"({"metas":[{"id":"a","type":"movie","name":"Hello"}]})";return r;};
 auto result=search_suggestions::fetch("https://configured.invalid/path/catalog/movie/top/search=hello.json?token=fixture",&cancel,request);assert(requested&&result.items.size()==1&&!result.failed);
 requested=false;cancel=true;assert(search_suggestions::fetch("unused",&cancel,request).items.empty()&&!requested);
 std::cout<<"Search/navigation: Unicode editing, Hangul composition, keyboard controls, all enabled tabs, shortcut ownership, bounded catalogue suggestions, cancellation and request limits passed\n";
}
