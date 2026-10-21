#include "live_tv.h"
#include "http.h"
#include "json.hpp"
#include <algorithm>
#include <cctype>
#include <curl/curl.h>
#include <map>
#include <set>
#include <sstream>
namespace live_tv {
namespace {
std::string trim(std::string s) {
  const auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos)
    return {};
  return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
std::string lower(std::string s) {
  for (auto &c : s)
    c = char(std::tolower((unsigned char)c));
  return s;
}
std::string clean(std::string s, size_t limit = 256) {
  s.erase(std::remove_if(s.begin(), s.end(),
                         [](unsigned char c) { return c < 32 || c == 127; }),
          s.end());
  if (s.size() > limit)
    s.resize(limit);
  return trim(s);
}
std::string attr(const std::string &text, const std::string &key) {
  size_t p = 0;
  while ((p = text.find(key, p)) != std::string::npos) {
    if (p && !std::isspace((unsigned char)text[p - 1])) {
      p += key.size();
      continue;
    }
    auto at = p + key.size();
    if (at >= text.size() || text[at] != '=') {
      p = at;
      continue;
    }
    ++at;
    if (at >= text.size())
      return {};
    const char q = text[at];
    if (q == '"' || q == '\'') {
      const auto end = text.find(q, at + 1);
      return end == std::string::npos ? std::string()
                                      : text.substr(at + 1, end - at - 1);
    }
    const auto end = text.find_first_of(" \t,", at);
    return text.substr(at, end - at);
  }
  return {};
}
std::string resolve(const std::string &base, const std::string &value) {
  CURLU *u = curl_url();
  if (!u)
    return {};
  std::string out;
  char *url = nullptr;
  if (curl_url_set(u, CURLUPART_URL, base.c_str(), 0) == CURLUE_OK &&
      curl_url_set(u, CURLUPART_URL, value.c_str(), 0) == CURLUE_OK &&
      curl_url_get(u, CURLUPART_URL, &url, 0) == CURLUE_OK) {
    out = url;
    curl_free(url);
  }
  curl_url_cleanup(u);
  return http_url(out) ? out : std::string();
}
struct Download {
  std::string body;
  const std::atomic<bool> *cancel;
  bool large = false;
};
size_t write(char *p, size_t a, size_t b, void *v) {
  auto &d = *static_cast<Download *>(v);
  if (a && b > max_bytes / a) {
    d.large = true;
    return 0;
  }
  const size_t n = a * b;
  if (n > max_bytes - d.body.size()) {
    d.large = true;
    return 0;
  }
  if (d.cancel && *d.cancel)
    return 0;
  d.body.append(p, n);
  return n;
}
int progress(void *v, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  auto c = static_cast<Download *>(v)->cancel;
  return c && *c;
}
} // namespace
bool http_url(const std::string &url) {
  if (url.empty() || url.size() > 8192 ||
      std::any_of(url.begin(), url.end(),
                  [](unsigned char c) { return c <= 32 || c == 127; }))
    return false;
  const auto s = lower(url.substr(0, 8));
  if (s.rfind("http://", 0) != 0 && s != "https://")
    return false;
  CURLU *u = curl_url();
  if (!u)
    return false;
  char *host = nullptr;
  const bool ok = curl_url_set(u, CURLUPART_URL, url.c_str(), 0) == CURLUE_OK &&
                  curl_url_get(u, CURLUPART_HOST, &host, 0) == CURLUE_OK &&
                  host && *host;
  if (host)
    curl_free(host);
  curl_url_cleanup(u);
  return ok;
}
Result parse(const std::string &body, const std::string &base) {
  Result r;
  if (body.size() > max_bytes) {
    r.error = "The playlist exceeds the 8 MB limit.";
    return r;
  }
  std::istringstream in(body);
  std::string line;
  Channel c;
  bool entry = false, header = false;
  std::set<std::string> seen;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.compare(0, 3, "\xef\xbb\xbf") == 0)
      line = line.substr(3);
    if (line.empty())
      continue;
    if (line.rfind("#EXTM3U", 0) == 0) {
      header = true;
      continue;
    }
    // Xtream/XUI channel lists may include session metadata. That header
    // alone does not make the list an HLS media/master manifest.
    if (line.rfind("#EXT-X-", 0) == 0 &&
        line.rfind("#EXT-X-SESSION-DATA:", 0) != 0) {
      r.channels.clear();
      r.error = "This is an HLS stream. Add a channel playlist (M3U), not an "
                "individual .m3u8 stream.";
      return r;
    }
    if (line.rfind("#EXTINF:", 0) == 0) {
      c = Channel{};
      entry = true;
      bool quoted = false;
      char quote = 0;
      size_t comma = std::string::npos;
      for (size_t i = 8; i < line.size(); ++i) {
        if ((line[i] == '"' || line[i] == '\'') &&
            (!quoted || line[i] == quote)) {
          quoted = !quoted;
          quote = line[i];
        } else if (line[i] == ',' && !quoted) {
          comma = i;
          break;
        }
      }
      c.name = clean(comma == std::string::npos ? attr(line, "tvg-name")
                                                : line.substr(comma + 1));
      if (c.name.empty())
        c.name = clean(attr(line, "tvg-name"));
      c.id = clean(attr(line, "tvg-id"));
      c.group = clean(attr(line, "group-title"));
      const auto logo = attr(line, "tvg-logo");
      if (!logo.empty())
        c.logo = resolve(base, logo);
      continue;
    }
    if (entry && line.rfind("#EXTGRP:", 0) == 0) {
      c.group = clean(line.substr(8));
      continue;
    }
    if (entry && line.rfind("#EXTVLCOPT:", 0) == 0) {
      const auto value = line.substr(11);
      const auto eq = value.find('=');
      if (eq != std::string::npos) {
        const auto key = lower(value.substr(0, eq));
        const auto val = value.substr(eq + 1);
        if (c.headers.size() < 16 && !val.empty() && val.size() <= 2048 &&
            val.find_first_of("\r\n") == std::string::npos) {
          if (key == "http-user-agent")
            c.headers.push_back("User-Agent: " + val);
          else if (key == "http-referrer" || key == "http-referer")
            c.headers.push_back("Referer: " + val);
        }
      }
      continue;
    }
    if (line[0] == '#')
      continue;
    if (!entry)
      continue;
    c.url = resolve(base, line);
    entry = false;
    if (c.url.empty() || c.name.empty() || !seen.insert(c.url).second)
      continue;
    if (c.id.empty())
      c.id = c.url;
    if (c.group.empty())
      c.group = "Other";
    if (r.channels.size() == max_channels) {
      r.channels.clear();
      r.error = "The playlist exceeds the 10,000 channel limit.";
      return r;
    }
    r.channels.push_back(std::move(c));
  }
  if (!header || r.channels.empty()) {
    r.channels.clear();
    r.error = "No supported HTTP/HLS channels were found in this M3U playlist.";
  }
  return r;
}
HttpResponse download(const std::string &url, const std::atomic<bool> *cancel,
                      bool redirects) {
  HttpResponse r;
  if (!http_url(url)) {
    r.error = "Enter an HTTP or HTTPS M3U playlist URL.";
    return r;
  }
  CURL *h = curl_easy_init();
  if (!h) {
    r.error = "Could not start the playlist request.";
    return r;
  }
  Download d{{}, cancel};
  http_setup_handle(h);
  curl_easy_setopt(h, CURLOPT_URL, url.c_str());
  curl_easy_setopt(h, CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, redirects ? 1L : 0L);
  curl_easy_setopt(h, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(h, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS_STR,
                   url.rfind("https://", 0) == 0 ? "https" : "http,https");
  curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(h, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
  if (!http_ca_bundle().empty())
    curl_easy_setopt(h, CURLOPT_CAINFO, http_ca_bundle().c_str());
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, write);
  curl_easy_setopt(h, CURLOPT_WRITEDATA, &d);
  curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, progress);
  curl_easy_setopt(h, CURLOPT_XFERINFODATA, &d);
  auto rc = curl_easy_perform(h);
  long status = 0;
  char *effective = nullptr;
  curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_getinfo(h, CURLINFO_EFFECTIVE_URL, &effective);
  std::string base = effective ? effective : url;
  curl_easy_cleanup(h);
  if (cancel && *cancel) {
    r.error = "Playlist loading cancelled.";
    return r;
  }
  if (d.large) {
    r.error = "The playlist exceeds the 8 MB limit.";
    return r;
  }
  if (status < 200 || status >= 300) {
    r.error = "Playlist request failed (HTTP " + std::to_string(status) +
              "). Check your source subscription or URL.";
    return r;
  }
  if (rc != CURLE_OK) {
    r.error =
        "Could not download the playlist. Check your connection and retry.";
    return r;
  }
  r.status = status;
  r.body = std::move(d.body);
  r.content_type = base;
  return r;
}

Result fetch(const std::string &url, const std::atomic<bool> *cancel) {
  auto r = download(url, cancel, true);
  if (!r.error.empty())
    return {{}, r.error};
  return parse(r.body, r.content_type);
}
namespace {
std::string encoded(const std::string &s) {
  char *e = curl_easy_escape(nullptr, s.c_str(), int(s.size()));
  if (!e)
    return {};
  std::string out = e;
  curl_free(e);
  return out;
}
std::string value(const nlohmann::json &j, const char *key) {
  if (!j.is_object() || !j.contains(key))
    return {};
  const auto &v = j[key];
  return v.is_string()           ? v.get<std::string>()
         : v.is_number_integer() ? v.dump()
                                 : std::string();
}
std::string root(std::string s) {
  while (!s.empty() && s.back() == '/')
    s.pop_back();
  return s;
}
bool credentials(const std::string &server, const std::string &user,
                 const std::string &password) {
  return http_url(server) && server.find_first_of("?#") == std::string::npos &&
         !user.empty() && !password.empty() && user.size() <= 256 &&
         password.size() <= 256;
}
} // namespace
Result parse_xtream(const std::string &body, const std::string &groups,
                    const std::string &server, const std::string &user,
                    const std::string &password, const std::string &output) {
  Result r;
  if (!credentials(server, user, password) || body.size() > max_bytes ||
      groups.size() > max_bytes) {
    r.error = "Invalid Xtream configuration or oversized response.";
    return r;
  }
  auto data = nlohmann::json::parse(body, nullptr, false),
       cats = nlohmann::json::parse(groups, nullptr, false);
  if (!data.is_array() || !cats.is_array()) {
    r.error = "The Xtream server did not return live channels. Check your "
              "login and subscription.";
    return r;
  }
  if (data.size() > max_channels) {
    r.error = "The service exceeds the 10,000 channel limit.";
    return r;
  }
  std::map<std::string, std::string> names;
  for (const auto &g : cats) {
    auto id = value(g, "category_id"), name = clean(value(g, "category_name"));
    if (!id.empty() && !name.empty())
      names[id] = name;
  }
  std::set<std::string> seen;
  for (const auto &item : data) {
    Channel c;
    auto id = value(item, "stream_id");
    if (id.empty() || id.size() > 20 ||
        id.find_first_not_of("0123456789") != std::string::npos ||
        !seen.insert(id).second)
      continue;
    c.id = id;
    c.name = clean(value(item, "name"));
    if (c.name.empty())
      continue;
    auto category = names.find(value(item, "category_id"));
    c.group = category == names.end() ? "Other" : category->second;
    auto icon = value(item, "stream_icon");
    if (!icon.empty())
      c.logo = resolve(server, icon);
    // Use the authenticated live endpoint, never untrusted direct_source URLs.
    auto extension = lower(value(item, "container_extension"));
    if (extension != "ts" && extension != "m3u8")
      extension = output == "m3u8" ? "m3u8" : "ts";
    c.url = root(server) + "/live/" + encoded(user) + "/" + encoded(password) +
            "/" + id + "." + extension;
    r.channels.push_back(std::move(c));
  }
  if (r.channels.empty())
    r.error = "This Xtream account returned no live channels.";
  return r;
}
Result fetch_xtream(const std::string &server, const std::string &user,
                    const std::string &password,
                    const std::atomic<bool> *cancel) {
  if (!credentials(server, user, password))
    return {{}, "Enter a valid server address, username and password."};
  const auto api = root(server) + "/player_api.php?username=" + encoded(user) +
                   "&password=" + encoded(password);
  auto auth = download(api, cancel, false);
  if (!auth.error.empty())
    return {{}, auth.error};
  auto j = nlohmann::json::parse(auth.body, nullptr, false);
  if (!j.is_object() || !j.contains("user_info") ||
      value(j["user_info"], "auth") != "1")
    return {{},
            "Xtream login was rejected. Check the username, password and "
            "subscription."};
  const auto status = value(j["user_info"], "status");
  if (!status.empty() && lower(status) != "active")
    return {{}, "This Xtream subscription is not active."};
  auto cats = download(api + "&action=get_live_categories", cancel, false);
  if (!cats.error.empty())
    return {{}, cats.error};
  auto channels = download(api + "&action=get_live_streams", cancel, false);
  if (!channels.error.empty())
    return {{}, channels.error};
  std::string output = "ts";
  const auto &info = j["user_info"];
  if (info.is_object() && info.contains("allowed_output_formats") &&
      info["allowed_output_formats"].is_array())
    for (const auto &v : info["allowed_output_formats"])
      if (v.is_string() && v.get<std::string>() == "m3u8")
        output = "m3u8";
  return parse_xtream(channels.body, cats.body, server, user, password, output);
}
} // namespace live_tv
