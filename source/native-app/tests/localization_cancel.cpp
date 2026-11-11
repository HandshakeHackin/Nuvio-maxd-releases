#include "localization_cancel.hpp"
#include "i18n.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
int main(){sx::LocaleCancelSession s;sx::i18n::set_language("de");
 for(auto purpose:{sx::DropdownState::Purpose::usb_scan,sx::DropdownState::Purpose::usb_import}){s.dropdown.purpose=purpose;s.dropdown.title="Completely different translated title";*s.live_usb_cancel_=false;s.dropdown_open=true;s.dropdown_cancel();assert(*s.live_usb_cancel_&&!s.dropdown_open);}
 s.dropdown.purpose=sx::DropdownState::Purpose::usb_import;s.open_dropdown("Normal",{"Abbrechen"},0,[](int){});assert(s.dropdown.purpose==sx::DropdownState::Purpose::generic);*s.live_usb_cancel_=false;s.dropdown_cancel();assert(!*s.live_usb_cancel_);
 char path[]="/tmp/nuvio-language-XXXXXX";assert(mkdtemp(path));sx::LanguageSession language;language.data_dir_=path;
 sx::i18n::set_language("en");language.settings_activate(sx::kUiLanguage);language.choice(1);assert(language.prefs_.ui_language=="de"&&!language.error);json stored;assert(load_json(std::string(path)+"/settings.json",stored)&&stored["ui_language"]=="de");assert(std::string(sx::i18n::code())=="en");
 language.choice(0);assert(language.prefs_.ui_language=="en");language.write_allowed=false;language.choice(1);assert(language.prefs_.ui_language=="en"&&language.error);language.choice(99);assert(language.prefs_.ui_language=="en");std::filesystem::remove_all(path);
 std::cout<<"Localization dialogs: production USB cancellation uses stable purposes across translations and replacement dialogs reset identity passed\n";
}
