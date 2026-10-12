#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,6> release_note_items = {
    "Choose Homebrew Store or GitHub/manual installation on first launch.",
    "Your install source is saved locally and editable in Settings.",
    "A startup update check points reminders to your chosen source.",
    "Store installs show store instructions; GitHub installs show GitHub options.",
    "Optional Support on Ko-fi support link and QR code in Settings.",
    "Reopen these notes from Settings → What's new."
};
}
