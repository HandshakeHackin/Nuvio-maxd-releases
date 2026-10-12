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
    return source=="store" ? "Homebrew Store" : source=="github" ? "GitHub / manual" : "Not sure";
}
inline std::string update_destination(std::string_view source) {
    return source=="store" ? "Check Homebrew Store for an update." : source=="github" ?
        "Update from GitHub. Open Settings → App updates." : "Choose Homebrew Store or GitHub in Settings → Install source.";
}
}
