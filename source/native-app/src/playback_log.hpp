#pragma once
#include <cctype>
#include <string>
#include <algorithm>
// FFmpeg can print segment URLs supplied inside an HLS manifest. Those URLs
// may contain server credentials even when the negotiated master used headers.
namespace playback_log {
inline std::string redact_urls(std::string s) {
  size_t at = 0;
  while (at < s.size()) {
    std::string rest = s.substr(at);
    for (auto &c : rest)
      c = char(std::tolower(static_cast<unsigned char>(c)));
    auto a = rest.find("http://"), b = rest.find("https://");
    auto p = a == std::string::npos   ? b
             : b == std::string::npos ? a
                                      : std::min(a, b);
    if (p == std::string::npos)
      break;
    p += at;
    auto end = s.find_first_of(" \t\r\n\"'<>", p);
    if (end == std::string::npos)
      end = s.size();
    s.replace(p, end - p, "[media URL]");
    at = p + 11;
  }
  return s;
}
} // namespace playback_log
