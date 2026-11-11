#include "localization_cancel.hpp"
#include "i18n.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
int main(){
 for(const auto& locale:sx::i18n::locales()){
  sx::LocaleCancelSession s;sx::i18n::set_language(locale.code);
  for(auto purpose:{sx::DropdownState::Purpose::usb_scan,sx::DropdownState::Purpose::usb_import}){s.dropdown.purpose=purpose;s.dropdown.title=sx::i18n::tr("Scanning USB playlists");*s.live_usb_cancel_=false;s.dropdown_open=true;s.dropdown_cancel();assert(*s.live_usb_cancel_&&!s.dropdown_open);}
  s.dropdown.purpose=sx::DropdownState::Purpose::usb_import;s.open_dropdown(sx::i18n::tr("Language"),{"x"},0,[](int){});assert(s.dropdown.purpose==sx::DropdownState::Purpose::generic);*s.live_usb_cancel_=false;s.dropdown_cancel();assert(!*s.live_usb_cancel_);
 }
 char path[]="/tmp/nuvio-language-XXXXXX";assert(mkdtemp(path));sx::LanguageSession language;language.data_dir_=path;sx::i18n::set_language("en");
 for(size_t n=0;n<sx::i18n::locales().size();++n){
  const auto& locale=sx::i18n::locales()[n];language.settings_activate(sx::kUiLanguage);assert(language.choices.size()==8&&language.choices[n]==locale.native_name);language.choice(int(n));assert(language.prefs_.ui_language==locale.code&&!language.error);
  json stored;assert(load_json(std::string(path)+"/settings.json",stored)&&stored["ui_language"]==locale.code);assert(std::string(sx::i18n::code())=="en");
  language.settings_activate(sx::kUiLanguage);assert(language.active==int(n));
 }
 const auto previous=language.prefs_.ui_language;language.write_allowed=false;language.choice(0);assert(language.prefs_.ui_language==previous&&language.error);language.choice(-1);language.choice(99);assert(language.prefs_.ui_language==previous);
 std::filesystem::remove_all(path);
 std::cout<<"Localization dialogs: eight native language names, real persistence/readback, failed-save rollback, restart boundary and stable USB cancellation purposes passed\n";
}
