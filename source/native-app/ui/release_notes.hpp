#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,9> release_note_items = {
    "Connect Trakt, SIMKL or MDBList in Tracking accounts.",
    "Choose Library Source and Watch Progress separately; Nuvio is default.",
    "Only connected trackers appear in the source choices.",
    "Keep tracker connections, choices and offline progress per profile.",
    "Use Nuvio temporarily if a tracker cannot be reached.",
    "Phone pairing needs application credentials; live tracker testing is pending.",
    "Try improved HDR switching and film pacing with Prefer 120 Hz.",
    "Added GPU lifetime safeguards and a provider-download socket fix; console tests pending.",
    "Keep readable Continue Watching entries when a profile's sync response contains an unsupported row."
};
}
