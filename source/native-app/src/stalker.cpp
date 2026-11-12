#include "stalker.h"
#include <algorithm>
#include <cctype>
#include <curl/curl.h>
#include <map>
#include <openssl/evp.h>
#include <regex>
#include <set>
#include <stdexcept>
namespace stalker {
namespace {
using J = nlohmann::json;
using Params = std::map<std::string, std::string>;
constexpr size_t metadata_limit = 8u << 20, catalog_limit = 128u << 20,
                 item_limit = 100000;
constexpr const char *agent =
    "Mozilla/5.0 (QtEmbedded; U; Linux; C) AppleWebKit/533.3 (KHTML, like "
    "Gecko) MAG250 stbapp ver: 4 rev: 1812 Mobile Safari/533.3";
struct Failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};
std::string field(const J &j, const char *key) {
  if (!j.is_object() || !j.contains(key))
    return {};
  const auto &v = j[key];
  return v.is_string()           ? v.get<std::string>()
         : v.is_number_integer() ? v.dump()
                                 : std::string();
}
int number(const J &j, const char *key, int fallback = 0) {
  auto s = field(j, key);
  if (s.empty() || s.size() > 10 ||
      s.find_first_not_of("0123456789") != std::string::npos)
    return fallback;
  auto n = std::stoll(s);
  return n <= 2147483647 ? n : fallback;
}
std::string clean(std::string s, size_t cap = 256) {
  s.erase(std::remove_if(s.begin(), s.end(),
                         [](unsigned char c) { return c < 32 || c == 127; }),
          s.end());
  if (!is_valid_utf8(s))
    return {};
  if (s.size() > cap) {
    s.resize(cap);
    while (!s.empty() && !is_valid_utf8(s))
      s.pop_back();
  }
  return s;
}
std::string digest(const std::string &s) {
  unsigned char bytes[EVP_MAX_MD_SIZE];
  unsigned int n = 0;
  if (!EVP_Digest(s.data(), s.size(), bytes, &n, EVP_sha1(), nullptr))
    throw Failure("Portal authentication unavailable.");
  const char *hex = "0123456789ABCDEF";
  std::string out;
  for (unsigned i = 0; i < n; ++i) {
    out += hex[bytes[i] >> 4];
    out += hex[bytes[i] & 15];
  }
  return out;
}
void check(const std::atomic<bool> *cancel) {
  if (cancel && *cancel)
    throw Failure("Cancelled");
}
J parse(const std::string &s) {
  bool deep = false;
  auto j = J::parse(
      s,
      [&](int d, J::parse_event_t, J &) {
        if (d > 40) {
          deep = true;
          return false;
        }
        return true;
      },
      false);
  if (deep || j.is_discarded())
    throw Failure("Portal returned an invalid response.");
  return j;
}
std::string command(const J &j) {
  for (auto key : {"cmd", "mc_cmd", "url"}) {
    auto s = field(j, key);
    if (!s.empty() && s != "null" && s.size() <= 8192)
      return s;
  }
  return {};
}
std::string media_url(std::string s) {
  if (s.rfind("ffmpeg ", 0) == 0)
    s = s.substr(7);
  else if (s.rfind("auto ", 0) == 0)
    s = s.substr(5);
  // Never run a command, accept a complete HTTP(S) URL only.
  if (!feature_http::safe_url(s, false))
    return {};
  CURLU *u = curl_url();
  if (!u)
    return {};
  char *host = nullptr;
  bool loop = false;
  if (curl_url_set(u, CURLUPART_URL, s.c_str(), 0) == CURLUE_OK &&
      curl_url_get(u, CURLUPART_HOST, &host, 0) == CURLUE_OK) {
    std::string h = host;
    std::transform(h.begin(), h.end(), h.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    loop = h == "localhost" || h == "127.0.0.1" || h == "[::1]";
    curl_free(host);
  }
  curl_url_cleanup(u);
  return loop ? std::string() : s;
}
class Client {
public:
  Source s;
  std::string url, token;
  const std::atomic<bool> *cancel;
  feature_http::Request request;
  Client(const Source &source, const std::atomic<bool> *c,
         const feature_http::Request &r)
      : s(source), url(endpoint(s.portal)), cancel(c),
        request(r ? r : feature_http::perform) {
    if (!valid(s))
      throw Failure(
          "Enter a portal URL and the MAC address assigned by your provider.");
    std::transform(s.mac.begin(), s.mac.end(), s.mac.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    auto h =
        call("stb", "handshake", {{"token", ""}, {"prehash", digest(s.mac)}});
    token = field(h, "token");
    if (token.empty() || token.size() > 4096 ||
        token.find_first_of("\r\n") != std::string::npos)
      throw Failure("The portal did not authorise this MAC address.");
    Params p{{"stb_type", "MAG250"},
             {"client_type", "STB"},
             {"hw_version", "1.7-BD-00"},
             {"image_version", "218"},
             {"hd", "1"},
             {"video_out", "hdmi"},
             {"num_banks", "2"},
             {"ver", "ImageDescription: 0.2.18-r14-pub-250; PORTAL version: "
                     "5.6.0; API Version: JS API version: 328; STB API "
                     "version: 134; Player Engine version: 0x566"},
             {"prehash", digest(s.mac)},
             {"auth_second_step", "0"},
             {"not_valid_token", field(h, "not_valid")},
             {"metrics", J{{"mac", s.mac},
                           {"model", "MAG250"},
                           {"type", "STB"},
                           {"random", field(h, "random")}}
                             .dump()}};
    if (!s.serial.empty())
      p["sn"] = s.serial;
    auto profile = call("stb", "get_profile", p);
    if (number(profile, "status") == 2) {
      if (s.username.empty() || s.password.empty())
        throw Failure("This portal also requires a username and password.");
      auto a = call("stb", "do_auth",
                    {{"login", s.username}, {"password", s.password}});
      if (a != J(true) && a != J(1))
        throw Failure("Portal login failed.");
      p["auth_second_step"] = "1";
      profile = call("stb", "get_profile", p);
    }
    if (!profile.is_object() || number(profile, "status") == 1 ||
        number(profile, "status") == 2 ||
        (number(profile, "blocked") == 1 ||
         (profile.contains("blocked") && profile["blocked"] == J(true))))
      throw Failure("The portal declined or blocked this account.");
  }
  J call(const std::string &type, const std::string &action,
         const Params &params = {}, size_t limit = metadata_limit) {
    check(cancel);
    feature_http::Query q;
    q.url =
        url + "?type=" + type + "&action=" + action + "&JsHttpRequest=1-xml";
    for (const auto &[key, val] : params)
      q.url += "&" + key + "=" + feature_http::encode(val);
    q.https_only = false;
    q.limit = limit;
    q.headers = {std::string("User-Agent: ") + agent,
                 "X-User-Agent: Model: MAG250; Link: WiFi",
                 "Referer: " + s.portal,
                 "Cookie: mac=" + feature_http::encode(s.mac) +
                     "; stb_lang=en; timezone=UTC"};
    if (!token.empty())
      q.headers.push_back("Authorization: Bearer " + token);
    auto r = request(q, cancel);
    check(cancel);
    if (!r.ok()) {
      if (r.status == 401 || r.status == 403)
        throw Failure("The portal rejected access. Check your account and "
                      "connection limit.");
      if (r.status >= 300 && r.status < 400)
        throw Failure(
            "The portal redirected the request. Enter its final portal URL.");
      throw Failure(r.error == "Response exceeds size limit"
                        ? "Portal response exceeds the size limit."
                        : "Could not reach the portal or read its response.");
    }
    auto j = parse(r.body);
    if (!j.is_object() || !j.contains("js") || j["js"].is_null())
      throw Failure("Portal returned an invalid response.");
    return j["js"];
  }
  template <class Visitor>
  void pages(const std::string &type, Params p, Visitor visit) {
    size_t received = 0, bytes = 0;
    int expected = -1;
    std::set<std::string> ids;
    for (size_t page = 1; page <= 2048; ++page) {
      p["p"] = std::to_string(page);
      auto j = call(type, "get_ordered_list", p);
      auto rows = j.find("data");
      if (!j.is_object() || rows == j.end() || !rows->is_array())
        throw Failure("Portal returned an invalid catalogue.");
      const auto total = number(j, "total_items", -1);
      if (page == 1)
        expected = total;
      if (total != expected || total > int(item_limit))
        throw Failure("Portal catalogue changed or exceeds the item limit.");
      bytes += j.dump().size();
      if (bytes > catalog_limit)
        throw Failure("Portal catalogue exceeds the size limit.");
      if (rows->empty()) {
        if (expected >= 0 && received != size_t(expected))
          throw Failure("Portal returned an incomplete catalogue.");
        return;
      }
      for (const auto &row : *rows) {
        check(cancel);
        auto id = field(row, "id");
        if (id.empty() || id.size() > 128 || !ids.insert(id).second)
          throw Failure("Portal repeated or omitted a catalogue identifier.");
        if (++received > item_limit ||
            (expected >= 0 && received > size_t(expected)))
          throw Failure("Portal catalogue exceeds the item limit.");
        visit(row);
      }
      if (expected >= 0 && received == size_t(expected))
        return;
    }
    throw Failure("Portal catalogue pagination did not finish.");
  }
};
std::vector<iptv::Category> categories(Client &c, const std::string &type) {
  auto j = c.call(type, "get_categories");
  if (!j.is_array() || j.size() > 10000)
    throw Failure("Portal returned invalid categories.");
  std::vector<iptv::Category> out;
  std::set<std::string> ids;
  for (const auto &r : j) {
    auto id = field(r, "id"), name = clean(field(r, "title"));
    if (id != "0" && id != "*" && !id.empty() && id.size() <= 128 &&
        !name.empty() && ids.insert(id).second)
      out.push_back({id, name});
  }
  return out;
}
} // namespace
Source source(const J &j) {
  return {field(j, "url"), field(j, "mac"), field(j, "username"),
          field(j, "password"), field(j, "serial")};
}
std::string endpoint(const std::string &value) {
  if (!feature_http::safe_url(value, false))
    return {};
  CURLU *u = curl_url();
  if (!u)
    return {};
  std::string out;
  char *p = nullptr;
  if (curl_url_set(u, CURLUPART_URL, value.c_str(), 0) == CURLUE_OK) {
    char *q = nullptr;
    if (curl_url_get(u, CURLUPART_QUERY, &q, 0) == CURLUE_OK) {
      curl_free(q);
      curl_url_cleanup(u);
      return {};
    }
    if (curl_url_get(u, CURLUPART_PATH, &p, 0) == CURLUE_OK) {
      std::string path = p;
      curl_free(p);
      p = nullptr;
      while (!path.empty() && path.back() == '/')
        path.pop_back();
      if (path.size() >= 2 && path.substr(path.size() - 2) == "/c")
        path.resize(path.size() - 2);
      if (path.size() < 4 || path.substr(path.size() - 4) != ".php")
        path += "/server/load.php";
      curl_url_set(u, CURLUPART_PATH, path.c_str(), 0);
      if (curl_url_get(u, CURLUPART_URL, &p, 0) == CURLUE_OK) {
        out = p;
        curl_free(p);
      }
    }
  }
  curl_url_cleanup(u);
  return out;
}
bool valid(const Source &s) {
  if (endpoint(s.portal).empty() || s.mac.size() != 17 ||
      s.username.size() > 256 || s.password.size() > 256 ||
      s.serial.size() > 256)
    return false;
  for (size_t i = 0; i < s.mac.size(); ++i)
    if (i % 3 == 2 ? s.mac[i] != ':' : !std::isxdigit((unsigned char)s.mac[i]))
      return false;
  return true;
}
live_tv::Result channels(const Source &s, const std::atomic<bool> *cancel,
                         const feature_http::Request &request) {
  live_tv::Result out;
  try {
    Client c(s, cancel, request);
    auto genres = c.call("itv", "get_genres");
    std::map<std::string, std::string> names;
    if (genres.is_array())
      for (const auto &g : genres)
        names[field(g, "id")] = clean(field(g, "title"));
    auto j = c.call("itv", "get_all_channels", {}, catalog_limit);
    if (!j.is_object() || !j.contains("data") || !j["data"].is_array() ||
        j["data"].size() > item_limit)
      throw Failure("Portal returned an invalid channel list.");
    std::set<std::string> ids;
    for (const auto &row : j["data"]) {
      check(cancel);
      auto id = field(row, "id"), cmd = command(row),
           name = clean(field(row, "name"));
      if (id.empty() || id.size() > 128 || name.empty() || cmd.empty())
        continue;
      if (!ids.insert(id).second)
        throw Failure("Portal returned duplicate channel identifiers.");
      live_tv::Channel ch;
      ch.id = id;
      ch.epg_id = id;
      ch.name = name;
      ch.group = names[field(row, "tv_genre_id")];
      ch.url = std::string(marker) + cmd;
      auto logo = field(row, "logo");
      if (feature_http::safe_url(logo, false))
        ch.logo = logo;
      out.channels.push_back(std::move(ch));
    }
  } catch (const Failure &e) {
    out.channels.clear();
    out.error = e.what();
  } catch (...) {
    out.channels.clear();
    out.error = "Portal returned an unsupported response.";
  }
  return out;
}
Link resolve(const Source &s, const std::string &cmd, bool vod, int episode,
             const std::atomic<bool> *cancel,
             const feature_http::Request &request) {
  Link out;
  try {
    if (cmd.empty() || cmd.size() > 8192 || episode < 0 || episode > 100000)
      throw Failure("Invalid portal playback command.");
    Client c(s, cancel, request);
    Params p{{"cmd", cmd},
             {"disable_ad", "0"},
             {"forced_storage", vod ? "undefined" : "0"}};
    if (!vod || episode > 0)
      p["series"] = episode > 0 ? std::to_string(episode) : "";
    auto j = c.call(vod ? "vod" : "itv", "create_link", p);
    out.url = media_url(command(j));
    if (out.url.empty())
      throw Failure("Portal returned no usable playback link. Refresh the "
                    "source or check your account.");
    // Portal Cookie/Authorization must never be forwarded to a CDN/player.
    out.headers = {std::string("User-Agent: ") + agent};
  } catch (const Failure &e) {
    out.error = e.what();
  } catch (...) {
    out.error = "Portal playback link could not be read.";
  }
  return out;
}
live_guide::Result guide(const Source &s, const std::string &channel,
                         int64_t now, const std::atomic<bool> *cancel,
                         const feature_http::Request &request) {
  live_guide::Result out;
  try {
    Client c(s, cancel, request);
    auto j =
        c.call("itv", "get_epg_info", {{"period", "24"}, {"ch_id", channel}},
               live_guide::max_bytes);
    if (j.is_object() && j.contains("data"))
      j = j["data"];
    if (j.is_object() && j.contains(channel))
      j = j[channel];
    if (!j.is_array())
      throw Failure("Portal returned no guide for this channel.");
    auto &list = out.channels[channel];
    std::set<int64_t> seen;
    for (const auto &row : j) {
      check(cancel);
      int64_t start = number(row, "start_timestamp"),
              end = number(row, "stop_timestamp");
      if (!end)
        end = number(row, "end_timestamp");
      auto title = clean(field(row, "name"), 512);
      if (title.empty())
        title = clean(field(row, "title"), 512);
      if (start <= 0 || end <= start || end < now - 86400 ||
          start > now + 7 * 86400 || title.empty() ||
          !seen.insert(start).second)
        continue;
      if (list.size() >= 96) {
        out.limited = true;
        break;
      }
      list.push_back({start, end, title});
    }
    std::sort(list.begin(), list.end(),
              [](const auto &a, const auto &b) { return a.start < b.start; });
  } catch (const Failure &e) {
    out.error = e.what();
  } catch (...) {
    out.error = "Portal guide could not be read.";
  }
  return out;
}
iptv::Capabilities probe(const Source &s, const std::atomic<bool> *cancel,
                         const feature_http::Request &request) {
  iptv::Capabilities out;
  try {
    Client c(s, cancel, request);
    try {
      out.movies = categories(c, "vod");
      out.movies_ok = true;
    } catch (const Failure &e) {
      out.error = e.what();
    }
    try {
      out.series = categories(c, "series");
      out.series_ok = true;
    } catch (const Failure &e) {
      out.error = e.what();
    }
  } catch (const Failure &e) {
    out.error = e.what();
  } catch (...) {
    out.error = "Portal categories could not be read.";
  }
  return out;
}
iptv::Catalog catalog(const Source &s, iptv::Kind kind,
                      const std::string &category,
                      const std::atomic<bool> *cancel,
                      const feature_http::Request &request) {
  iptv::Catalog out;
  try {
    Client c(s, cancel, request);
    Params p{{"sortby", "name"}};
    if (category.size() > 128)
      throw Failure("Invalid portal category.");
    if (kind == iptv::Kind::episodes)
      p["movie_id"] = category;
    else
      p["category"] = category.empty() ? "0" : category;
    std::set<std::pair<int, int>> episode_coordinates;
    c.pages(
        kind == iptv::Kind::movies ? "vod" : "series", p, [&](const J &row) {
          iptv::Entry e;
          e.id = field(row, "id");
          e.name = clean(field(row, "name"));
          if (e.name.empty())
            e.name = clean(field(row, "title"));
          e.category = field(row, "category_id");
          e.command = command(row);
          e.year = clean(field(row, "year"), 4);
          auto poster = field(row, "screenshot_uri");
          if (feature_http::safe_url(poster, false))
            e.poster = poster;
          if (kind != iptv::Kind::episodes) {
            if (!category.empty() && category != "0" && e.category != category)
              throw Failure("Portal ignored the category filter.");
            if (!e.name.empty())
              out.entries.push_back(e);
            return;
          }
          e.season = number(row, "season_number", number(row, "season", -1));
          if (e.season < 0) {
            std::smatch match;
            const std::regex season_name("^(?:season[ ]*|s)([0-9]{1,4})$",
                                         std::regex::icase);
            if (std::regex_match(e.name, match, season_name))
              e.season = std::stoi(match[1]);
          }
          if (e.season < 0)
            throw Failure("Portal season numbering is unavailable.");
          auto add = [&](int n) {
            if (n <= 0 || n > 100000 || e.command.empty())
              return;
            if (!episode_coordinates.insert({e.season, n}).second)
              throw Failure("Portal returned ambiguous episode coordinates.");
            auto ep = e;
            ep.episode = n;
            ep.id = category + ":" + ep.id + ":" + std::to_string(n);
            ep.name = "Episode " + std::to_string(n);
            out.entries.push_back(std::move(ep));
          };
          if (row.contains("series") && row["series"].is_array())
            for (const auto &v : row["series"]) {
              J n{{"n", v}};
              add(number(n, "n"));
              if (out.entries.size() > item_limit)
                throw Failure("Portal series exceeds the episode limit.");
            }
          else
            add(number(row, "episode_num", number(row, "episode_number")));
        });
    if (out.entries.size() > item_limit)
      throw Failure("Portal catalogue exceeds the item limit.");
    if (kind == iptv::Kind::episodes)
      std::sort(out.entries.begin(), out.entries.end(),
                [](const auto &a, const auto &b) {
                  return std::tie(a.season, a.episode) <
                         std::tie(b.season, b.episode);
                });
  } catch (const Failure &e) {
    out.entries.clear();
    out.error = e.what();
  } catch (...) {
    out.entries.clear();
    out.error = "Portal catalogue could not be read.";
  }
  return out;
}
} // namespace stalker
