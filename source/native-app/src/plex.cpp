// Plex's documented JSON API and device-JWT flow. No account password is used.
#include "plex.h"
#include <algorithm>
#include <cmath>
#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <set>
namespace plex {
using namespace media_server;
namespace {
bool credential(const std::string &s) {
  return !s.empty() && s.size() < 16384 &&
         std::all_of(
             s.begin(), s.end(),
             [](unsigned char c) {
               return c >= 33 && c <= 126 && c != '"' && c != '\\';
             });
}
bool number(const std::string &s) {
  return !s.empty() && s.size() <= 20 &&
         std::all_of(s.begin(), s.end(),
                     [](unsigned char c) { return c >= '0' && c <= '9'; });
}
std::string id(const json &j, const char *key) {
  const auto &v = jobj(j, key);
  return v.is_string()            ? v.get<std::string>()
         : v.is_number_unsigned() ? std::to_string(v.get<uint64_t>())
         : v.is_number_integer()  ? std::to_string(v.get<int64_t>())
                                  : "";
}
std::string random_hex(size_t n) {
  std::vector<unsigned char> bytes(n);
  if (RAND_bytes(bytes.data(), int(n)) != 1)
    return {};
  const char *h = "0123456789abcdef";
  std::string s;
  for (auto c : bytes) {
    s += h[c >> 4];
    s += h[c & 15];
  }
  return s;
}
std::string unbase64(std::string s) {
  if (s.empty() || s.size() > 32768 ||
      !std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_';
      }))
    return {};
  for (auto &c : s) {
    if (c == '-')
      c = '+';
    else if (c == '_')
      c = '/';
  }
  size_t padding = (4 - s.size() % 4) % 4;
  s.append(padding, '=');
  std::string out(s.size(), 0);
  int n = EVP_DecodeBlock(reinterpret_cast<unsigned char *>(out.data()),
                          reinterpret_cast<const unsigned char *>(s.data()),
                          int(s.size()));
  if (n < 0 || size_t(n) < padding)
    return {};
  out.resize(size_t(n) - padding);
  return out;
}
double expiry(const std::string &token) {
  auto a = token.find('.'),
       b = token.find('.', a == std::string::npos ? a : a + 1);
  if (a == std::string::npos || b == std::string::npos)
    return 0;
  auto j =
      json::parse(unbase64(token.substr(a + 1, b - a - 1)), nullptr, false);
  return j.is_discarded() ? 0 : jnum(j, "exp");
}
Result decode(const HttpResponse &h) {
  Result r;
  if (!h.ok()) {
    r.error =
        h.status == 401 || h.status == 403 || h.status == 498
            ? "Plex access expired or was denied. Reconnect in Media servers."
        : h.status == 429
            ? "Plex is rate limiting requests. Wait and reconnect."
            : "Plex request failed" +
                  (h.status ? " (HTTP " + std::to_string(h.status) + ")"
                            : ". Check connectivity and the console clock.");
    return r;
  }
  if (h.body.empty()) {
    r.data = json::object();
    return r;
  }
  r.data = json::parse(h.body, nullptr, false);
  if (r.data.is_discarded()) {
    r.data = {};
    r.error = "Plex returned invalid JSON";
  }
  return r;
}
json container(const json &j) {
  const auto &m = jobj(j, "MediaContainer");
  return m.is_object() ? m : json::object();
}
json list(const json &j, const char *key) {
  const auto &a = jobj(j, key);
  return a.is_array() ? a : json::array();
}
json map_item(const json &j, bool library = false) {
  std::string key = library ? id(j, "key") : id(j, "ratingKey"),
              type = jstr(j, "type");
  if (!number(key))
    return {};
  if (!library && type != "movie" && type != "show" && type != "season" &&
      type != "episode")
    return {};
  if (library && type != "movie" && type != "show")
    return {};
  json r = {{"Id", library ? "lib-" + key : key},
            {"Name", jstr(j, "title")},
            {"Type", library             ? "CollectionFolder"
                     : type == "show"    ? "Series"
                     : type == "season"  ? "Season"
                     : type == "episode" ? "Episode"
                                         : "Movie"},
            {"Overview", jstr(j, "summary")},
            {"ProductionYear", jnum(j, "year")},
            {"RunTimeTicks", jnum(j, "duration") * 10000},
            {"ParentIndexNumber", jnum(j, "parentIndex")},
            {"IndexNumber", jnum(j, "index")},
            {"UserData",
             {{"PlaybackPositionTicks", jnum(j, "viewOffset") * 10000},
              {"Played", jnum(j, "viewCount") > 0}}},
            {"MediaSources", json::array()}};
  for (const auto &m : list(j, "Media")) {
    auto mid = id(m, "id");
    if (number(mid))
      r["MediaSources"].push_back(
          {{"Id", mid},
           {"Name", jstr(m, "videoResolution") + " · " + jstr(m, "videoCodec") +
                        " · " + jstr(m, "container") + " · " +
                        std::to_string(int(jnum(m, "audioChannels"))) +
                        " channels"}});
  }
  return r;
}
std::string safe_path(std::string p, const Connection &c) {
  if (p.empty() || p.size() > 8192 ||
      p.find_first_of("\r\n\\#") != std::string::npos)
    return {};
  if (p.starts_with("//"))
    return {};
  if (p.starts_with("http://") || p.starts_with("https://")) {
    if (origin(p) != origin(c.base))
      return {};
    auto at = p.find('/', p.find("://") + 3);
    if (at == std::string::npos)
      return {};
    p = p.substr(at);
    auto slash = c.base.find('/', c.base.find("://") + 3);
    if (slash != std::string::npos) {
      auto prefix = c.base.substr(slash);
      if (p.starts_with(prefix + "/"))
        p = p.substr(prefix.size());
    }
  }
  if (!p.starts_with('/'))
    return {};
  auto q = p.find('?');
  if (q != std::string::npos)
    p.resize(q);
  return p;
}
} // namespace
std::string encode(const std::string &s) {
  auto c = curl_easy_init();
  if (!c)
    return {};
  auto p = curl_easy_escape(c, s.data(), int(s.size()));
  std::string r = p ? p : "";
  curl_free(p);
  curl_easy_cleanup(c);
  return r;
}
std::string base64url(const unsigned char *p, size_t n) {
  std::string s(4 * ((n + 2) / 3), 0);
  EVP_EncodeBlock(reinterpret_cast<unsigned char *>(s.data()), p, int(n));
  while (s.ends_with('='))
    s.pop_back();
  for (auto &c : s) {
    if (c == '+')
      c = '-';
    else if (c == '/')
      c = '_';
  }
  return s;
}
std::string device_jwt(const Pair &p, const std::string &nonce) {
  auto raw = unbase64(p.key);
  if (raw.size() != 32 || !safe_id(p.device) || !safe_id(p.kid))
    return {};
  auto key = EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(raw.data()), raw.size());
  if (!key)
    return {};
  double now = now_epoch_ms() / 1000.;
  json head = {{"kid", p.kid}, {"alg", "EdDSA"}, {"typ", "JWT"}},
       body = {{"aud", "plex.tv"},
               {"iss", p.device},
               {"iat", int64_t(now)},
               {"exp", int64_t(now + 300)}};
  if (!nonce.empty()) {
    body["nonce"] = nonce;
    body["scope"] = "username";
  }
  auto b64 = [](const std::string &s) {
    return base64url(reinterpret_cast<const unsigned char *>(s.data()),
                     s.size());
  };
  std::string payload = b64(head.dump()) + "." + b64(body.dump());
  auto ctx = EVP_MD_CTX_new();
  unsigned char signature[64];
  size_t n = sizeof(signature);
  bool ok =
      ctx && EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1 &&
      EVP_DigestSign(ctx, signature, &n,
                     reinterpret_cast<const unsigned char *>(payload.data()),
                     payload.size()) == 1;
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(key);
  return ok ? payload + "." + base64url(signature, n) : "";
}
HttpResponse Client::call(const Connection &c, const std::string &path,
                          const json *body,
                          const std::atomic<bool> *cancel) const {
  if (cancel && cancel->load()) {
    HttpResponse r;
    r.error = "Cancelled";
    return r;
  }
  return request_ ? request_(c, path, body, cancel)
                  : transport(c, path, body, cancel);
}
Result Client::cloud(const std::string &device, const std::string &token,
                     const std::string &path, const json *body,
                     const std::atomic<bool> *cancel) const {
  Connection c;
  c.kind = "plex";
  c.base = "https://clients.plex.tv";
  c.device = device;
  c.token = token;
  return decode(call(c, "/api/v2" + path, body, cancel));
}
Pair Client::start(const std::atomic<bool> *cancel) const {
  Pair p;
  unsigned char secret[32];
  if (RAND_bytes(secret, sizeof(secret)) != 1) {
    p.error = "Could not create Plex device key";
    return p;
  }
  p.device = "maxd-plex-" + random_hex(16);
  p.kid = random_hex(16);
  p.key = base64url(secret, sizeof(secret));
  auto key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, secret,
                                          sizeof(secret));
  OPENSSL_cleanse(secret, sizeof(secret));
  unsigned char pub[32];
  size_t n = sizeof(pub);
  if (!key || EVP_PKEY_get_raw_public_key(key, pub, &n) != 1) {
    EVP_PKEY_free(key);
    p.error = "Could not create Plex device key";
    return p;
  }
  EVP_PKEY_free(key);
  json body = {{"jwk",
                {{"kty", "OKP"},
                 {"crv", "Ed25519"},
                 {"x", base64url(pub, n)},
                 {"kid", p.kid},
                 {"alg", "EdDSA"},
                 {"use", "sig"}}},
               {"strong", false}};
  auto r = cloud(p.device, "", "/pins", &body, cancel);
  if (!r.ok()) {
    p.error = r.error;
    return p;
  }
  p.id = id(r.data, "id");
  p.code = jstr(r.data, "code");
  p.expires = int(std::clamp(jnum(r.data, "expiresIn", 600), 30.0, 1800.0));
  if (!number(p.id) || p.code.size() < 4 || p.code.size() > 16 ||
      !safe_id(p.code)) {
    p.error = "Plex returned an invalid connection code";
    return p;
  }
  p.url = "https://plex.tv/link/?pin=" + encode(p.code);
  return p;
}
Pair Client::poll(Pair p, const std::atomic<bool> *cancel) const {
  if (!number(p.id)) {
    p.error = "Invalid Plex pairing session";
    return p;
  }
  auto jwt = device_jwt(p);
  if (jwt.empty()) {
    p.error = "Could not sign Plex device request";
    return p;
  }
  auto r = cloud(p.device, "", "/pins/" + p.id + "?deviceJWT=" + encode(jwt),
                 nullptr, cancel);
  if (!r.ok()) {
    p.error = r.error;
    return p;
  }
  p.token = jstr(r.data, "authToken", jstr(r.data, "auth_token"));
  if (!p.token.empty() && !credential(p.token)) {
    p.error = "Plex returned an invalid account token";
    return p;
  }
  p.pending = p.token.empty();
  return p;
}
Servers Client::servers(const Pair &p, const std::atomic<bool> *cancel) const {
  Servers result;
  if (!safe_id(p.device) || !credential(p.token)) {
    result.error = "Plex account is not connected";
    return result;
  }
  auto r = cloud(p.device, p.token,
                 "/resources?includeHttps=1&includeRelay=1&includeIPv6=0",
                 nullptr, cancel);
  if (!r.ok()) {
    result.error = r.error;
    return result;
  }
  auto resources =
      r.data.is_array() ? r.data : list(container(r.data), "Device");
  if (resources.size() > 128) {
    result.error = "Plex returned too many resources";
    return result;
  }
  for (const auto &resource : resources) {
    auto provides = jstr(resource, "provides");
    auto parts = split(provides, ',');
    if (std::find(parts.begin(), parts.end(), "server") == parts.end())
      continue;
    Server s;
    s.id = jstr(resource, "clientIdentifier");
    s.name = jstr(resource, "name");
    s.token = jstr(resource, "accessToken");
    if (!safe_id(s.id) || !credential(s.token))
      continue;
    std::vector<std::pair<int, std::string>> urls;
    for (const auto &q : list(resource, "connections")) {
      auto u = normalize(jstr(q, "uri"), "plex");
      bool local = jbool(q, "local") || jnum(q, "local") == 1,
           relay = jbool(q, "relay") || jnum(q, "relay") == 1;
      if (u.empty() || (!u.starts_with("https://") && !local))
        continue;
      urls.push_back({relay   ? 4
                      : local ? (u.starts_with("https://") ? 0 : 1)
                              : 2,
                      u});
    }
    std::stable_sort(urls.begin(), urls.end());
    for (const auto &u : urls)
      if (s.urls.size() < 4 &&
          std::find(s.urls.begin(), s.urls.end(), u.second) == s.urls.end())
        s.urls.push_back(u.second);
    if (!s.urls.empty() && result.items.size() < 64)
      result.items.push_back(std::move(s));
  }
  if (result.items.empty())
    result.error = "No accessible Plex movie/TV servers found. Check server "
                   "sharing and connectivity.";
  return result;
}
Result Client::connect(Connection &c, const Pair &p, const Server &s,
                       const std::atomic<bool> *cancel) const {
  Result r;
  r.error = "Could not reach this Plex server. Check LAN access, HTTPS and "
            "server availability.";
  if (!safe_id(c.id) || !safe_id(s.id) || !credential(s.token) ||
      !safe_id(p.device))
    return r;
  for (const auto &base : s.urls) {
    if (cancel && cancel->load())
      return {{}, "Cancelled"};
    Connection candidate;
    candidate.id = c.id;
    candidate.kind = "plex";
    candidate.device = p.device;
    candidate.user = s.id;
    candidate.user_name = "Plex";
    candidate.server_name = s.name;
    candidate.token = s.token;
    candidate.base = normalize(base, "plex");
    candidate.auth = {
        {"key", p.key}, {"kid", p.kid}, {"accountToken", p.token}};
    if (!candidate.valid())
      continue;
    auto root = decode(call(candidate, "/", nullptr, cancel));
    if (!root.ok())
      continue;
    if (jstr(container(root.data), "machineIdentifier") != s.id) {
      r.error = "Plex server identity did not match the selected server";
      continue;
    }
    c = std::move(candidate);
    return {{}, ""};
  }
  return r;
}
Result Client::prepare(Connection &c, bool force, const std::string &failed,
                       const std::atomic<bool> *cancel) const {
  if (!c.valid())
    return {{}, "Invalid Plex connection"};
  std::unique_lock<std::mutex> lock(c.credentials->mutex);
  auto &state = *c.credentials;
  if (!state.initialized) {
    state.token = c.token;
    state.auth = c.auth;
    state.initialized = true;
  }
  c.token = state.token;
  c.auth = state.auth;
  if (force && c.token != failed)
    return {};
  double exp = expiry(jstr(c.auth, "accountToken")),
         server_exp = expiry(c.token), now = now_epoch_ms() / 1000.;
  if (!force && !(exp > 0 && exp < now + 120) &&
      !(server_exp > 0 && server_exp < now + 120))
    return {};
  Pair p;
  p.device = c.device;
  p.key = jstr(c.auth, "key");
  p.kid = jstr(c.auth, "kid");
  if (p.key.empty())
    return {{}, "Plex access expired. Reconnect in Media servers."};
  auto nonce = cloud(c.device, "", "/auth/nonce", nullptr, cancel);
  if (!nonce.ok())
    return nonce;
  auto value = jstr(nonce.data, "nonce");
  if (value.empty() || value.size() > 512)
    return {{}, "Invalid Plex refresh challenge"};
  auto jwt = device_jwt(p, value);
  if (jwt.empty())
    return {{}, "Could not sign Plex refresh request"};
  json body = {{"jwt", jwt}};
  auto refreshed = cloud(c.device, "", "/auth/token", &body, cancel);
  if (!refreshed.ok())
    return refreshed;
  p.token =
      jstr(refreshed.data, "auth_token", jstr(refreshed.data, "authToken"));
  auto resources = servers(p, cancel);
  if (!resources.error.empty())
    return {{}, resources.error};
  for (const auto &s : resources.items)
    if (s.id == c.user) {
      state.token = s.token;
      state.auth["accountToken"] = p.token;
      c.token = state.token;
      c.auth = state.auth;
      return {};
    }
  return {{},
          "This Plex server is no longer shared with the connected account"};
}
HttpResponse Client::server(Connection c, const std::string &path,
                            const json *body,
                            const std::atomic<bool> *cancel) const {
  auto ready = prepare(c, false, "", cancel);
  if (!ready.ok()) {
    HttpResponse r;
    r.error = ready.error;
    return r;
  }
  auto h = call(c, path, body, cancel);
  if (h.status == 401 || h.status == 498) {
    auto r = prepare(c, true, c.token, cancel);
    if (!r.ok()) {
      h.error = r.error;
      return h;
    }
    h = call(c, path, body, cancel);
  }
  return h;
}
Result Client::browse(const Connection &c, const std::string &parent,
                      const std::string &type, const std::string &query,
                      size_t offset, const std::atomic<bool> *cancel) const {
  if (!c.valid() || (!parent.empty() && !safe_id(parent)) ||
      query.size() > 256 || offset > 1000000)
    return {{}, "Invalid Plex library request"};
  std::string path;
  if (type == "views")
    path = "/library/sections";
  else if (type == "resume")
    path = "/hubs/home/continueWatching";
  else if (type == "episodes")
    path = "/library/metadata/" + encode(parent) + "/allLeaves";
  else if (!query.empty())
    path = "/hubs/search?query=" + encode(query) +
           "&limit=100&includeExternalMedia=0";
  else if (parent.starts_with("lib-") && number(parent.substr(4)))
    path = "/library/sections/" + parent.substr(4) + "/all";
  else if (number(parent))
    path = "/library/metadata/" + parent + "/children";
  else
    path = "/library/all";
  path += (path.find('?') == std::string::npos ? "?" : "&") +
          std::string("X-Plex-Container-Start=") + std::to_string(offset) +
          "&X-Plex-Container-Size=100";
  auto h = server(c, path, nullptr, cancel);
  if (type == "resume" && h.status == 404)
    h = server(c,
               "/library/onDeck?X-Plex-Container-Start=" +
                   std::to_string(offset) + "&X-Plex-Container-Size=100",
               nullptr, cancel);
  auto r = decode(h);
  if (!r.ok())
    return r;
  if (!jobj(r.data, "MediaContainer").is_object())
    return {{}, "Plex returned an invalid library response"};
  auto mc = container(r.data);
  json items = json::array();
  auto add = [&](const json &a, bool lib) {
    for (const auto &j : a) {
      auto mapped = map_item(j, lib);
      if (!mapped.is_null() && !mapped.empty() && items.size() < 100)
        items.push_back(std::move(mapped));
    }
  };
  if (type == "views")
    add(list(mc, "Directory"), true);
  else if (mc.contains("Hub")) {
    for (const auto &hub : list(mc, "Hub"))
      add(list(hub, "Metadata"), false);
  } else
    add(list(mc, "Metadata"), false);
  if (offset > 0 && jnum(mc, "offset", -1) != double(offset))
    return {{}, "Plex did not return the requested library page"};
  double total =
      std::clamp(jnum(mc, "totalSize", double(items.size())), 0.0, 1000000.0);
  if (!query.empty() || type == "views")
    total = double(items.size());
  r.data = {{"Items", items}, {"TotalRecordCount", total}};
  return r;
}
Playback Client::playback(const Connection &c, const std::string &item,
                          const std::string &source, bool compatible,
                          const std::atomic<bool> *cancel) const {
  Playback p;
  if (!number(item) || (!source.empty() && !number(source))) {
    p.error = "Invalid Plex video";
    return p;
  }
  auto r = decode(server(c, "/library/metadata/" + item, nullptr, cancel));
  if (!r.ok()) {
    p.error = r.error;
    return p;
  }
  auto metadata = list(container(r.data), "Metadata");
  if (metadata.size() != 1) {
    p.error = "Plex video is unavailable";
    return p;
  }
  p.duration = jnum(metadata[0], "duration") / 1000.;
  auto media = list(metadata[0], "Media");
  size_t index = 0;
  bool selected = false;
  for (; index < media.size(); ++index)
    if (source.empty() || id(media[index], "id") == source) {
      selected = true;
      break;
    }
  if (!selected) {
    p.error = "Plex file version is unavailable";
    return p;
  }
  auto parts = list(media[index], "Part");
  if (parts.size() != 1) {
    p.error =
        "Multi-part Plex files are not supported in this experimental version";
    return p;
  }
  p.source = id(media[index], "id");
  p.session = random_hex(16);
  if (p.session.empty()) {
    p.error = "Could not create Plex playback session";
    return p;
  }
  if (!compatible) {
    auto path = safe_path(jstr(parts[0], "key"), c);
    if (path.empty()) {
      p.error = "Plex returned an unsupported file address";
      return p;
    }
    p.url = c.base + path;
    p.method = "DirectPlay";
  } else {
    std::string args =
        "path=" + encode("/library/metadata/" + item) +
        "&mediaIndex=" + std::to_string(index) +
        "&partIndex=0&protocol=hls&directPlay=0&directStream=0&session=" +
        p.session + "&transcodeSessionId=" + p.session +
        "&videoResolution=1920x1080&maxVideoBitrate=20000&audioChannelCount=2&"
        "location=lan&offset=0";
    const std::string profile =
        "add-transcode-target(type=videoProfile&context=streaming&protocol=hls&"
        "container=mpegts&videoCodec=h264&audioCodec=aac&replace=true)";
    args += "&X-Plex-Client-Profile-Extra=" + encode(profile);
    auto decision = decode(server(
        c, "/video/:/transcode/universal/decision?" + args, nullptr, cancel));
    if (!decision.ok()) {
      p.error = decision.error;
      return p;
    }
    if (!jobj(decision.data, "MediaContainer").is_object()) {
      p.error = "Plex returned an invalid playback decision";
      return p;
    }
    auto mc = container(decision.data);
    if (jnum(mc, "generalDecisionCode") >= 2000 ||
        jnum(mc, "transcodeDecisionCode") >= 2000) {
      p.error = "Plex denied compatible playback. Check server transcoding "
                "permissions and availability.";
      return p;
    }
    p.url = c.base + "/video/:/transcode/universal/start.m3u8?" + args;
    p.method = "Transcode";
  }
  p.headers = c.headers(true);
  p.headers.push_back("X-Plex-Session-Identifier: " + p.session);
  return p;
}
Result Client::report(const Connection &c, const Playback &p,
                      const std::string &item, double pos, bool paused,
                      const std::string &event,
                      const std::atomic<bool> *cancel) const {
  if (!number(item) || !std::isfinite(pos) || pos < 0 || pos > 1e8)
    return {{}, "Invalid Plex progress"};
  auto ms = [](double s) {
    return std::to_string(int64_t(std::clamp(s, 0.0, 1e8) * 1000));
  };
  std::string path = "/:/timeline?ratingKey=" + item +
                     "&key=" + encode("/library/metadata/" + item) + "&state=" +
                     (event == "stop" ? "stopped"
                      : paused        ? "paused"
                                      : "playing") +
                     "&time=" + ms(pos) + "&duration=" + ms(p.duration) +
                     "&session=" + encode(p.session) +
                     "&X-Plex-Session-Identifier=" + encode(p.session);
  auto h = server(c, path, nullptr, cancel);
  if (event == "stop" && p.method == "Transcode")
    server(c, "/video/:/transcode/universal/stop?session=" + encode(p.session),
           nullptr, cancel);
  if (h.ok())
    return {};
  return decode(h);
}
HttpResponse Client::image(const Connection &c, const std::string &item,
                           const std::atomic<bool> *cancel) const {
  if (!number(item)) {
    HttpResponse r;
    r.error = "Plex library has no video poster";
    return r;
  }
  return server(c, "/library/metadata/" + item + "/thumb", nullptr, cancel);
}
} // namespace plex
