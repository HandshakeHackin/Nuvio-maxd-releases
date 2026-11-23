// A bundled last-resort face, independent of system fonts or language tags.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <string>
namespace subtitle_fonts {
inline constexpr const char* fallback_family="Nuvio Subtitle Fallback";
inline constexpr const char* fallback_file="NuvioSubtitleFallback-Regular.ttf";
inline std::string fallback_path(const std::string& directory) {return directory+"/"+fallback_file;}
inline std::array<std::string,3> vlc_options() {
 const auto font=fallback_path("/app0/fonts");
 return {":freetype-font="+font,":freetype-monofont="+font,":ssa-fontsdir=/app0/fonts"};
}
}
