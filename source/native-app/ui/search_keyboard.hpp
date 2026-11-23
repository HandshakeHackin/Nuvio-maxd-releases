// Search text only. PINs, passwords and other secret fields never enter this model.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/input.hpp"
#include "search_languages.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace sx::search_keyboard {
constexpr std::size_t max_bytes=512, max_characters=128, max_suggestions=6;
inline std::string utf8(char32_t c) {
 std::string s;
 if(c<0x80)s+=char(c);
 else if(c<0x800){s+=char(0xc0|(c>>6));s+=char(0x80|(c&63));}
 else if(c<0x10000){s+=char(0xe0|(c>>12));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}
 else {s+=char(0xf0|(c>>18));s+=char(0x80|((c>>12)&63));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}
 return s;
}
inline std::u32string decode(std::string_view input) {
 std::u32string out;
 for(std::size_t i=0;i<input.size();) {
  unsigned char c=input[i++];char32_t cp=c;int count=c<128?0:(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:(c&0xf8)==0xf0?3:-1;
  if(count<0||i+std::size_t(count)>input.size())return {};
  cp&=count==0?127:count==1?31:count==2?15:7;
  for(int n=0;n<count;++n){unsigned char d=input[i++];if((d&0xc0)!=0x80)return {};cp=(cp<<6)|(d&63);}
  if((count==1&&cp<0x80)||(count==2&&cp<0x800)||(count==3&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return {};
  // Reject controls/directional overrides; keep normal RTL letters and combining marks.
  if(cp<32||cp==127||(cp>=0x202a&&cp<=0x202e)||(cp>=0x2066&&cp<=0x2069))return {};
  out+=cp;
 }
 return out;
}
inline std::string bounded(std::string_view text) {
 const auto points=decode(text);std::string out;std::size_t count=0;
 for(char32_t c:points){auto s=utf8(c);if(++count>max_characters||out.size()+s.size()>max_bytes)break;out+=s;}
 return out;
}
// Standard Hangul composition, including compound vowels/finals and resyllabification.
inline constexpr std::u32string_view initials=U"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ";
inline constexpr std::u32string_view vowels=U"ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ";
inline constexpr std::u32string_view finals{U"\0ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ",28};
struct Pair {char32_t a,b,result;};
inline constexpr Pair pairs[]={
 {U'ㅗ',U'ㅏ',U'ㅘ'},{U'ㅗ',U'ㅐ',U'ㅙ'},{U'ㅗ',U'ㅣ',U'ㅚ'},{U'ㅜ',U'ㅓ',U'ㅝ'},{U'ㅜ',U'ㅔ',U'ㅞ'},{U'ㅜ',U'ㅣ',U'ㅟ'},{U'ㅡ',U'ㅣ',U'ㅢ'},
 {U'ㄱ',U'ㅅ',U'ㄳ'},{U'ㄴ',U'ㅈ',U'ㄵ'},{U'ㄴ',U'ㅎ',U'ㄶ'},{U'ㄹ',U'ㄱ',U'ㄺ'},{U'ㄹ',U'ㅁ',U'ㄻ'},{U'ㄹ',U'ㅂ',U'ㄼ'},{U'ㄹ',U'ㅅ',U'ㄽ'},{U'ㄹ',U'ㅌ',U'ㄾ'},{U'ㄹ',U'ㅍ',U'ㄿ'},{U'ㄹ',U'ㅎ',U'ㅀ'},{U'ㅂ',U'ㅅ',U'ㅄ'}
};
inline char32_t combine(char32_t a,char32_t b){for(const auto& p:pairs)if(a==p.a&&b==p.b)return p.result;return 0;}
inline void compose(std::u32string& s,char32_t c) {
 if(s.empty()){s+=c;return;}auto& last=s.back();auto v=vowels.find(c);
 if(auto l=initials.find(last);l!=initials.npos&&v!=vowels.npos){last=0xac00+(l*21+v)*28;return;}
 if(last>=0xac00&&last<=0xd7a3){
  auto n=last-0xac00,l=n/588,oldv=(n%588)/28,t=n%28;
  if(v!=vowels.npos){
   if(!t){if(auto both=combine(vowels[oldv],c)){last=0xac00+(l*21+vowels.find(both))*28;return;}}
   else {char32_t move=finals[t],keep=0;for(const auto& p:pairs)if(p.result==move){keep=p.a;move=p.b;break;}
    if(auto li=initials.find(move);li!=initials.npos){last=0xac00+(l*21+oldv)*28+(keep?finals.find(keep):0);s+=0xac00+(li*21+v)*28;return;}}
  }else if(auto f=finals.find(c);f!=finals.npos&&f){
   if(!t){last+=f;return;}
   if(auto both=combine(finals[t],c)){last=last-t+finals.find(both);return;}
  }
 }
 s+=c;
}
struct Suggestion {std::string id,type,name,year;};
enum class Event {none,changed,language,submit,system,cancel};
struct State {
 std::string title,text,status;
 std::size_t language=0;
 bool shift=false, suggestions_enabled=false;
 int row=0,col=0,suggestion=-1;
 std::vector<Suggestion> suggestions;
 std::vector<std::u32string> rows()const {
  std::u32string chars=U"1234567890-.";
  chars+=shift?languages[language].upper:languages[language].lower;
  chars+=U"'!?&:()/+";
  std::vector<std::u32string> result;
  for(std::size_t n=0;n<chars.size();n+=12)result.emplace_back(chars.substr(n,12));
  return result;
 }
 void clamp(){auto keys=rows();row=std::clamp(row,0,int(keys.size()));col=std::clamp(col,0,row==int(keys.size())?5:int(keys[row].size())-1);if(suggestions.empty())suggestion=-1;else suggestion=std::min(suggestion,int(suggestions.size())-1);}
 void cycle(int delta){language=(language+std::size(languages)+delta)%std::size(languages);clamp();}
 void append(char32_t c){auto points=decode(text);if(std::string_view(languages[language].code)=="ko")compose(points,c);else points+=c;std::string out;for(auto cp:points)out+=utf8(cp);if(out.size()<=max_bytes&&points.size()<=max_characters)text=std::move(out);}
 void erase(){auto points=decode(text);if(points.empty())return;auto& c=points.back();
  if(std::string_view(languages[language].code)=="ko"&&c>=0xac00&&c<=0xd7a3){auto n=c-0xac00,t=n%28,v=(n%588)/28;
   if(t){char32_t keep=0;for(const auto& p:pairs)if(p.result==finals[t])keep=p.a;c=c-t+(keep?finals.find(keep):0);}
   else {char32_t keep=0;for(const auto& p:pairs)if(p.result==vowels[v])keep=p.a;if(keep)c=c-v*28+vowels.find(keep)*28;else c=initials[n/588];}}
  else {
   points.pop_back();
  }
  text.clear();
  for(auto cp:points)text+=utf8(cp);
 }
 Event input(const hui::InputFrame& in){
  using hui::Action;using hui::Direction;clamp();
  if(in.is_pressed(Action::back))return Event::cancel;
  if(in.is_pressed(Action::menu))return Event::system;
  if(in.is_pressed(Action::north))return Event::submit;
  if(in.is_pressed(Action::page_prev)||in.is_pressed(Action::page_next)){cycle(in.is_pressed(Action::page_prev)?-1:1);return Event::language;}
  if(in.is_pressed(Action::west)){shift=!shift;clamp();return Event::none;}
  const auto keys=rows();
  if(suggestion>=0){
   if(in.nav==Direction::left)suggestion=-1;
   else if(in.nav==Direction::up)suggestion=std::max(0,suggestion-1);
   else if(in.nav==Direction::down)suggestion=std::min(int(suggestions.size())-1,suggestion+1);
   if(in.is_pressed(Action::confirm)&&suggestion>=0){text=bounded(suggestions[suggestion].name);return Event::submit;}
   return Event::none;
  }
  if(in.nav==Direction::up)--row;
  else if(in.nav==Direction::down)++row;
  else if(in.nav==Direction::left)--col;
  else if(in.nav==Direction::right){if(col==(row==int(keys.size())?5:int(keys[row].size())-1)&&!suggestions.empty())suggestion=0;else ++col;}
  clamp();
  if(!in.is_pressed(Action::confirm))return Event::none;
  const auto before=text;
  if(row<int(keys.size()))append(keys[row][col]);
  else switch(col){case 0:erase();break;case 1:append(U' ');break;case 2:text.clear();break;case 3:cycle(1);return Event::language;case 4:return Event::system;case 5:return Event::submit;}
  return before!=text?Event::changed:Event::none;
 }
};
} // namespace sx::search_keyboard
