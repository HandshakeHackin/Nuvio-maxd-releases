#include "remote_session_boundary.hpp"
#include <iostream>
#include <fstream>
int main(int argc,char** argv){
 sx::Session s;s.remote_menu();s.pick(0);s.pick(int(sx::RemoteAction::search));assert(s.remote_learning());
 s.remote_receive({});assert(s.saves==0);
 s.remote_receive({1,true,99,0});assert(s.saves==1&&!s.remote_learning());
 assert(sx::load_remote_bindings(s.saved)==s.prefs_.remote_bindings);
 s.remote_menu();s.pick(3);s.pick(1);s.pick(int(sx::RemoteAction::options));assert(s.prefs_.remote_bindings[0].action==sx::RemoteAction::options);
 s.remote_menu();s.pick(2);s.pick(0);assert(s.prefs_.remote_bindings.size()==1);
 s.remote_menu();s.pick(3);s.pick(2);assert(s.prefs_.remote_bindings.empty());
 s.remote_learn(-1);int saves=s.saves;s.remote_receive({0,true,88,0});assert(s.saves==saves&&s.prefs_.remote_bindings.empty());assert(s.dropdown.title.find("88")!=std::string::npos);
 s.pick(0);assert(s.remote_learning());fake_remote_time+=16;s.remote_capture_tick();assert(!s.remote_learning()&&s.dropdown.title=="No remote button received");assert(s.dropdown.message.find("system level")!=std::string::npos);
 s.remote_learn(0);s.dropdown_open=false;s.remote_capture_tick();assert(s.remote_capture_until_==0);
 s.remote_learn(0);++s.profile_generation_;s.remote_receive({1,true,99,0});assert(s.saves==saves);s.remote_capture_tick();assert(s.remote_capture_until_==0);
 s.remote_learn(0);fake_remote_time+=16;s.remote_receive({1,true,99,0});assert(s.saves==saves);s.remote_capture_tick();
 for(unsigned i=1;i<=16;++i)sx::set_remote_binding(s.prefs_.remote_bindings,{1,true,i,0},sx::RemoteAction::search);
 s.remote_learn(0);s.remote_receive({1,true,99,0});assert(s.saves==saves&&s.prefs_.remote_bindings.size()==16);
 s.remote_menu();s.pick(2);s.pick(1);assert(s.prefs_.remote_bindings.empty());assert(sx::load_remote_bindings(s.saved).empty());
 nlohmann::json bad={{"remote_button_mappings",nlohmann::json::array()}};
 for(auto value:{nlohmann::json(-1),nlohmann::json(1.5),nlohmann::json("1"),nlohmann::json(UINT64_MAX)})bad["remote_button_mappings"].push_back({{"source",value},{"kind",0},{"code",99},{"aliases",0},{"action",0}});
 assert(sx::load_remote_bindings(bad).empty());
 if(argc>1){
  sx::Session preview;nlohmann::json scenes=nlohmann::json::object();
  const auto snapshot=[&](const char* mode){scenes[mode]={{"title",preview.dropdown.title},{"message",preview.dropdown.message},{"options",preview.choices}};};
  preview.remote_menu();snapshot("remote-remap");preview.remote_learn(0);snapshot("remote-learn");fake_remote_time+=16;preview.remote_capture_tick();snapshot("remote-timeout");
  std::ofstream(argv[1])<<scenes.dump(2);
 }
 std::cout<<"Remote Session: action selection, assign/edit/remove/reset, saved roundtrip, detection-only mode, timeout/cancel, profile races and mapping limit passed\n";
}
