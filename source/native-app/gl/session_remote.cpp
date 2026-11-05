// Experimental app-local Media Remote/CEC mappings. No PlayStation OS remapping.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
namespace sx {
void Session::remote_menu(){
    std::vector<std::string> options={"Add button mapping", "Test remote buttons", "Reset all remote mappings"};
    for(const auto& m:prefs_.remote_bindings)options.push_back(remote_button_name(m.button)+" → "+remote_action_names[int(m.action)]);
    const auto generation=profile_generation_;
    open_dropdown("Remote buttons (experimental)",options,0,[this,generation](int choice){
        if(generation!=profile_generation_)return;
        if(choice==0){
            std::vector<std::string> actions(std::begin(remote_action_names),std::end(remote_action_names));
            open_dropdown("Choose what the remote button should do",actions,0,[this,generation](int action){
                if(generation==profile_generation_&&action>=0&&action<int(RemoteAction::count))remote_learn(action);
            });
        }else if(choice==1)remote_learn(-1);
        else if(choice==2){
            open_dropdown("Reset all remote mappings?",{"Cancel","Reset"},0,[this,generation](int pick){
                if(pick==1&&generation==profile_generation_){prefs_.remote_bindings.clear();save_prefs();refresh_settings();say("Normal remote controls restored");}
            });
        }else if(choice>=3&&choice<int(prefs_.remote_bindings.size())+3){
            const auto binding=prefs_.remote_bindings[size_t(choice-3)];
            open_dropdown(remote_button_name(binding.button),{"Cancel","Change action","Remove mapping"},0,[this,generation,binding](int pick){
                if(generation!=profile_generation_)return;
                if(pick==1){
                    std::vector<std::string> actions(std::begin(remote_action_names),std::end(remote_action_names));
                    open_dropdown("Choose a new action",actions,int(binding.action),[this,generation,binding](int action){
                        if(generation!=profile_generation_||action<0||action>=int(RemoteAction::count))return;
                        set_remote_binding(prefs_.remote_bindings,binding.button,RemoteAction(action));save_prefs();refresh_settings();remote_menu();
                    });
                }else if(pick==2){
                    auto& b=prefs_.remote_bindings;b.erase(std::remove_if(b.begin(),b.end(),[&](const auto& m){return m.button.same(binding.button);}),b.end());
                    save_prefs();refresh_settings();remote_menu();
                }
            });
        }
    });
    dropdown.message="These settings only affect buttons that Nuvio receives while it is open. PlayStation or TV controls may not reach the app. Keep a controller handy; its controls stay unchanged. Select an existing mapping to change or remove it.";
}
void Session::remote_learn(int action){
    remote_capture_action_=action;remote_capture_generation_=profile_generation_;
    remote_capture_until_=now_seconds()+15;
    open_dropdown(action<0?"Test a remote button":"Press a remote button",{"Cancel"},0,[this](int){remote_capture_until_=0;});
    dropdown.message="Release the remote buttons, then press one button. Waiting up to 15 seconds. If nothing is detected, the PlayStation or TV may be handling that button before Nuvio can see it. Cancel with the controller.";
}
void Session::remote_receive(const RemoteButton& button){
    if(!remote_learning()||!button.valid())return;
    const int action=remote_capture_action_;remote_capture_until_=0;
    dropdown_open=false;dropdown_chosen_=nullptr;
    if(action<0){
        open_dropdown("Button received: "+remote_button_name(button),{"Test another button","Close"},1,[this](int pick){if(pick==0)remote_learn(-1);});
        dropdown.message="This button reached Nuvio and can be assigned an action. Buttons that open another app or change TV volume may still be controlled by PlayStation or your TV.";
        return;
    }
    if(!set_remote_binding(prefs_.remote_bindings,button,RemoteAction(action))){say("Cannot add this mapping. Remove an existing mapping if you have reached the 16-button limit.",8,true);return;}
    save_prefs();refresh_settings();
    say(remote_button_name(button)+" assigned to "+remote_action_names[action],6);
}
void Session::remote_capture_tick(){
    if(remote_capture_until_<=0)return;
    if(!dropdown_open||remote_capture_generation_!=profile_generation_){remote_capture_until_=0;return;}
    if(now_seconds()>=remote_capture_until_){
        remote_capture_until_=0;dropdown_open=false;dropdown_chosen_=nullptr;
        open_dropdown("No remote button received",{"Try again","Close"},1,[this](int pick){if(pick==0)remote_learn(remote_capture_action_);});
        dropdown.message="Nuvio did not receive a new button press. PlayStation or your TV may control that button at the system level. This attempt cannot remap a button the app cannot see. Try a different button, or use Triangle on the controller for Search.";
    }
}
} // namespace sx
