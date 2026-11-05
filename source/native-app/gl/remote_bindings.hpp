// App-local mappings for buttons actually delivered by PS5 Media Remote/CEC.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "json.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace sx {
inline constexpr std::uint32_t remote_pad_mask=0x0010fffeu;
enum class RemoteAction : int { search, options, select, back, toggle, play, pause, rewind, forward, up, down, left, right, disabled, count };
inline constexpr const char* remote_action_names[]={"Search", "Options", "Select / OK", "Back", "Play / Pause", "Play", "Pause", "Seek backward", "Seek forward", "Up", "Down", "Left", "Right", "Do nothing"};
struct RemoteButton {
 int source=-1; bool key=true; std::uint32_t code=0, aliases=0;
 bool operator==(const RemoteButton&) const=default;
 bool valid() const {
  return source>=0&&source<2&&code && (key?code<255:((code&remote_pad_mask)==code && (code&(code-1))==0)) && (aliases&~remote_pad_mask)==0;
 }
 bool same(const RemoteButton& b)const{return source==b.source&&key==b.key&&code==b.code;}
};
struct RemoteBinding {
 RemoteButton button;RemoteAction action=RemoteAction::search;
 bool operator==(const RemoteBinding&) const=default;
};
inline bool valid_remote_action(RemoteAction a){return int(a)>=0&&a<RemoteAction::count;}
inline std::string remote_button_name(const RemoteButton& b){
 const std::string prefix=b.source==0?"TV/CEC ":"Media Remote ";
 if(b.key){
  const char* name=b.code==13?"OK":b.code==15?"Back":(b.code==18||b.code==27)?"Options":b.code==22?"Play":b.code==26?"Pause":b.code==45?"Play/Pause":b.code==23?"Rewind":b.code==24?"Fast forward":nullptr;
  return prefix+(name?std::string(name):"key "+std::to_string(b.code));
 }
 return prefix+"button "+std::to_string(b.code);
}
inline bool set_remote_binding(std::vector<RemoteBinding>& mappings,RemoteButton b,RemoteAction action){
 if(!b.valid()||!valid_remote_action(action))return false;
 for(auto& m:mappings)if(m.button.same(b)){m={b,action};return true;}
 if(mappings.size()>=16)return false;
 mappings.push_back({b,action});return true;
}
inline std::vector<RemoteBinding> load_remote_bindings(const nlohmann::json& j){
 std::vector<RemoteBinding> out;
 auto it=j.find("remote_button_mappings");if(it==j.end()||!it->is_array())return out;
 for(const auto& x:*it){
  if(!x.is_object())continue;
  const auto integer=[&](const char* k,int max)->int{
   auto p=x.find(k);if(p==x.end()||!p->is_number_integer())return -1;
   if(*p<0||*p>max)return -1;return p->get<int>();
  };
  const int source=integer("source",1),kind=integer("kind",1),code=integer("code",int(remote_pad_mask)),aliases=integer("aliases",int(remote_pad_mask)),action=integer("action",int(RemoteAction::count)-1);
  if(source<0||kind<0||code<0||aliases<0||action<0)continue;
  RemoteButton b{source,kind==0,std::uint32_t(code),std::uint32_t(aliases)};
  if(std::any_of(out.begin(),out.end(),[&](const auto& m){return m.button.same(b);}))continue;
  set_remote_binding(out,b,RemoteAction(action));
  if(out.size()==16)break;
 }
 return out;
}
inline void save_remote_bindings(nlohmann::json& j,const std::vector<RemoteBinding>& mappings){
 auto a=nlohmann::json::array();
 for(const auto& m:mappings)if(m.button.valid()&&valid_remote_action(m.action)&&a.size()<16)
  a.push_back({{"source",m.button.source},{"kind",m.button.key?0:1},{"code",m.button.code},{"aliases",m.button.aliases},{"action",int(m.action)}});
 j["remote_button_mappings"]=std::move(a);
}
}
