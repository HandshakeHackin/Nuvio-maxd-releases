#include "live_guide.h"
#include "airtime.hpp"
#include "util.h"
#include <algorithm>
#include <array>
#include <expat.h>
#include <limits>
#include <zlib.h>
namespace live_guide {
namespace {
constexpr size_t max_programmes = 1000000, max_kept = 50000,
                 max_per_channel = 8;
std::string clean(std::string text) {
  for (char &c : text)
    if (static_cast<unsigned char>(c) < 32 || c == 127)
      c = ' ';
  text = trim(text);
  if (text.size() > 320) {
    size_t end = 320;
    while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
      --end;
    text.resize(end);
  }
  return text;
}
void add(Result &r, const std::string &channel, Programme p, int64_t now) {
  if (channel.empty() || channel.size() > 512 || p.title.empty() ||
      p.start <= 0 || p.end <= p.start || p.end - p.start > 172800 ||
      p.end <= now || p.start > now + 172800)
    return;
  if (!r.channels.count(channel) &&
      r.channels.size() >= max_kept / max_per_channel) {
    r.limited = true;
    return;
  }
  auto &rows = r.channels[channel];
  for (const auto &old : rows)
    if (old.start == p.start && old.end == p.end)
      return;
  rows.push_back(std::move(p));
  std::sort(rows.begin(), rows.end(),
            [](const auto &a, const auto &b) { return a.start < b.start; });
  if (rows.size() > max_per_channel)
    rows.resize(max_per_channel);
}
struct Xml {
  XML_Parser parser = nullptr;
  Result result;
  const std::set<std::string> &wanted;
  const std::atomic<bool> *cancel;
  int64_t now;
  size_t count = 0;
  int depth = 0, title_depth = 0;
  bool active = false;
  std::string channel{};
  Programme p{};
  void stop() { XML_StopParser(parser, XML_FALSE); }
};
const char *attribute(const XML_Char **attrs, const char *key) {
  for (size_t i = 0; attrs[i]; i += 2)
    if (std::string(attrs[i]) == key)
      return attrs[i + 1];
  return "";
}
void XMLCALL start(void *ptr, const XML_Char *name, const XML_Char **attrs) {
  auto &x = *static_cast<Xml *>(ptr);
  if (++x.depth > 32 || (x.cancel && *x.cancel)) {
    x.stop();
    return;
  }
  if (x.depth == 1 && std::string(name) != "tv") {
    x.stop();
    return;
  }
  if (x.depth == 2 && std::string(name) == "programme") {
    if (++x.count > max_programmes) {
      x.stop();
      return;
    }
    x.channel = attribute(attrs, "channel");
    x.p = {};
    auto a = airtime::xmltv(attribute(attrs, "start")),
         b = airtime::xmltv(attribute(attrs, "stop"));
    x.active = a && b && x.wanted.count(x.channel);
    if (x.active) {
      x.p.start = *a;
      x.p.end = *b;
    }
  } else if (x.active && x.depth == 3 && std::string(name) == "title" &&
             x.p.title.empty())
    x.title_depth = x.depth;
}
void XMLCALL end(void *ptr, const XML_Char *name) {
  auto &x = *static_cast<Xml *>(ptr);
  if (x.depth == x.title_depth)
    x.title_depth = 0;
  if (x.depth == 2 && std::string(name) == "programme") {
    if (x.active) {
      x.p.title = clean(x.p.title);
      add(x.result, x.channel, std::move(x.p), x.now);
    }
    x.active = false;
  }
  --x.depth;
}
void XMLCALL chars(void *ptr, const XML_Char *data, int length) {
  auto &x = *static_cast<Xml *>(ptr);
  if (x.title_depth && x.p.title.size() < 2048)
    x.p.title.append(data, std::min<size_t>(length, 2048 - x.p.title.size()));
}
void XMLCALL entity(void *ptr, const XML_Char *, int, const XML_Char *, int,
                    const XML_Char *, const XML_Char *, const XML_Char *,
                    const XML_Char *) {
  static_cast<Xml *>(ptr)->stop();
}
std::optional<int64_t> epoch(const json &v) {
  const auto s = v.is_string()           ? v.get<std::string>()
                 : v.is_number_integer() ? std::to_string(v.get<int64_t>())
                                         : "";
  if (s.empty() || s.size() > 11)
    return {};
  int64_t n = 0;
  for (char c : s) {
    if (c < '0' || c > '9')
      return {};
    n = n * 10 + c - '0';
  }
  return n > 0 && n < 7308115200LL ? std::optional<int64_t>(n) : std::nullopt;
}
std::string title(std::string text) {
  // Xtream titles are normally base64. Plain-text provider variants remain
  // usable.
  if (text.size() > 2048)
    return {};
  if (!text.empty() && text.size() % 4 == 0) {
    const std::string alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;
    unsigned bits = 0;
    int n = 0;
    bool valid = true, padded = false;
    for (char c : text) {
      if (c == '=') {
        padded = true;
        continue;
      }
      auto v = alphabet.find(c);
      if (padded || v == std::string::npos) {
        valid = false;
        break;
      }
      bits = (bits << 6) | unsigned(v);
      n += 6;
      if (n >= 8) {
        n -= 8;
        decoded += char((bits >> n) & 255);
      }
    }
    size_t padding = text.size() - text.find_last_not_of('=') - 1;
    if (padding > 2)
      valid = false;
    for (size_t i = 0; i < decoded.size();) {
      unsigned char c = decoded[i++];
      if (c < 32 || c == 127) {
        valid = false;
        break;
      }
      if (c < 128)
        continue;
      int n = c >= 0xc2 && c <= 0xdf   ? 1
              : c >= 0xe0 && c <= 0xef ? 2
              : c >= 0xf0 && c <= 0xf4 ? 3
                                       : -1;
      if (n < 0 || i + size_t(n) > decoded.size()) {
        valid = false;
        break;
      }
      unsigned cp = c & ((1u << (6 - n)) - 1);
      for (int k = 0; k < n; ++k) {
        unsigned char b = decoded[i++];
        if ((b & 0xc0) != 0x80)
          valid = false;
        cp = (cp << 6) | (b & 63);
      }
      if ((n == 1 && cp < 128) || (n == 2 && cp < 2048) ||
          (n == 3 && cp < 65536) || cp > 0x10ffff ||
          (cp >= 0xd800 && cp <= 0xdfff))
        valid = false;
    }
    if (valid && !decoded.empty())
      text = std::move(decoded);
  }
  return clean(text);
}
Result failure() {
  Result r;
  r.error = "Provider programme guide is unavailable. Channels can still play.";
  return r;
}
} // namespace
NowNext current(const Listings &list, int64_t now) {
  NowNext r;
  for (const auto &p : list)
    if (p.start <= now && p.end > now && (!r.now || p.start > r.now->start))
      r.now = &p;
  for (const auto &p : list)
    if (p.start > now && (!r.now || p.start >= r.now->end) &&
        (!r.next || p.start < r.next->start))
      r.next = &p;
  return r;
}
std::string label(const Programme &p) {
  time_t t = p.start;
  tm local{};
  localtime_r(&t, &local);
  char time[8];
  strftime(time, sizeof(time), "%H:%M", &local);
  return std::string(time) + " · " + p.title;
}
Result parse_xmltv(const std::string &body, const std::set<std::string> &wanted,
                   int64_t now, const std::atomic<bool> *cancel) {
  if (body.empty() || body.size() > max_bytes || (cancel && *cancel))
    return failure();
  Xml x{nullptr, {}, wanted, cancel, now};
  x.parser = XML_ParserCreate(nullptr);
  if (!x.parser)
    return failure();
  XML_SetUserData(x.parser, &x);
  XML_SetElementHandler(x.parser, start, end);
  XML_SetCharacterDataHandler(x.parser, chars);
  XML_SetEntityDeclHandler(x.parser, entity);
  XML_SetParamEntityParsing(x.parser, XML_PARAM_ENTITY_PARSING_NEVER);
  bool ok = true;
  for (size_t at = 0; at < body.size(); at += 32768) {
    auto size = std::min<size_t>(32768, body.size() - at);
    if ((cancel && *cancel) ||
        XML_Parse(x.parser, body.data() + at, int(size),
                  at + size == body.size()) == XML_STATUS_ERROR) {
      ok = false;
      break;
    }
  }
  XML_ParserFree(x.parser);
  return ok ? std::move(x.result) : failure();
}
Result parse_xtream(const std::string &body, const std::string &channel,
                    int64_t now) {
  if (body.size() > live_tv::max_metadata_bytes)
    return failure();
  auto j = json::parse(body, nullptr, false);
  if (!j.is_object() || !jobj(j, "epg_listings").is_array())
    return failure();
  Result r;
  const auto &rows = j["epg_listings"];
  if (rows.size() > 256)
    return failure();
  for (const auto &row : rows) {
    auto a = epoch(jobj(row, "start_timestamp")),
         b = epoch(jobj(row, "stop_timestamp"));
    if (!b)
      b = epoch(jobj(row, "end_timestamp"));
    if (a && b)
      add(r, channel, {*a, *b, title(jstr(row, "title"))}, now);
  }
  return r;
}
Result fetch_xmltv(const std::string &url, const std::set<std::string> &wanted,
                   int64_t now, const std::atomic<bool> *cancel) {
  auto response = live_tv::download(url, cancel, true, max_bytes);
  if (!response.ok())
    return failure();
  auto &body = response.body;
  if (body.size() > 2 && uint8_t(body[0]) == 0x1f && uint8_t(body[1]) == 0x8b) {
    z_stream z{};
    z.next_in = reinterpret_cast<Bytef *>(body.data());
    z.avail_in = uInt(body.size());
    if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK)
      return failure();
    std::string expanded;
    std::array<char, 32768> block{};
    int rc = Z_OK;
    while (rc == Z_OK && !(cancel && *cancel)) {
      z.next_out = reinterpret_cast<Bytef *>(block.data());
      z.avail_out = block.size();
      rc = inflate(&z, Z_NO_FLUSH);
      auto n = block.size() - z.avail_out;
      if (n > max_bytes - expanded.size()) {
        rc = Z_MEM_ERROR;
        break;
      }
      expanded.append(block.data(), n);
    }
    inflateEnd(&z);
    if (rc != Z_STREAM_END)
      return failure();
    body = std::move(expanded);
  }
  auto result = parse_xmltv(body, wanted, now, cancel);
  if (result.error.empty())
    result.document = std::make_shared<const std::string>(std::move(body));
  return result;
}
Result fetch_xtream(const std::string &server, const std::string &user,
                    const std::string &pass, const std::string &channel,
                    int64_t now, const std::atomic<bool> *cancel) {
  if (!live_tv::http_url(server) || user.empty() || pass.empty() ||
      user.size() > 256 || pass.size() > 256 || channel.empty() ||
      channel.size() > 16 ||
      channel.find_first_not_of("0123456789") != std::string::npos)
    return failure();
  auto root = server.substr(0, server.find_first_of("?#"));
  while (!root.empty() && root.back() == '/')
    root.pop_back();
  auto response = live_tv::download(
      root + "/player_api.php?username=" + url_encode(user) +
          "&password=" + url_encode(pass) +
          "&action=get_short_epg&stream_id=" + channel + "&limit=8",
      cancel, false, live_tv::max_metadata_bytes);
  return response.ok() ? parse_xtream(response.body, channel, now) : failure();
}
} // namespace live_guide
