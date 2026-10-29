#include "iptv_library.h"
#include "live_tv.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <curl/curl.h>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace iptv {
std::string failure_message(const std::string &error, long status,
                            bool playback) {
  std::string lower = error.substr(0, 8192);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (!status) {
    const auto pos = lower.find("http ");
    if (pos != std::string::npos && pos + 8 <= lower.size()) {
      const auto code = lower.substr(pos + 5, 3);
      if (code.find_first_not_of("0123456789") == std::string::npos &&
          (pos + 8 == lower.size() ||
           !std::isdigit(static_cast<unsigned char>(lower[pos + 8]))))
        status = std::stoi(code);
    }
  }
  if (status >= 500 && status <= 599)
    return "IPTV provider unavailable: its server returned HTTP " +
           std::to_string(status) +
           ". The service may be experiencing an outage. Try again later or "
           "contact your IPTV provider.";
  if (status == 401 || status == 403)
    return "Your IPTV provider rejected access. Check your subscription, login "
           "and connection limit, or contact the provider.";
  if (status == 404 || status == 410)
    return "This title is unavailable from your IPTV provider. Refresh the "
           "IPTV Library or choose another title.";
  if (status == 429)
    return "Your IPTV provider is limiting requests or connections. Stop other "
           "IPTV sessions and try again later.";
  if (status >= 400 && status <= 499)
    return "Your IPTV provider rejected this request (HTTP " +
           std::to_string(status) +
           "). Try again later or contact the provider.";
  for (const auto *term :
       {"timed out", "timeout", "could not resolve", "couldn't resolve",
        "could not connect", "couldn't connect", "failed to connect",
        "connection refused", "connection reset", "network is unreachable"}) {
    if (lower.find(term) != std::string::npos)
      return "Cannot reach your IPTV provider. Check your internet connection; "
             "the provider may be offline. Try again later.";
  }
  if (lower.find("web page instead of video") != std::string::npos)
    return "Your IPTV provider returned a web page instead of video. Refresh "
           "the IPTV Library or contact the provider.";
  return playback
             ? "IPTV playback could not start or continue. Try another title "
               "or player. If it continues, save the playback log in Settings "
               "for troubleshooting."
             : "Could not load the IPTV provider's library. Check your "
               "connection and account, then retry. Saved titles are retained.";
}
namespace {
std::mutex cache_mutex;
std::atomic<unsigned long> cache_sequence{0};
bool cancelled(const std::atomic<bool> *c) { return c && c->load(); }
std::string value(const Json &j, const char *key) {
  if (!j.is_object() || !j.contains(key))
    return {};
  const auto &v = j[key];
  return v.is_string()           ? v.get<std::string>()
         : v.is_number_integer() ? v.dump()
                                 : std::string();
}
Json parse_json(const std::string &body) {
  bool deep = false;
  auto out = Json::parse(
      body,
      [&](int depth, Json::parse_event_t, Json &) {
        if (depth > 64) {
          deep = true;
          return false;
        }
        return true;
      },
      false);
  return deep ? Json(Json::value_t::discarded) : out;
}
std::string clean(std::string s, size_t limit = 256) {
  s.erase(std::remove_if(s.begin(), s.end(),
                         [](unsigned char c) { return c < 32 || c == 127; }),
          s.end());
  if (s.size() > limit)
    s.resize(limit);
  return s;
}
bool number(const std::string &s) {
  return !s.empty() && s.size() <= 20 &&
         s.find_first_not_of("0123456789") == std::string::npos;
}
std::string encode(const std::string &s) {
  char *p = curl_easy_escape(nullptr, s.c_str(), int(s.size()));
  if (!p)
    return {};
  std::string out = p;
  curl_free(p);
  return out;
}
std::string part(CURLU *u, CURLUPart key) {
  char *p = nullptr;
  if (curl_url_get(u, key, &p, 0) != CURLUE_OK)
    return {};
  std::string s = p;
  curl_free(p);
  return s;
}
std::string decode(const std::string &s) {
  int n = 0;
  auto input = s;
  std::replace(input.begin(), input.end(), '+', ' ');
  char *p = curl_easy_unescape(nullptr, input.c_str(), int(input.size()), &n);
  if (!p)
    return {};
  std::string out(p, size_t(n));
  curl_free(p);
  return out;
}
bool playlist_account(Account &a) {
  if (!live_tv::http_url(a.server))
    return false;
  auto *u = curl_url();
  if (!u)
    return false;
  bool ok = curl_url_set(u, CURLUPART_URL, a.server.c_str(), 0) == CURLUE_OK &&
            part(u, CURLUPART_USER).empty() &&
            part(u, CURLUPART_PASSWORD).empty();
  auto path = part(u, CURLUPART_PATH);
  ok = ok && path.size() >= 8 && path.substr(path.size() - 8) == "/get.php";
  auto query = part(u, CURLUPART_QUERY);
  std::set<std::string> seen;
  while (ok && !query.empty()) {
    auto amp = query.find('&');
    auto item = query.substr(0, amp);
    query = amp == std::string::npos ? "" : query.substr(amp + 1);
    auto eq = item.find('=');
    if (eq == std::string::npos)
      continue;
    auto key = decode(item.substr(0, eq));
    if (key != "username" && key != "password")
      continue;
    if (!seen.insert(key).second) {
      ok = false;
      break;
    }
    auto val = decode(item.substr(eq + 1));
    if (key == "username")
      a.user = val;
    else
      a.password = val;
  }
  if (ok) {
    path.resize(path.size() - 8);
    if (path.empty())
      path = "/";
    curl_url_set(u, CURLUPART_PATH, path.c_str(), 0);
    curl_url_set(u, CURLUPART_QUERY, nullptr, 0);
    curl_url_set(u, CURLUPART_FRAGMENT, nullptr, 0);
    a.server = part(u, CURLUPART_URL);
  }
  curl_url_cleanup(u);
  return ok && !a.user.empty() && !a.password.empty();
}
bool valid_server(const std::string &s) {
  if (!live_tv::http_url(s) || s.find_first_of("?#") != std::string::npos)
    return false;
  auto *u = curl_url();
  if (!u)
    return false;
  bool ok = curl_url_set(u, CURLUPART_URL, s.c_str(), 0) == CURLUE_OK &&
            part(u, CURLUPART_USER).empty() &&
            part(u, CURLUPART_PASSWORD).empty();
  curl_url_cleanup(u);
  return ok;
}
std::string extension(std::string s) {
  for (auto &c : s)
    c = char(std::tolower((unsigned char)c));
  if (s.empty())
    return "mp4";
  return s.size() <= 8 &&
                 s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789") ==
                     std::string::npos
             ? s
             : std::string();
}
std::string poster(std::string s) {
  if (!live_tv::http_url(s) || s.size() > 2048)
    return {};
  // Artwork must not persist authenticated URLs in the shared image cache/logs.
  auto *u = curl_url();
  if (!u)
    return {};
  bool ok = curl_url_set(u, CURLUPART_URL, s.c_str(), 0) == CURLUE_OK &&
            part(u, CURLUPART_USER).empty() &&
            part(u, CURLUPART_PASSWORD).empty() &&
            part(u, CURLUPART_QUERY).empty();
  curl_url_cleanup(u);
  return ok ? s : std::string();
}
std::string api(const Account &a) {
  return a.server + "/player_api.php?username=" + encode(a.user) +
         "&password=" + encode(a.password);
}
HttpResponse get(const std::string &url, size_t limit,
                 const std::atomic<bool> *cancel, const Request &request) {
  if (cancelled(cancel))
    return {0, {}, "IPTV request cancelled.", {}};
  auto r = request ? request(url, limit, cancel)
                   : live_tv::download(url, cancel, false, limit);
  if (cancelled(cancel))
    r.error = "IPTV request cancelled.";
  else if (!r.ok() || !r.error.empty())
    r.error = failure_message(r.error, r.status, false);
  else if (r.body.size() > limit)
    r.error = "The IPTV response exceeds the supported import limit.";
  return r;
}
std::vector<Category> parse_categories(const std::string &body, bool &ok) {
  ok = false;
  std::vector<Category> out;
  if (body.size() > max_info_bytes)
    return out;
  auto j = parse_json(body);
  if (!j.is_array() || j.size() > 10000)
    return out;
  std::set<std::string> seen;
  for (const auto &x : j) {
    auto id = value(x, "category_id"), name = clean(value(x, "category_name"));
    if (number(id) && !name.empty() && seen.insert(id).second)
      out.push_back({id, name});
  }
  ok = true;
  return out;
}
Json entry_json(const Entry &e) {
  return {{"id", e.id},
          {"name", e.name},
          {"category", e.category},
          {"poster", e.poster},
          {"extension", e.extension},
          {"year", e.year},
          {"season", e.season},
          {"episode", e.episode}};
}
Entry entry_from_json(const Json &j) {
  Entry e;
  e.id = value(j, "id");
  e.name = clean(value(j, "name"));
  e.category = value(j, "category");
  e.poster = poster(value(j, "poster"));
  e.extension = extension(value(j, "extension"));
  e.year = clean(value(j, "year"), 32);
  auto integer = [&](const char *key) {
    auto v = value(j, key);
    return number(v) && v.size() < 6 ? std::stoi(v) : 0;
  };
  e.season = integer("season");
  e.episode = integer("episode");
  return e;
}
// Retain only small scalar fields from each list item, skipping nested
// metadata.
class Items : public nlohmann::json_sax<Json> {
  int depth_ = 0;
  std::string key_;
  Json item_ = Json::object();
  std::function<bool(const Json &)> emit_;
  const std::atomic<bool> *cancel_;
  bool scalar(const std::string &s) {
    if (depth_ < 2 || cancelled(cancel_))
      return false;
    if (depth_ == 2 && !key_.empty())
      item_[key_] =
          clean(s, key_ == "cover" || key_ == "stream_icon" ? 2048 : 256);
    return true;
  }

public:
  Items(std::function<bool(const Json &)> emit, const std::atomic<bool> *c)
      : emit_(std::move(emit)), cancel_(c) {}
  bool null() override { return depth_ > 1 && !cancelled(cancel_); }
  bool boolean(bool) override { return depth_ > 1 && !cancelled(cancel_); }
  bool number_integer(number_integer_t n) override {
    return scalar(std::to_string(n));
  }
  bool number_unsigned(number_unsigned_t n) override {
    return scalar(std::to_string(n));
  }
  bool number_float(number_float_t, const string_t &) override {
    return depth_ > 1 && !cancelled(cancel_);
  }
  bool string(string_t &s) override { return scalar(s); }
  bool binary(binary_t &) override { return false; }
  bool start_object(size_t) override {
    if (depth_ == 0 || depth_ >= 64 || cancelled(cancel_))
      return false;
    if (++depth_ == 2) {
      item_ = Json::object();
      key_.clear();
    }
    return true;
  }
  bool key(string_t &s) override {
    if (depth_ == 2)
      key_ = (s == "stream_id" || s == "series_id" || s == "name" ||
              s == "category_id" || s == "stream_icon" || s == "cover" ||
              s == "container_extension" || s == "year" || s == "releaseDate" ||
              s == "release_date")
                 ? s
                 : std::string();
    return !cancelled(cancel_);
  }
  bool end_object() override {
    if (depth_ == 2 && !emit_(item_))
      return false;
    --depth_;
    return !cancelled(cancel_);
  }
  bool start_array(size_t) override {
    if (depth_ == 1 || depth_ >= 64 || cancelled(cancel_))
      return false;
    ++depth_;
    return true;
  }
  bool end_array() override {
    --depth_;
    return !cancelled(cancel_);
  }
  bool parse_error(size_t, const std::string &,
                   const nlohmann::detail::exception &) override {
    return false;
  }
};
bool write_all(int fd, const std::string &s) {
  size_t at = 0;
  while (at < s.size()) {
    auto n = ::write(fd, s.data() + at, s.size() - at);
    if (n <= 0)
      return false;
    at += size_t(n);
  }
  return true;
}
std::string hash(const std::string &s) {
  uint64_t h = 14695981039346656037ull;
  for (unsigned char c : s) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return std::to_string(h);
}
std::string cache_key(const std::string &source, Kind k,
                      const std::string &category) {
  return hash(source) + "-" + kind_name(k) + "-" + hash(category) + ".jsonl";
}
struct CacheFile {
  std::string path;
  size_t bytes;
  time_t modified;
};
std::vector<CacheFile> cache_files(const std::string &dir) {
  std::vector<CacheFile> files;
  DIR *d = opendir(dir.c_str());
  if (!d)
    return files;
  while (auto *ent = readdir(d)) {
    std::string name = ent->d_name;
    if (name.size() < 6 || name.substr(name.size() - 6) != ".jsonl")
      continue;
    struct stat st{};
    auto path = dir + "/" + name;
    if (lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode))
      files.push_back({path, size_t(st.st_size), st.st_mtime});
  }
  closedir(d);
  return files;
}
} // namespace
std::string kind_name(Kind k) {
  return k == Kind::movies   ? "movies"
         : k == Kind::series ? "series"
                             : "episodes";
}
bool library_type(const std::string &s) {
  return s == "iptv-movie" || s == "iptv-series";
}
std::string identity(const std::string &source, Kind k, const std::string &id) {
  return "iptv:" + source + ":" + kind_name(k) + ":" + id;
}
std::vector<Account> accounts(const Json &sources) {
  std::vector<Account> out;
  if (!sources.is_array())
    return out;
  for (const auto &s : sources) {
    Account a{value(s, "id"), clean(value(s, "name")), value(s, "url"),
              value(s, "username"), value(s, "password")};
    if (value(s, "type") == "m3u") {
      if (!playlist_account(a))
        continue;
    } else if (value(s, "type") != "xtream")
      continue;
    while (!a.server.empty() && a.server.back() == '/')
      a.server.pop_back();
    if (!a.id.empty() && a.id.size() <= 128 &&
        a.id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU"
                               "VWXYZ0123456789_-") == std::string::npos &&
        valid_server(a.server) && !a.user.empty() && !a.password.empty() &&
        a.user.size() <= 256 && a.password.size() <= 256 && out.size() < 16) {
      if (a.name.empty())
        a.name = "IPTV service";
      out.push_back(std::move(a));
    }
  }
  return out;
}
std::string playback_url(const Account &a, Kind k, const Entry &e) {
  if (k == Kind::series || !valid_server(a.server) || a.user.empty() ||
      a.password.empty() || !number(e.id))
    return {};
  auto ext = extension(e.extension);
  if (ext.empty())
    return {};
  return a.server + (k == Kind::movies ? "/movie/" : "/series/") +
         encode(a.user) + "/" + encode(a.password) + "/" + e.id + "." + ext;
}
Catalog parse_catalog(const std::string &body, Kind kind,
                      const std::atomic<bool> *cancel) {
  Catalog r;
  if (body.size() > max_catalog_bytes || kind == Kind::episodes)
    return {{}, "The IPTV catalog is too large or has an invalid type."};
  std::set<std::string> seen;
  Items sax(
      [&](const Json &j) {
        Entry e;
        e.id = value(j, kind == Kind::movies ? "stream_id" : "series_id");
        e.name = clean(value(j, "name"));
        if (!number(e.id) || e.name.empty() || !seen.insert(e.id).second)
          return true;
        e.category = value(j, "category_id");
        e.poster =
            poster(value(j, kind == Kind::movies ? "stream_icon" : "cover"));
        e.extension = extension(value(j, "container_extension"));
        e.year = clean(value(j, "year"), 32);
        if (e.year.empty()) {
          e.year = value(j, "releaseDate");
          if (e.year.empty())
            e.year = value(j, "release_date");
          if (e.year.size() > 4)
            e.year.resize(4);
        }
        if (r.entries.size() == max_items) {
          r.error = "The IPTV catalog exceeds 100,000 items. Choose a smaller "
                    "category.";
          return false;
        }
        r.entries.push_back(std::move(e));
        return true;
      },
      cancel);
  if (!Json::sax_parse(body, &sax)) {
    r.entries.clear();
    if (r.error.empty())
      r.error = cancelled(cancel)
                    ? "IPTV request cancelled."
                    : "The IPTV server returned an invalid catalog.";
  }
  return r;
}
Catalog parse_episodes(const std::string &body,
                       const std::atomic<bool> *cancel) {
  Catalog r;
  if (body.size() > max_info_bytes)
    return {{}, "The series details exceed the 8 MiB limit."};
  auto j = parse_json(body);
  if (!j.is_object() || !j.contains("episodes") ||
      (!j["episodes"].is_object() && !j["episodes"].is_array()))
    return {{}, "The IPTV server returned invalid series details."};
  std::set<std::string> seen;
  auto add = [&](const Json &x, const std::string &season) {
    if (cancelled(cancel)) {
      r.error = "IPTV request cancelled.";
      return;
    }
    Entry e;
    e.id = value(x, "id");
    e.name = clean(value(x, "title"));
    if (!number(e.id) || !seen.insert(e.id).second)
      return;
    auto s = value(x, "season");
    if (s.empty())
      s = season;
    auto n = value(x, "episode_num");
    if (!number(s) || s.size() > 4 || !number(n) || n.size() > 4)
      return;
    e.season = std::stoi(s);
    e.episode = std::stoi(n);
    e.extension = extension(value(x, "container_extension"));
    if (e.name.empty())
      e.name = "Episode " + n;
    if (x.contains("info"))
      e.poster = poster(value(x["info"], "movie_image"));
    if (r.entries.size() == 10000) {
      r.error = "This series exceeds the 10,000 episode limit.";
      return;
    }
    r.entries.push_back(std::move(e));
  };
  const auto &episodes = j["episodes"];
  if (episodes.is_array()) {
    for (const auto &x : episodes)
      add(x, "0");
  } else
    for (auto it = episodes.begin(); it != episodes.end(); ++it) {
      if (!it.value().is_array()) {
        r.error = "The IPTV server returned invalid season data.";
        break;
      }
      for (const auto &x : it.value())
        add(x, it.key());
    }
  if (!r.error.empty()) {
    r.entries.clear();
    return r;
  }
  std::stable_sort(r.entries.begin(), r.entries.end(),
                   [](const auto &a, const auto &b) {
                     return a.season != b.season ? a.season < b.season
                                                 : a.episode < b.episode;
                   });
  return r;
}
Catalog fetch(const Account &a, Kind kind, const std::string &category,
              const std::atomic<bool> *cancel, const Request &request) {
  if (!valid_server(a.server) || a.user.empty() || a.password.empty() ||
      (!category.empty() && !number(category)))
    return {{}, "Invalid IPTV account or category."};
  auto url = api(a) + "&action=" +
             (kind == Kind::movies   ? "get_vod_streams"
              : kind == Kind::series ? "get_series"
                                     : "get_series_info");
  if (!category.empty())
    url += (kind == Kind::episodes ? "&series_id=" : "&category_id=") +
           encode(category);
  auto r = get(url, kind == Kind::episodes ? max_info_bytes : max_catalog_bytes,
               cancel, request);
  if (!r.error.empty())
    return {{}, r.error};
  auto catalog = kind == Kind::episodes ? parse_episodes(r.body, cancel)
                                        : parse_catalog(r.body, kind, cancel);
  for (auto &entry : catalog.entries)
    for (const auto &credential : {a.user, a.password}) {
      const auto segment = "/" + encode(credential) + "/";
      if (entry.poster.find(segment) != std::string::npos)
        entry.poster.clear();
    }
  return catalog;
}
Capabilities probe(const Account &a, const std::atomic<bool> *cancel,
                   const Request &request) {
  Capabilities c;
  if (!valid_server(a.server) || a.user.empty() || a.password.empty()) {
    c.error = "Invalid IPTV account.";
    return c;
  }
  auto auth = get(api(a), max_info_bytes, cancel, request);
  if (!auth.error.empty()) {
    c.error = auth.error;
    return c;
  }
  auto j = parse_json(auth.body);
  if (!j.is_object() || !j.contains("user_info") ||
      value(j["user_info"], "auth") != "1") {
    c.error = "IPTV login was rejected. Check the subscription and retry.";
    return c;
  }
  auto status = value(j["user_info"], "status");
  for (auto &x : status)
    x = char(std::tolower((unsigned char)x));
  if (!status.empty() && status != "active") {
    c.error = "The IPTV subscription is not active.";
    return c;
  }
  for (auto kind : {Kind::movies, Kind::series}) {
    auto r = get(api(a) + "&action=" +
                     (kind == Kind::movies ? "get_vod_categories"
                                           : "get_series_categories"),
                 max_info_bytes, cancel, request);
    bool ok = false;
    auto cats = r.error.empty() ? parse_categories(r.body, ok)
                                : std::vector<Category>{};
    if (ok && cats.empty()) {
      auto list = fetch(a, kind, "", cancel, request);
      ok = list.error.empty();
      if (ok && !list.entries.empty())
        cats.push_back(
            {"", kind == Kind::movies ? "All movies" : "All series"});
      if (!ok)
        c.error = list.error;
    }
    if (!r.error.empty())
      c.error = r.error;
    else if (!ok && c.error.empty())
      c.error = "The IPTV server returned invalid categories. Retry to check "
                "availability.";
    if (kind == Kind::movies) {
      c.movies = std::move(cats);
      c.movies_ok = ok;
    } else {
      c.series = std::move(cats);
      c.series_ok = ok;
    }
  }
  return c;
}
std::vector<Category> categories(const Json &state, const std::string &source,
                                 Kind kind) {
  std::vector<Category> out;
  if (!state.is_object() || !state.contains("sources") ||
      !state["sources"].is_object() || !state["sources"].contains(source))
    return out;
  const auto &s = state["sources"][source];
  auto key = kind_name(kind);
  if (!s.is_object() || !s.contains(key) || !s[key].is_array())
    return out;
  for (const auto &c : s[key]) {
    auto id = value(c, "id"), name = clean(value(c, "name"));
    if ((id.empty() || number(id)) && !name.empty() && out.size() < 10000)
      out.push_back({id, name});
  }
  return out;
}
void apply_capabilities(Json &state, const std::string &source,
                        const Capabilities &c) {
  if (!state.is_object())
    state = Json::object();
  if (!state.contains("sources") || !state["sources"].is_object())
    state["sources"] = Json::object();
  auto &target = state["sources"][source];
  if (!target.is_object())
    target = Json::object();
  for (auto kind : {Kind::movies, Kind::series}) {
    if (!(kind == Kind::movies ? c.movies_ok : c.series_ok))
      continue;
    target[kind_name(kind)] = Json::array();
    for (const auto &x : kind == Kind::movies ? c.movies : c.series)
      target[kind_name(kind)].push_back({{"id", x.id}, {"name", x.name}});
  }
}
void reconcile(Json &state, const std::vector<Account> &accounts) {
  if (!state.is_object())
    state = Json::object();
  if (!state.contains("sources") || !state["sources"].is_object())
    state["sources"] = Json::object();
  for (auto it = state["sources"].begin(); it != state["sources"].end();) {
    bool present = std::any_of(accounts.begin(), accounts.end(),
                               [&](const auto &a) { return a.id == it.key(); });
    if (!present)
      it = state["sources"].erase(it);
    else
      ++it;
  }
  if (!state.contains("progress") || !state["progress"].is_object())
    state["progress"] = Json::object();
  for (auto it = state["progress"].begin(); it != state["progress"].end();) {
    bool present =
        std::any_of(accounts.begin(), accounts.end(), [&](const auto &a) {
          return it.key().rfind("iptv:" + a.id + ":", 0) == 0;
        });
    if (!present)
      it = state["progress"].erase(it);
    else
      ++it;
  }
}
bool available(const Json &state, const std::vector<Account> &accounts) {
  for (const auto &a : accounts)
    if (!categories(state, a.id, Kind::movies).empty() ||
        !categories(state, a.id, Kind::series).empty())
      return true;
  return false;
}
bool save_private(const std::string &path, const Json &value) {
  auto body = value.dump();
  if (body.size() > max_info_bytes)
    return false;
  const auto tmp = path + ".tmp";
  int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    return false;
  bool ok = fchmod(fd, 0600) == 0 && write_all(fd, body);
  ok = (close(fd) == 0) && ok;
  if (ok)
    ok = rename(tmp.c_str(), path.c_str()) == 0;
  if (!ok)
    unlink(tmp.c_str());
  return ok;
}
Json load_private(const std::string &path) {
  struct stat st{};
  if (lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
      size_t(st.st_size) > max_info_bytes)
    return Json::object();
  std::ifstream in(path);
  std::string body((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  auto out = parse_json(body);
  return out.is_object() ? out : Json::object();
}
bool save_cache(const std::string &dir, const std::string &source, Kind kind,
                const std::string &category, const Catalog &catalog,
                const std::atomic<bool> *cancel) {
  std::lock_guard<std::mutex> lock(cache_mutex);
  if (!catalog.error.empty() || catalog.entries.size() > max_items ||
      cancelled(cancel))
    return false;
  mkdir(dir.c_str(), 0700);
  const auto path = dir + "/" + cache_key(source, kind, category),
             tmp = path + ".tmp-" + std::to_string(cache_sequence.fetch_add(1));
  int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    return false;
  std::string header = Json{{"version", 1},
                            {"source", source},
                            {"kind", kind_name(kind)},
                            {"category", category}}
                           .dump() +
                       "\n";
  bool ok = fchmod(fd, 0600) == 0 && write_all(fd, header);
  size_t bytes = header.size();
  for (const auto &e : catalog.entries) {
    auto line = entry_json(e).dump() + "\n";
    if (cancelled(cancel) || line.size() > max_cache_file - bytes) {
      ok = false;
      break;
    }
    bytes += line.size();
    if (!write_all(fd, line)) {
      ok = false;
      break;
    }
  }
  ok = (close(fd) == 0) && ok;
  if (ok && !cancelled(cancel))
    ok = rename(tmp.c_str(), path.c_str()) == 0;
  else
    ok = false;
  if (!ok) {
    unlink(tmp.c_str());
    return false;
  }
  auto files = cache_files(dir);
  size_t total = 0;
  for (const auto &f : files)
    total += f.bytes;
  std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
    return a.modified < b.modified;
  });
  for (const auto &f : files)
    if (total > max_cache_total && f.path != path) {
      if (unlink(f.path.c_str()) == 0)
        total -= f.bytes;
    }
  return true;
}
bool load_cache(const std::string &dir, const std::string &source, Kind kind,
                const std::string &category, Catalog &out,
                const std::atomic<bool> *cancel) {
  auto path = dir + "/" + cache_key(source, kind, category);
  struct stat st{};
  if (lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
      size_t(st.st_size) > max_cache_file)
    return false;
  std::ifstream in(path);
  std::string line;
  Catalog loaded;
  if (!std::getline(in, line) || line.size() > 1024)
    return false;
  auto h = parse_json(line);
  if (!h.is_object() ||
      (!h.contains("version") || !h["version"].is_number_integer() ||
       h["version"] != 1) ||
      value(h, "source") != source || value(h, "kind") != kind_name(kind) ||
      value(h, "category") != category)
    return false;
  while (std::getline(in, line)) {
    if (cancelled(cancel) || line.size() > 8192 ||
        loaded.entries.size() >= max_items)
      return false;
    auto j = parse_json(line);
    if (!j.is_object())
      return false;
    auto e = entry_from_json(j);
    if (!number(e.id) || e.name.empty())
      return false;
    loaded.entries.push_back(std::move(e));
  }
  if (!in.eof())
    return false;
  out = std::move(loaded);
  return true;
}
// Small UTF-8 fold for controller navigation/search, independent of locale.
// Unsupported scripts retain their text for exact search and group under #.
std::string search_key(const std::string &text) {
  std::string out;
  for (size_t i = 0; i < text.size();) {
    const auto start = i;
    unsigned char c = text[i++];
    unsigned cp = c;
    int tail = c < 0x80                   ? 0
               : (c >= 0xc2 && c < 0xe0)  ? 1
               : (c < 0xf0 && c >= 0xe0)  ? 2
               : (c <= 0xf4 && c >= 0xf0) ? 3
                                          : -1;
    if (tail < 0 || i + size_t(tail) > text.size()) {
      out += '?';
      continue;
    }
    cp &= tail ? (1u << (6 - tail)) - 1 : 0x7f;
    bool valid = true;
    for (int t = 0; t < tail; ++t) {
      unsigned char b = text[i];
      if ((b & 0xc0) != 0x80) {
        valid = false;
        break;
      }
      cp = (cp << 6) | (b & 63);
      ++i;
    }
    if (!valid) {
      out += '?';
      continue;
    }
    if (cp >= 0x300 && cp <= 0x36f)
      continue;
    if (cp < 128) {
      out += char(std::tolower(static_cast<unsigned char>(cp)));
      continue;
    }
    const char *fold = nullptr;
    if ((cp >= 0xc0 && cp <= 0xc5) || (cp >= 0xe0 && cp <= 0xe5) ||
        cp == 0x100 || cp == 0x101 || cp == 0x102 || cp == 0x103 ||
        cp == 0x104 || cp == 0x105)
      fold = "a";
    else if (cp == 0xc6 || cp == 0xe6)
      fold = "ae";
    else if (cp == 0xc7 || cp == 0xe7 || (cp >= 0x106 && cp <= 0x10d))
      fold = "c";
    else if ((cp >= 0xc8 && cp <= 0xcb) || (cp >= 0xe8 && cp <= 0xeb) ||
             (cp >= 0x112 && cp <= 0x11b))
      fold = "e";
    else if ((cp >= 0xcc && cp <= 0xcf) || (cp >= 0xec && cp <= 0xef) ||
             (cp >= 0x128 && cp <= 0x131))
      fold = "i";
    else if (cp == 0xd1 || cp == 0xf1 || (cp >= 0x143 && cp <= 0x148))
      fold = "n";
    else if ((cp >= 0xd2 && cp <= 0xd6) || (cp >= 0xf2 && cp <= 0xf6) ||
             cp == 0xd8 || cp == 0xf8 || (cp >= 0x14c && cp <= 0x151))
      fold = "o";
    else if ((cp >= 0xd9 && cp <= 0xdc) || (cp >= 0xf9 && cp <= 0xfc) ||
             (cp >= 0x168 && cp <= 0x173))
      fold = "u";
    else if (cp == 0xdd || cp == 0xfd || cp == 0xff || cp == 0x178)
      fold = "y";
    else if (cp == 0xdf || cp == 0x1e9e)
      fold = "ss";
    else if (cp == 0x141 || cp == 0x142)
      fold = "l";
    else if (cp >= 0x15a && cp <= 0x161)
      fold = "s";
    else if (cp >= 0x179 && cp <= 0x17e)
      fold = "z";
    else if (cp >= 0x154 && cp <= 0x159)
      fold = "r";
    if (fold)
      out += fold;
    else
      out.append(text, start, i - start);
  }
  return out;
}
std::string initial(const std::string &name) {
  const auto key = search_key(name);
  const auto i = key.find_first_not_of(" \t\r\n");
  return i != std::string::npos && key[i] >= 'a' && key[i] <= 'z'
             ? std::string(1, char(key[i] - 'a' + 'A'))
             : "#";
}
SearchResults search_saved(const std::string &dir,
                           const std::vector<Account> &accounts,
                           const std::string &query,
                           const std::atomic<bool> *cancel) {
  SearchResults result;
  const auto key = search_key(query);
  if (key.empty() || key.size() > 512 || cancelled(cancel))
    return result;
  auto files = cache_files(dir);
  // Newer saved categories win when a title appears in both All and category
  // caches.
  std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
    return a.modified != b.modified ? a.modified > b.modified : a.path < b.path;
  });
  std::set<std::string> seen;
  size_t bytes = 0, count = 0;
  for (const auto &f : files) {
    if (cancelled(cancel))
      return {};
    if (++count > 512 || f.bytes > max_cache_total - bytes) {
      result.limited = true;
      break;
    }
    bytes += f.bytes;
    if (f.bytes > max_cache_file)
      continue;
    std::ifstream in(f.path);
    std::string line;
    if (!std::getline(in, line) || line.size() > 1024)
      continue;
    auto h = parse_json(line);
    const auto source = value(h, "source"), kind = value(h, "kind"),
               category = value(h, "category");
    const auto a = std::find_if(accounts.begin(), accounts.end(),
                                [&](const auto &a) { return a.id == source; });
    if (a == accounts.end() || (kind != "movies" && kind != "series"))
      continue;
    const auto k = kind == "movies" ? Kind::movies : Kind::series;
    if (f.path != dir + "/" + cache_key(source, k, category))
      continue;
    Catalog catalog;
    if (!load_cache(dir, source, k, category, catalog, cancel))
      continue;
    for (auto &e : catalog.entries) {
      if (cancelled(cancel))
        return {};
      if (search_key(e.name).find(key) == std::string::npos)
        continue;
      if (!seen.insert(identity(source, k, e.id)).second)
        continue;
      if (result.hits.size() >= 2000) {
        result.limited = true;
        break;
      }
      result.hits.push_back({source, a->name, k, std::move(e)});
    }
    if (result.limited) break;
  }
  std::stable_sort(result.hits.begin(), result.hits.end(),
                   [](const auto &a, const auto &b) {
                     return search_key(a.entry.name) < search_key(b.entry.name);
                   });
  return result;
}
void remove_cache(const std::string &dir, const std::string &source) {
  std::lock_guard<std::mutex> lock(cache_mutex);
  auto prefix = dir + "/" + hash(source) + "-";
  for (const auto &f : cache_files(dir))
    if (f.path.rfind(prefix, 0) == 0)
      unlink(f.path.c_str());
}
} // namespace iptv
