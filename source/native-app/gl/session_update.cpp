// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
namespace sx {
void Session::install_source_choose(int index) {
    static const char* sources[]={"store","github","unknown"};
    prefs_.install_source=sources[index>=0 && index<3?index:2];
    install_source_dialog_=false; install_source_first_=false;
    save_prefs(); refresh_settings();
    if(login_after_notes_) { login_after_notes_=false; login_start(); }
}
void Session::install_source_menu(bool first) {
    const int active=prefs_.install_source=="store"?0:prefs_.install_source=="github"?1:2;
    open_dropdown("Install source",{"Homebrew Store","GitHub / manual download","Not sure"},
        active,[this](int i){install_source_choose(i);});
    dropdown.message="We'll point update reminders to the place you chose. This choice stays on your PS5 and can be changed in Settings → Install source.";
    install_source_dialog_=true; install_source_first_=first;
}
void Session::update_menu() {
    using Phase=AppUpdate::Phase;
    const auto s=updater_.status();
    background_update_check_=false;
    if(s.phase==Phase::available && prefs_.install_source=="store") {
        open_dropdown("New release: "+s.release.version,{"Close","Change install source"},0,[this](int i){if(i==1)install_source_menu();});
        dropdown.message="Open Homebrew Store, find Nuvio Max'd, and check for an update. The store may publish it later than GitHub.\n\n"+s.release.notes.substr(0,350);
    } else if(s.phase==Phase::available && prefs_.install_source!="github") {
        open_dropdown("New release: "+s.release.version,{"Choose install source","Later"},0,[this](int i){if(i==0)install_source_menu();});
        dropdown.message="Choose where you installed the app so we can show the right update instructions.";
    } else if(s.phase==Phase::available) {
        open_dropdown("Nuvio Max'd "+s.release.version,{"Download update in app","GitHub update instructions","Later"},0,[this](int i){if(i==0){updater_.download(base_dir_,data_dir_);update_menu();}
            else if(i==1){open_dropdown("Update from GitHub",{"Close"},0,[](int){});
                dropdown.message="Open github.com/HandshakeHackin/Nuvio-maxd-releases/releases on your phone or computer. Download the latest ZIP or FFPFSC, close Nuvio Max'd and replace your app using the same install method. Your saved data stays in place.";}});
        dropdown.message=std::to_string((s.release.size+(1<<20)-1)>>20)+" MB FFPFSC. Matching source is included in the public release.\n\n"+s.release.notes.substr(0,350);
    } else if(s.phase==Phase::ready) {
        open_dropdown("Update ready",{"Install and close Nuvio Max'd","Cancel update"},0,[this](int i){if(i==0){updater_.install();say("Closing Nuvio Max'd after the helper is ready",8);}else updater_.cancel();});
        dropdown.message="The verified image is staged. Nuvio Max'd will close, retain a backup and request a ShadowMount+ rescan. Reopen it after the update notification. Your saved data stays in place.";
    } else if(s.phase==Phase::checking || s.phase==Phase::downloading || s.phase==Phase::staging) {
        open_dropdown("App updates",{"Cancel"},0,[this](int){updater_.cancel();});
        dropdown.message=s.message;dropdown.progress=s.progress;
    } else {
        open_dropdown("App updates",{"Check for updates","Close"},0,[this](int i){if(i==0){updater_.check();update_menu();}});
        dropdown.message=s.message.empty()?"Nuvio Max'd "+std::string(nuvio_update::current_version)+". "+update_destination(prefs_.install_source):s.message;
    }
    update_dialog_=true;
}
void Session::update_status() {
    const bool unobstructed=boot_done_ && !watching && !release_notes_open && !dropdown_open && !sign_in_open && !typing();
    if(unobstructed && prefs_.install_source.empty()) {install_source_menu(true);return;}
    if(unobstructed && !startup_update_checked_ && !prefs_.install_source.empty()) {
        startup_update_checked_=true;
        if(updater_.status().phase==AppUpdate::Phase::idle) {background_update_check_=true;updater_.check();}
    }
    const auto s=updater_.status();
    if(s.phase==AppUpdate::Phase::closing) {exit_=true;return;}
    if(s.phase!=update_phase_) {
        update_phase_=s.phase;refresh_settings();
        if(update_dialog_ && dropdown_open) update_menu();
        else if(s.phase==AppUpdate::Phase::error && !background_update_check_) say(s.message,9,true);
    } else if(update_dialog_ && dropdown_open && s.progress>=0) {
        dropdown.progress=s.progress;dropdown.message=s.message;
    }
    if(unobstructed && s.phase==AppUpdate::Phase::available && notified_update_!=s.release.version) {
        notified_update_=s.release.version;
        say("Nuvio Max'd "+s.release.version+" is available. "+update_destination(prefs_.install_source),15);
    }
}
}
