// Offline, app-scoped verification; independent of account/profile selection.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "build_integrity.h"
#include "build_integrity_text.hpp"
#include "release_public_key.hpp"
#include "../update/core.hpp"
namespace sx {
void Session::integrity_start() {
 if(integrity_checking_)return;
 integrity_checking_=true;integrity_result_={};
 integrity_warning_pending_=false;
 const auto root=base_dir_,version=std::string(nuvio_update::current_version)+"-native";
 integrity_cancel_=std::make_shared<std::atomic<bool>>(false);const auto cancel=integrity_cancel_;
 // Deliberately use the app task pool rather than profile-scoped bg(): a profile
 // switch must not discard a release-file result or its pending warning.
 g_tasks.run<build_integrity::Result>([root,version,cancel] {
  return build_integrity::verify(root,version,build_integrity::official_key,cancel.get());
 },[this,cancel](build_integrity::Result& result) {
  if(*cancel)return;
  integrity_checking_=false;integrity_result_=result;
  integrity_warning_pending_=result.status==build_integrity::Status::unverified;
  if(dropdown_open&&dropdown.purpose==DropdownState::Purpose::build_integrity)
   dropdown.message=integrity_credits(result.status);
  refresh_settings();
 });
}
void Session::integrity_menu() {
 integrity_start();
 open_dropdown(i18n::tr("Build authenticity & credits"),{i18n::tr("Recheck release files"),i18n::tr("Close")},1,[this](int pick){if(pick==0)integrity_menu();});
 dropdown.purpose=DropdownState::Purpose::build_integrity;
 dropdown.active=-1;
 dropdown.wide=true;
 dropdown.message=integrity_credits(integrity_result_.status);
}
void Session::integrity_maybe_warn() {
 if(!integrity_warning_pending_||integrity_warning_shown_||!boot_done_||typing()||watching||resuming_||pin_open||
    release_notes_open||dropdown_open||sign_in_open||install_source_dialog_)return;
 integrity_warning_pending_=false;integrity_warning_shown_=true;
 open_dropdown(i18n::tr("Unverified build"),{i18n::tr("Continue")},0,[](int){});
 dropdown.wide=true;
 dropdown.message=integrity_warning();
 dropdown.active=-1;
}
}
