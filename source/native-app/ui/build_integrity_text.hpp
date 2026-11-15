// Presentation only; no crypto, filesystem or user data dependencies.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "i18n.hpp"
#include "../src/build_integrity.h"
#include <string>
namespace sx {
inline const char* integrity_status(build_integrity::Status status) {
 switch(status) {
 case build_integrity::Status::verified:return i18n::tr("Verified release files");
 case build_integrity::Status::unverified:return i18n::tr("Unverified build");
 default:return i18n::tr("Checking release files...");
 }
}
inline std::string integrity_credits(build_integrity::Status status) {
 return std::string(integrity_status(status))+"\n"+
  i18n::tr("Nuvio Max'd — HandshakeHackin. Based on work by Nuvio, BlackBearReloaded, Sp9nky, Husam Osman and the credited library authors.")+"\n"+
  i18n::tr("GPL-3.0. Forks and redistribution are allowed under the license. See LICENSE and THIRD_PARTY.md for notices.")+"\n"+build_integrity::project_url;
}
inline std::string integrity_warning() {
 return std::string(i18n::tr("Some release files or credits could not be verified. This may be a modified build or a damaged installation."))+"\n"+
  i18n::tr("Verification runs offline. You can continue using the app.")+"\n"+build_integrity::project_url;
}
}
