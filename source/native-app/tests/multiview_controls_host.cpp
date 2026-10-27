// Execute the production grid input handler at its UI/device boundaries.
#include "core/input.hpp"
#include "live_tv.h"
#include <array>
#include <vector>
#include <string>
#include <cassert>
#include <iostream>
namespace sx {
struct Session {
 bool multiview_active=true;int multiview_focus=0,multiview_audio=0;
 std::array<live_tv::Channel,4> multi_channels_;
 std::vector<std::string> events;
 void multi_input(const hui::InputFrame&);
 void multi_stop(){events.push_back("stop");multiview_active=false;}
 void multi_open(int i,bool audio){events.push_back(std::to_string(i)+(audio?" audible":" muted"));}
 void multi_choose(std::vector<live_tv::Channel>,int tile){events.push_back("replace "+std::to_string(tile));}
 void live_play(const live_tv::Channel& c){events.push_back("fullscreen "+c.name);}
};
#include "multiview_input_body.hpp"
}
int main() {
 sx::Session s;s.multi_channels_[3].name="News";
 hui::InputFrame in;in.nav=hui::Direction::right;s.multi_input(in);
 in.nav=hui::Direction::down;s.multi_input(in);assert(s.multiview_focus==3 && s.multiview_audio==0);
 in.nav=hui::Direction::none;in.pressed=hui::action_bit(hui::Action::confirm);s.multi_input(in);
 assert(s.multiview_audio==3 && s.events==std::vector<std::string>({"0 muted","3 audible"}));
 s.events.clear();in.pressed=hui::action_bit(hui::Action::west);s.multi_input(in);assert(s.events[0]=="replace 3");
 s.events.clear();in.pressed=hui::action_bit(hui::Action::north);s.multi_input(in);
 assert(!s.multiview_active && s.events==std::vector<std::string>({"stop","fullscreen News"}));
 s.multiview_active=true;s.events.clear();in.pressed=hui::action_bit(hui::Action::back);s.multi_input(in);
 assert(!s.multiview_active && s.events==std::vector<std::string>({"stop"}));
 std::cout<<"Multiview controls: focus, single audio transfer order, replacement, full-screen and stop passed\n";
}
