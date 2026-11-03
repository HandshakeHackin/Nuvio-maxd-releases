#include "media_server.h"
#include "http_credentials.hpp"
#include "plex.h"
#include <algorithm>
#include <cmath>
#include <curl/curl.h>
namespace media_server {
namespace {
constexpr size_t cap = 16u << 20;
std::string enc(const std::string &s) {
  auto c = curl_easy_init();
  if (!c)
    return {};
  auto p = curl_easy_escape(c, s.data(), int(s.size()));
  std::string r = p ? p : "";
  curl_free(p);
  curl_easy_cleanup(c);
  return r;
}
struct Transfer {
  std::string body;
  const std::atomic<bool> *cancel;
  bool overflow = false;
};
size_t write(char *p, size_t a, size_t b, void *v) {
  auto &t = *static_cast<Transfer *>(v);
  size_t n = a * b;
  if (n > cap - t.body.size()) {
    t.overflow = true;
    return 0;
  }
  t.body.append(p, n);
  return n;
}
int progress(void *v, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  auto p = static_cast<Transfer *>(v);
  return p->cancel && p->cancel->load();
}
HttpResponse transfer(const Connection &c, const std::string &path,
                      const json *body, const std::atomic<bool> *cancel) {
  HttpResponse r;
  if (cancel && cancel->load()) {
    r.error = "Cancelled";
    return r;
  }
  auto h = curl_easy_init();
  if (!h) {
    r.error = "Could not start media server request";
    return r;
  }
  Transfer t{{}, cancel};
  curl_slist *headers = nullptr;
  for (const auto &s : c.headers())
    headers = curl_slist_append(headers, s.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");
  std::string url = c.base + path, payload = body ? body->dump() : "";
  http_setup_handle(h);
  curl_easy_setopt(h, CURLOPT_URL, url.c_str());
  curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(h, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, c.kind == "plex" ? 4L : 8L);
  curl_easy_setopt(h, CURLOPT_TIMEOUT, c.kind == "plex" ? 10L : 20L);
  curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(h, CURLOPT_USERAGENT, kUserAgent);
  if (!http_ca_bundle().empty())
    curl_easy_setopt(h, CURLOPT_CAINFO, http_ca_bundle().c_str());
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, write);
  curl_easy_setopt(h, CURLOPT_WRITEDATA, &t);
  curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, progress);
  curl_easy_setopt(h, CURLOPT_XFERINFODATA, &t);
  if (body) {
    curl_easy_setopt(h, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, long(payload.size()));
  }
  auto rc = curl_easy_perform(h);
  curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &r.status);
  curl_slist_free_all(headers);
  curl_easy_cleanup(h);
  r.body = std::move(t.body);
  if (rc != CURLE_OK)
    r.error = t.overflow ? "Media server response too large"
                         : (cancel && cancel->load()
                                ? "Cancelled"
                                : "Media server connection failed");
  return r;
}
std::string userpath(const Connection &c, const std::string &tail) {
  return c.kind == "emby" ? "/Users/" + enc(c.user) + tail : tail;
}
} // namespace
bool safe_id(const std::string &s) {
  return !s.empty() && s.size() <= 128 &&
         std::all_of(s.begin(), s.end(), [](unsigned char c) {
           return std::isalnum(c) || c == '-' || c == '_';
         });
}
std::string origin(const std::string &s) { return http_credentials::origin(s); }
std::string normalize(std::string b, const std::string &kind) {
  b = trim(b);
  if (b.find("://") == std::string::npos)
    b = "http://" + b;
  if (b.size() > 2048 || origin(b).empty() ||
      b.find_first_of("?#\\\r\n\t ") != std::string::npos)
    return {};
  for (unsigned char c : b)
    if (c < 32 || c == 127)
      return {};
  while (b.ends_with('/'))
    b.pop_back();
  for (const auto &suffix :
       {std::string("/web/index.html"), std::string("/web")})
    if (b.ends_with(suffix))
      b.resize(b.size() - suffix.size());
  while (b.ends_with('/'))
    b.pop_back();
  if (kind == "emby" && !b.ends_with("/emby"))
    b += "/emby";
  return b;
}
Connection Connection::snapshot() const {
  Connection c = *this;
  if (kind == "plex" && credentials) {
    std::unique_lock<std::mutex> l(credentials->mutex, std::try_to_lock);
    if (l.owns_lock() && credentials->initialized) {
      c.token = credentials->token;
      c.auth = credentials->auth;
    }
  }
  return c;
}
HttpResponse transport(const Connection &c, const std::string &path,
                       const json *body, const std::atomic<bool> *cancel) {
  return transfer(c, path, body, cancel);
}
bool Connection::valid() const {
  return (kind == "jellyfin" || kind == "emby" || kind == "plex") &&
         safe_id(id) && safe_id(user) && safe_id(device) && !token.empty() &&
         token.size() < (kind == "plex" ? 16384u : 4096u) &&
         std::all_of(
             token.begin(), token.end(),
             [](unsigned char v) {
               return v >= 33 && v <= 126 && v != '"' && v != '\\';
             }) &&
         !base.empty() && normalize(base, kind) == base;
}
json Connection::saved() const {
  const auto c = snapshot();
  return {{"id", id},
          {"kind", kind},
          {"base", base},
          {"user", user},
          {"user_name", user_name},
          {"server_name", server_name},
          {"token", c.token},
          {"device", device},
          {"plexAuth", c.auth}};
}
Connection Connection::restore(const json &j) {
  Connection c;
  c.id = jstr(j, "id");
  c.kind = jstr(j, "kind");
  c.base = jstr(j, "base");
  c.user = jstr(j, "user");
  c.user_name = jstr(j, "user_name");
  c.server_name = jstr(j, "server_name");
  c.token = jstr(j, "token");
  c.device = jstr(j, "device");
  if (jobj(j, "plexAuth").is_object())
    c.auth = j["plexAuth"];
  return c;
}
std::vector<std::string> Connection::headers(bool scoped) const {
  if (kind == "plex") {
    const auto c = snapshot();
    std::vector<std::string> h = {
        "Accept: application/json",     "X-Plex-Client-Identifier: " + device,
        "X-Plex-Product: Maxd PS5",     "X-Plex-Version: 1.0",
        "X-Plex-Platform: PS5",         "X-Plex-Device: PS5",
        "X-Plex-Device-Name: Maxd PS5", "X-Plex-Client-Profile-Name: generic"};
    if (!c.token.empty())
      h.push_back("X-Plex-Token: " + c.token);
    if (scoped)
      h.push_back("X-Maxd-Credential-Origin: " + origin(base));
    return h;
  }
  std::string v = "MediaBrowser Client=\"Maxd\", Device=\"PS5\", DeviceId=\"" +
                  device + "\", Version=\"1.0\"";
  if (!token.empty())
    v += ", Token=\"" + token + "\"";
  std::vector<std::string> h{
      (kind == "emby" ? "X-Emby-Authorization: " : "Authorization: ") + v};
  if (scoped)
    h.push_back("X-Maxd-Credential-Origin: " + origin(base));
  return h;
}
Result request(const Connection &c, const std::string &path, const json *body,
               const std::atomic<bool> *cancel) {
  Result r;
  if (c.base.empty() || origin(c.base).empty() || !path.starts_with('/') ||
      path.starts_with("//")) {
    r.error = "Invalid media server address";
    return r;
  }
  auto h = transfer(c, path, body, cancel);
  if (!h.ok()) {
    r.error = h.status == 401 || h.status == 403
                  ? "Media server sign-in expired or access denied. Reconnect "
                    "in Settings."
              : h.status >= 300 && h.status < 400
                  ? "Media server redirected the request. Enter its final "
                    "server address in Settings."
              : h.error.empty()
                  ? "Media server returned HTTP " + std::to_string(h.status)
                  : h.error;
    return r;
  }
  if (h.body.empty()) {
    r.data = json::object();
    return r;
  }
  r.data = json::parse(h.body, nullptr, false);
  if (r.data.is_discarded()) {
    r.data = {};
    r.error = "Media server returned invalid JSON";
  }
  return r;
}
Result authenticate(Connection &c, const std::string &username,
                    const std::string &password,
                    const std::atomic<bool> *cancel) {
  Result r;
  c.base = normalize(c.base, c.kind);
  if (c.base.empty() || !safe_id(c.device) || !safe_id(c.id) ||
      username.empty() || username.size() > 256 || password.size() > 4096) {
    r.error = "Check server address and username";
    return r;
  }
  c.token.clear();
  auto info = request(c, "/System/Info/Public", nullptr, cancel);
  if (!info.ok())
    return info;
  auto reported = lower(jstr(info.data, "ProductName"));
  if (!reported.empty() && reported.find(c.kind) == std::string::npos) {
    r.error = "The address belongs to a different server type";
    return r;
  }
  json body = {{"Username", username}, {"Pw", password}};
  r = request(c, "/Users/AuthenticateByName", &body, cancel);
  if (!r.ok())
    return r;
  c.user = jstr(jobj(r.data, "User"), "Id");
  c.user_name = jstr(jobj(r.data, "User"), "Name");
  c.token = jstr(r.data, "AccessToken");
  c.server_name = jstr(info.data, "ServerName");
  if (!c.valid()) {
    c.token.clear();
    r.error = "Media server returned an invalid sign-in";
  }
  return r;
}
Result browse(const Connection &c, const std::string &parent,
              const std::string &type, const std::string &query, size_t offset,
              const std::atomic<bool> *cancel) {
  if (c.kind == "plex")
    return plex::Client().browse(c, parent, type, query, offset, cancel);
  if (!c.valid() || (!parent.empty() && !safe_id(parent)) ||
      query.size() > 256 || offset > 1000000)
    return {{}, "Invalid media server library request"};
  std::string path;
  if (type == "views")
    path = c.kind == "emby" ? userpath(c, "/Views") : "/UserViews";
  else if (type == "episodes")
    path = "/Shows/" + enc(parent) + "/Episodes";
  else if (type == "resume")
    path =
        c.kind == "emby" ? userpath(c, "/Items/Resume") : "/UserItems/Resume";
  else
    path = c.kind == "emby" ? userpath(c, "/Items") : "/Items";
  path += "?UserId=" + enc(c.user) +
          "&Limit=100&StartIndex=" + std::to_string(offset) +
          "&Fields=Overview,MediaSources,UserData,ProviderIds&SortBy=" +
          (type == "episodes" ? "ParentIndexNumber,IndexNumber" : "SortName") +
          "&SortOrder=Ascending";
  if (type == "items") {
    path += "&Recursive=true&IncludeItemTypes=Movie,Series";
    if (!parent.empty())
      path += "&ParentId=" + enc(parent);
  }
  if (!query.empty())
    path += "&SearchTerm=" + enc(query);
  auto r = request(c, path, nullptr, cancel);
  if (r.ok() && (!r.data.is_object() || !r.data.contains("Items") ||
                 !r.data["Items"].is_array() || r.data["Items"].size() > 100)) {
    r.error = "Invalid media server library response";
  }
  return r;
}
Playback playback(const Connection &c, const std::string &item,
                  const std::string &source, bool compatible,
                  const std::atomic<bool> *cancel) {
  if (c.kind == "plex")
    return plex::Client().playback(c, item, source, compatible, cancel);
  Playback p;
  if (!c.valid() || !safe_id(item) || (!source.empty() && !safe_id(source))) {
    p.error = "Invalid media server item";
    return p;
  }
  json body = {
      {"UserId", c.user},
      {"EnableDirectPlay", !compatible},
      {"EnableDirectStream", !compatible},
      {"EnableTranscoding", true},
      {"AutoOpenLiveStream", true},
      {"MaxStreamingBitrate", 120000000},
      {"DeviceProfile",
       {{"Name", "Maxd PS5"},
        {"MaxStreamingBitrate", 120000000},
        {"DirectPlayProfiles",
         json::array({{{"Container", "mkv,mp4,mov,avi,ts,mpegts,webm"},
                       {"Type", "Video"},
                       {"VideoCodec", "h264,hevc,vp9,mpeg2video"},
                       {"AudioCodec",
                        "aac,ac3,eac3,dts,flac,mp3,pcm_s16le,pcm_s24le"}}})},
        {"TranscodingProfiles",
         json::array({{{"Container", "ts"},
                       {"Type", "Video"},
                       {"VideoCodec", "h264"},
                       {"AudioCodec", "aac"},
                       {"Protocol", "hls"},
                       {"Context", "Streaming"},
                       {"MaxAudioChannels", "2"},
                       {"MinSegments", "2"},
                       {"BreakOnNonKeyFrames", true}}})}}}};
  if (!source.empty())
    body["MediaSourceId"] = source;
  auto r =
      request(c, "/Items/" + enc(item) + "/PlaybackInfo?UserId=" + enc(c.user),
              &body, cancel);
  if (!r.ok()) {
    p.error = r.error;
    return p;
  }
  p.session = jstr(r.data, "PlaySessionId");
  auto list = jobj(r.data, "MediaSources");
  if (!list.is_array()) {
    p.error = "No playable media server version";
    return p;
  }
  for (const auto &s : list) {
    auto id = jstr(s, "Id");
    if (!safe_id(id) || (!source.empty() && id != source))
      continue;
    p.source = id;
    if (!compatible && jbool(s, "SupportsDirectPlay")) {
      p.url = c.base + "/Videos/" + enc(item) +
              "/stream?Static=true&MediaSourceId=" + enc(id) +
              "&PlaySessionId=" + enc(p.session) + "&DeviceId=" + enc(c.device);
      p.method = "DirectPlay";
    } else {
      auto u = jstr(s, "TranscodingUrl");
      if (u.empty())
        continue;
      if (u.starts_with('/') &&
          !u.starts_with("//")) { // Root-relative URLs commonly include the
                                  // reverse-proxy base already.
        auto root = origin(c.base);
        const auto slash = c.base.find('/', c.base.find("://") + 3);
        const auto prefix =
            slash == std::string::npos ? std::string() : c.base.substr(slash);
        u = (!prefix.empty() && !(u.starts_with(prefix + "/") || u == prefix))
                ? c.base + u
                : root + u;
      } else if (u.find("://") == std::string::npos)
        u = c.base + "/" + u;
      if (origin(u) != origin(c.base) ||
          u.find_first_of("\r\n\\") != std::string::npos)
        continue;
      // Never retain a server-supplied token in URLs, diagnostic logs or
      // caches.
      auto q = u.find('?');
      if (q != std::string::npos) {
        std::string clean = u.substr(0, q), query = u.substr(q + 1);
        bool first = true;
        size_t at = 0;
        while (at <= query.size()) {
          auto end = query.find('&', at);
          auto part =
              query.substr(at, end == std::string::npos ? end : end - at);
          auto key = part.substr(0, part.find('='));
          int length = 0;
          auto decoded = curl_easy_unescape(nullptr, key.c_str(),
                                            int(key.size()), &length);
          if (decoded) {
            key.assign(decoded, size_t(length));
            curl_free(decoded);
          }
          key = lower(key);
          if (key != "api_key" && key != "apikey" && key != "access_token" &&
              key != "accesstoken" && key != "token" && key != "x-emby-token" &&
              key != "authorization") {
            clean += first ? '?' : '&';
            clean += part;
            first = false;
          }
          if (end == std::string::npos)
            break;
          at = end + 1;
        }
        u = clean;
      }
      p.url = u;
      p.method =
          jbool(s, "SupportsDirectStream") ? "DirectStream" : "Transcode";
    }
    p.headers = c.headers(true);
    return p;
  }
  p.error = "Server cannot provide this version. Try compatible HLS playback; "
            "server transcoding must be enabled.";
  return p;
}
Result report(const Connection &c, const Playback &p, const std::string &item,
              double pos, bool paused, const std::string &event,
              const std::atomic<bool> *cancel) {
  if (c.kind == "plex")
    return plex::Client().report(c, p, item, pos, paused, event, cancel);
  if (!safe_id(item) || !std::isfinite(pos) || pos < 0 || pos > 1e8)
    return {{}, "Invalid playback progress"};
  json body = {{"ItemId", item},
               {"MediaSourceId", p.source},
               {"PlaySessionId", p.session},
               {"PositionTicks", int64_t(pos * 10000000)},
               {"IsPaused", paused},
               {"CanSeek", true},
               {"PlayMethod", p.method},
               {"EventName", paused ? "Pause" : "TimeUpdate"}};
  return request(c,
                 "/Sessions/Playing" + (event == "start"  ? std::string()
                                        : event == "stop" ? "/Stopped"
                                                          : "/Progress"),
                 &body, cancel);
}
HttpResponse image(const Connection &c, const std::string &item,
                   const std::atomic<bool> *cancel) {
  if (c.kind == "plex")
    return plex::Client().image(c, item, cancel);
  if (!c.valid() || !safe_id(item)) {
    HttpResponse r;
    r.error = "Invalid image";
    return r;
  }
  return transfer(
      c, "/Items/" + enc(item) + "/Images/Primary?maxHeight=600&quality=85",
      nullptr, cancel);
}
Reporter::Reporter()
    : thread_([this] {
        for (;;) {
          Job j;
          {
            std::unique_lock<std::mutex> l(mutex_);
            cv_.wait(l, [&] { return stop_ || !jobs_.empty(); });
            if (stop_)
              return;
            j = std::move(jobs_.front());
            jobs_.pop_front();
          }
          auto r = report(j.c, j.p, j.item, j.pos, j.paused, j.event, &stop_);
          if (!r.ok() && !stop_)
            dlog("media server: watch progress report failed; check server "
                 "connectivity or reconnect");
        }
      }) {}
Reporter::~Reporter() {
  stop_ = true;
  cv_.notify_all();
  if (thread_.joinable())
    thread_.join();
}
void Reporter::push(const Connection &c, const Playback &p,
                    const std::string &item, double pos, bool paused,
                    const std::string &event) {
  std::lock_guard<std::mutex> l(mutex_);
  if (stop_)
    return;
  Job j{c, p, item, event, pos, paused};
  if (event == "progress" && !jobs_.empty() && jobs_.back().event == event &&
      jobs_.back().p.session == p.session && jobs_.back().c.id == c.id &&
      jobs_.back().item == item) {
    jobs_.back() = std::move(j);
    return;
  }
  if (jobs_.size() >= 16) {
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                           [](auto &x) { return x.event == "progress"; });
    if (it != jobs_.end())
      jobs_.erase(it);
    else
      return;
  }
  jobs_.push_back(std::move(j));
  cv_.notify_one();
}

} // namespace media_server
