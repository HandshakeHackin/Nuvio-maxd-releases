#include "../ui/i18n.hpp"
#pragma once
#include <string>
#include <string_view>
namespace sx {
inline constexpr const char* donation_url="https://ko-fi.com/handshakehackin";
inline constexpr const char* donation_link="ko-fi.com/handshakehackin";
inline bool valid_install_source(std::string_view source) {
    return source=="store" || source=="github" || source=="unknown";
}
inline const char* install_source_label(std::string_view source) {
    return source=="store" ? sx::i18n::tr("Homebrew Store") : source=="github" ? sx::i18n::tr("GitHub / manual") : sx::i18n::tr("Not sure");
}
inline std::string update_destination(std::string_view source) {
    return source=="store" ? sx::i18n::tr("Check Homebrew Store for an update.") : source=="github" ?
        sx::i18n::tr("Update from GitHub. Open Settings → App updates.") : sx::i18n::tr("Choose Homebrew Store or GitHub in Settings → Install source.");
}
}
