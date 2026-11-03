#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
namespace http_credentials {
inline std::string folded(std::string s) {
  for (auto &c : s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
inline std::string origin(const std::string &s) {
  auto p = s.find("://");
  if (p == std::string::npos)
    return {};
  auto scheme = folded(s.substr(0, p));
  if (scheme != "http" && scheme != "https")
    return {};
  auto e = s.find_first_of("/?#", p + 3);
  auto authority =
      folded(s.substr(p + 3, e == std::string::npos ? e : e - p - 3));
  if (authority.empty() || authority.find('@') != std::string::npos)
    return {};
  auto suffix = scheme == "https" ? ":443" : ":80";
  if (authority.size() > std::char_traits<char>::length(suffix) &&
      authority.ends_with(suffix))
    authority.resize(authority.size() - std::char_traits<char>::length(suffix));
  return scheme + "://" + authority;
}
inline bool secret(const std::string &s) {
  auto k = folded(s.substr(0, s.find(':')));
  return k == "authorization" || k == "x-emby-authorization" ||
         k == "x-emby-token" || k == "x-plex-token";
}
inline bool scoped(const std::vector<std::string> &h) {
  for (const auto &s : h)
    if (s.starts_with("X-Maxd-Credential-Origin: "))
      return true;
  return false;
}
inline std::vector<std::string> filter(const std::vector<std::string> &h,
                                       const std::string &url) {
  std::string scope;
  for (const auto &s : h)
    if (s.starts_with("X-Maxd-Credential-Origin: "))
      scope = s.substr(26);
  std::vector<std::string> out;
  for (const auto &s : h) {
    if (s.starts_with("X-Maxd-Credential-Origin:"))
      continue;
    if (secret(s) && !scope.empty() && origin(url) != scope)
      continue;
    out.push_back(s);
  }
  return out;
}
inline bool protected_request(const std::vector<std::string> &h) {
  for (const auto &s : h)
    if (secret(s))
      return true;
  return false;
}
} // namespace http_credentials
