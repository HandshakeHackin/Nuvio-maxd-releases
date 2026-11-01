// Strict UTC conversion for guide listings and episode release metadata.
#pragma once
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
namespace airtime {
inline int number(const std::string &s, size_t p, size_t n) {
  if (p + n > s.size())
    return -1;
  int v = 0;
  for (size_t i = p; i < p + n; ++i) {
    if (s[i] < '0' || s[i] > '9')
      return -1;
    v = v * 10 + s[i] - '0';
  }
  return v;
}
inline std::optional<int64_t> utc(int y, int m, int d, int h, int min, int sec,
                                  int offset = 0) {
  if (y < 1970 || y > 2200 || m < 1 || m > 12 || d < 1 || d > 31 || h < 0 ||
      h > 23 || min < 0 || min > 59 || sec < 0 || sec > 59)
    return {};
  tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = min;
  t.tm_sec = sec;
  auto epoch = timegm(&t);
  tm check{};
  gmtime_r(&epoch, &check);
  if (check.tm_year != y - 1900 || check.tm_mon != m - 1 ||
      check.tm_mday != d || check.tm_hour != h || check.tm_min != min ||
      check.tm_sec != sec)
    return {};
  return int64_t(epoch) - offset;
}
inline std::optional<int> zone(const std::string &z, bool colon) {
  if (z == "Z")
    return 0;
  if (z.size() != (colon ? 6u : 5u) || (z[0] != '+' && z[0] != '-') ||
      (colon && z[3] != ':'))
    return {};
  int h = number(z, 1, 2), m = number(z, colon ? 4 : 3, 2);
  if (h < 0 || h > 14 || m < 0 || m > 59 || (h == 14 && m))
    return {};
  return (z[0] == '-' ? -1 : 1) * (h * 3600 + m * 60);
}
inline std::optional<int64_t> iso(const std::string &s) {
  if (s.size() < 10 || s[4] != '-' || s[7] != '-')
    return {};
  int y = number(s, 0, 4), m = number(s, 5, 2), d = number(s, 8, 2);
  if (s.size() == 10) {
    auto t = utc(y, m, d, 0, 0, 0);
    return t ? std::optional<int64_t>(*t + 86400) : std::nullopt;
  } // Date-only: wait until the day has ended.
  if (s.size() < 20 || (s[10] != 'T' && s[10] != 't') || s[13] != ':' ||
      s[16] != ':')
    return {};
  size_t z = 19;
  if (s[z] == '.') {
    ++z;
    size_t first = z;
    while (z < s.size() && s[z] >= '0' && s[z] <= '9')
      ++z;
    if (z == first)
      return {};
  }
  auto off = zone(s.substr(z), true);
  if (!off)
    return {};
  return utc(y, m, d, number(s, 11, 2), number(s, 14, 2), number(s, 17, 2),
             *off);
}
inline std::optional<int64_t> xmltv(const std::string &s) {
  size_t space = s.find(' ');
  auto digits = s.substr(0, space);
  if (digits.size() != 12 && digits.size() != 14)
    return {};
  auto offset = space == std::string::npos ? std::optional<int>(0)
                                           : zone(s.substr(space + 1), false);
  if (!offset)
    return {};
  return utc(number(digits, 0, 4), number(digits, 4, 2), number(digits, 6, 2),
             number(digits, 8, 2), number(digits, 10, 2),
             digits.size() == 14 ? number(digits, 12, 2) : 0, *offset);
}
} // namespace airtime
