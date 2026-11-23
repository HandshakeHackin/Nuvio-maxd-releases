#include "config_drive.h"
#include "config_backup.h"
#include <algorithm>
namespace config_drive {
namespace {
std::string field(const Json &j, const char *k) {
  auto p = j.find(k);
  return j.is_object() && p != j.end() && p->is_string() ? p->get<std::string>()
                                                         : std::string();
}
Json parse(const std::string &s) {
  bool deep = false;
  size_t nodes = 0;
  auto j = Json::parse(
      s,
      [&](int d, Json::parse_event_t, Json &) {
        if (d > 20 || ++nodes > 100000) {
          deep = true;
          return false;
        }
        return true;
      },
      false);
  return deep ? Json() : j;
}
bool identifier(const std::string &s) {
  return !s.empty() && s.size() <= 256 &&
         s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVW"
                             "XYZ0123456789_-") == std::string::npos;
}
bool scope_id(const std::string &s) {
  return s.size() == 64 &&
         s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
bool secret(const std::string &s) {
  return !s.empty() && s.size() <= 8192 &&
         s.find_first_of("\r\n") == std::string::npos &&
         s.find('\0') == std::string::npos;
}
feature_http::Response request(const feature_http::Query &q,
                               const std::atomic<bool> *c,
                               const feature_http::Request &r) {
  return (r ? r : feature_http::perform)(q, c);
}
feature_http::Response
form(const std::string &path,
     const std::vector<std::pair<std::string, std::string>> &fields,
     const std::atomic<bool> *cancel, const feature_http::Request &r) {
  feature_http::Query q;
  q.url = "https://oauth2.googleapis.com/" + path;
  q.method = "POST";
  q.limit = 1u << 20;
  q.headers = {"Content-Type: application/x-www-form-urlencoded"};
  for (const auto &[k, v] : fields) {
    if (!q.body.empty())
      q.body += '&';
    q.body += k + "=" + feature_http::encode(v);
  }
  return request(q, cancel, r);
}
bool configured(const Application &a) {
  return secret(a.client_id) && secret(a.client_secret);
}
Token token(const feature_http::Response &r) {
  Token out;
  auto j = parse(r.body);
  auto e = field(j, "error");
  if (e == "authorization_pending" || e == "slow_down") {
    out.pending = true;
    out.slow = e == "slow_down";
    return out;
  }
  if (!r.ok()) {
    out.error =
        e == "access_denied"   ? "Google sign-in was declined."
        : e == "expired_token" ? "Google code expired. Connect again."
        : e == "invalid_grant"
            ? "Google connection expired. Connect again."
            : "Google sign-in failed. Check the app registration and network.";
    return out;
  }
  out.access = field(j, "access_token");
  out.refresh = field(j, "refresh_token");
  if (!out.refresh.empty() && !secret(out.refresh)) {
    out.refresh.clear();
    out.error = "Google returned an invalid refresh token.";
    return out;
  }
  if (!secret(out.access)) {
    out.access.clear();
    out.error = "Google returned an invalid access token.";
  }
  return out;
}
std::string error(const feature_http::Response &r) {
  return r.status == 412 ? "Cloud backup changed on another device. Sync again "
                           "before choosing a version."
         : r.status == 401 || r.status == 403
             ? "Google Drive rejected access. Reconnect or check the app "
               "registration."
         : r.error == "Response exceeds size limit"
             ? "Cloud backup exceeds its size limit."
             : "Google Drive request failed. Existing backups were retained.";
}
feature_http::Query drive(const std::string &access, const std::string &path,
                          size_t limit) {
  feature_http::Query q;
  q.url = "https://www.googleapis.com/" + path;
  q.limit = limit;
  q.headers = {"Authorization: Bearer " + access};
  return q;
}
} // namespace
Device begin(const Application &a, const std::atomic<bool> *cancel,
             const feature_http::Request &r) {
  Device out;
  if (!configured(a)) {
    out.error =
        "Configure a Google OAuth TV application before connecting Drive.";
    return out;
  }
  auto h = form("device/code",
                {{"client_id", a.client_id},
                 {"scope", "https://www.googleapis.com/auth/drive.file"}},
                cancel, r);
  auto j = parse(h.body);
  if (!h.ok()) {
    out.error =
        "Google device sign-in is unavailable for this app registration.";
    return out;
  }
  out.code = field(j, "device_code");
  out.user_code = field(j, "user_code");
  out.url = field(j, "verification_url");
  if (out.url.empty())
    out.url = field(j, "verification_uri");
  if (!secret(out.code) || out.user_code.empty() || out.user_code.size() > 32 ||
      (out.url != "https://www.google.com/device" &&
       out.url != "https://www.google.com/device/" &&
       out.url != "https://google.com/device")) {
    out = {};
    out.error = "Google returned an invalid device code.";
    return out;
  }
  if (j.contains("interval") && j["interval"].is_number_integer())
    out.interval = std::clamp(j["interval"].get<int>(), 5, 60);
  if (j.contains("expires_in") && j["expires_in"].is_number_integer())
    out.lifetime = std::clamp(j["expires_in"].get<int>(), 60, 1800);
  return out;
}
Token poll(const Application &a, const std::string &code,
           const std::atomic<bool> *cancel, const feature_http::Request &r) {
  if (!configured(a) || !secret(code))
    return {{}, {}, "Google connection setup is incomplete."};
  return token(
      form("token",
           {{"client_id", a.client_id},
            {"client_secret", a.client_secret},
            {"device_code", code},
            {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}},
           cancel, r));
}
Token refresh(const Application &a, const std::atomic<bool> *cancel,
              const feature_http::Request &r) {
  if (!configured(a) || !secret(a.refresh_token))
    return {{}, {}, "Connect Google Drive first."};
  return token(form("token",
                    {{"client_id", a.client_id},
                     {"client_secret", a.client_secret},
                     {"refresh_token", a.refresh_token},
                     {"grant_type", "refresh_token"}},
                    cancel, r));
}
Remote download(const std::string &access, const std::string &scope,
                const std::atomic<bool> *cancel,
                const feature_http::Request &r) {
  Remote out;
  if (!secret(access) || !scope_id(scope)) {
    out.error = "Invalid cloud backup context.";
    return out;
  }
  const auto query =
      "trashed = false and appProperties has { key='nuvio_scope' and value='" +
      scope + "' }";
  auto h = request(
      drive(access,
            "drive/v3/"
            "files?spaces=drive&pageSize=10&fields=files(id),nextPageToken&q=" +
                feature_http::encode(query),
            1u << 20),
      cancel, r);
  auto j = parse(h.body);
  if (!h.ok()) {
    out.error = error(h);
    return out;
  }
  if (!j.is_object() || !j.contains("files") || !j["files"].is_array() ||
      !field(j, "nextPageToken").empty() || j["files"].size() > 1) {
    out.error = "Multiple or invalid cloud backups found. Resolve duplicate "
                "backups in Drive before syncing.";
    return out;
  }
  if (j["files"].empty()) {
    out.missing = true;
    return out;
  }
  out.id = field(j["files"][0], "id");
  if (!identifier(out.id)) {
    out.error = "Invalid cloud backup identifier.";
    return out;
  }
  h = request(drive(access, "drive/v3/files/" + out.id + "?alt=media",
                    config_backup::max_encrypted),
              cancel, r);
  if (!h.ok()) {
    out.error = error(h);
    return out;
  }
  out.bytes = std::move(h.body);
  out.etag = h.etag;
  return out;
}
Remote upload(const std::string &access, const std::string &scope,
              const Remote &previous, const std::string &bytes,
              const std::atomic<bool> *cancel, const feature_http::Request &r) {
  Remote out;
  if (!secret(access) || !scope_id(scope) || bytes.size() < 52 ||
      bytes.size() > config_backup::max_encrypted) {
    out.error = "Invalid cloud backup context or size.";
    return out;
  }
  feature_http::Query q;
  if (previous.missing) {
    // Check again immediately before create. Concurrent first-time creations
    // remain separate files and are reported as a conflict on the next sync.
    auto current = download(access, scope, cancel, r);
    if (!current.error.empty())
      return current;
    if (!current.missing) {
      out.error = "A cloud backup appeared on another device. Sync again.";
      return out;
    }
    const auto boundary = "maxd_" + config_backup::hash(bytes).substr(0, 32);
    Json metadata{
        {"name", "NuvioMaxd-configuration-" + scope.substr(0, 16) + ".nmb"},
        {"mimeType", "application/octet-stream"},
        {"appProperties", {{"nuvio_scope", scope}, {"nuvio_format", "1"}}}};
    q = drive(access, "upload/drive/v3/files?uploadType=multipart&fields=id",
              1u << 20);
    q.method = "POST";
    q.headers.push_back("Content-Type: multipart/related; boundary=" +
                        boundary);
    q.body = "--" + boundary +
             "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n" +
             metadata.dump() + "\r\n--" + boundary +
             "\r\nContent-Type: application/octet-stream\r\n\r\n" + bytes +
             "\r\n--" + boundary + "--\r\n";
  } else {
    if (!identifier(previous.id) || previous.etag.empty() ||
        previous.etag.size() > 1024 ||
        previous.etag.find_first_of("\r\n") != std::string::npos) {
      out.error = "Drive supplied no usable revision tag. Update was cancelled "
                  "to protect concurrent changes.";
      return out;
    }
    q = drive(access,
              "upload/drive/v3/files/" + previous.id +
                  "?uploadType=media&fields=id",
              1u << 20);
    q.method = "PATCH";
    q.body = bytes;
    q.headers.push_back("Content-Type: application/octet-stream");
    q.headers.push_back("If-Match: " + previous.etag);
  }
  auto h = request(q, cancel, r);
  if (!h.ok()) {
    out.error = error(h);
    return out;
  }
  out = download(access, scope, cancel, r);
  if (out.error.empty() && (out.missing || out.bytes != bytes))
    out.error = "Cloud upload could not be verified. Sync again before making "
                "another change.";
  return out;
}
Plan plan(const std::string &base, const std::string &local,
          const std::string &remote, bool missing) {
  if (missing)
    return base.empty() ? Plan::first_upload : Plan::conflict;
  if (local == remote)
    return Plan::equal;
  if (base.empty())
    return Plan::conflict;
  if (local == base)
    return Plan::restore;
  if (remote == base)
    return Plan::upload;
  return Plan::conflict;
}
} // namespace config_drive
