#include "integrity_session_boundary.hpp"
#include <cassert>
#include <iostream>
namespace build_integrity {
Result response{Status::unverified,Reason::changed_file,3,false};int calls=0;
Result verify(const std::string& root,const std::string& version,const std::array<unsigned char,32>&,const std::atomic<bool>* cancel,unsigned budget) noexcept {
 assert(budget==30000);
 assert(root=="/fixture-app");assert(version==std::string(nuvio_update::current_version)+"-native");++calls;
 return cancel&&*cancel?Result{Status::cancelled,Reason::none,0,false}:response;
}
}
int main() {
 sx::Session s;s.integrity_start();s.integrity_start();assert(g_tasks.pending.size()==1&&s.integrity_checking_);
 ++s.profile_generation_;g_tasks.one();assert(!s.integrity_checking_&&s.integrity_warning_pending_&&s.refreshes==1);
 s.integrity_maybe_warn();assert(!s.dropdown_open);s.boot_done_=true;
 for(auto flag:{&s.watching,&s.resuming_,&s.pin_open,&s.release_notes_open,&s.dropdown_open,&s.sign_in_open,&s.install_source_dialog_,&s.keyboard}) {
  *flag=true;const auto dialogs=s.dialogs;s.integrity_maybe_warn();assert(s.dialogs==dialogs);*flag=false;
 }
 s.integrity_maybe_warn();assert(s.dropdown_open&&s.dropdown.title=="Unverified build"&&s.dropdown.options.size()==1);
 assert(s.dropdown.message.find(build_integrity::project_url)!=std::string::npos&&!s.watching);
 s.dropdown_open=false;const auto dialogs=s.dialogs;s.integrity_maybe_warn();assert(s.dialogs==dialogs);
 s.integrity_menu();assert(s.integrity_checking_&&s.dropdown.purpose==sx::DropdownState::Purpose::build_integrity);
 const auto callback=s.pick;callback(0);assert(g_tasks.pending.size()==1); // No duplicate work while checking.
 build_integrity::response={build_integrity::Status::verified,build_integrity::Reason::none,7,true};
 g_tasks.one();assert(s.dropdown.message.find("Verified release files")!=std::string::npos&&!s.integrity_warning_pending_);
 s.dropdown_open=false;s.integrity_start();*s.integrity_cancel_=true;const auto refreshes=s.refreshes;g_tasks.one();assert(s.refreshes==refreshes);
 // A shutdown-cancelled callback can run after the UI owner has gone away.
 {auto owner=std::make_unique<sx::Session>();owner->integrity_start();*owner->integrity_cancel_=true;owner.reset();g_tasks.one();}
 std::cout<<"Integrity Session: app-scoped results, duplicate suppression, deferred/nonblocking warnings, profile switching, credits refresh and shutdown cancellation passed\n";
}
