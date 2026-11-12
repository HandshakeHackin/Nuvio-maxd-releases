#include "config_backup.h"
#include "feature_http.h"
#include "live_tv.h"
#include "util.h"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace config_backup {
namespace {
std::recursive_mutex io_mutex;
std::atomic<unsigned> sequence{0};
constexpr const char *journal_name = "/.configuration-rollback.json";
bool stopped(const std::atomic<bool> *c) { return c && *c; }
Json parse(const std::string &s) {
  bool deep = false;
  size_t nodes = 0;
  auto j = Json::parse(
      s,
      [&](int d, Json::parse_event_t, Json &) {
        if (d > 32 || ++nodes > 500000) {
          deep = true;
          return false;
        }
        return true;
      },
      false);
  return deep ? Json() : j;
}
std::string str(const Json &j, const char *k) {
  auto p = j.find(k);
  return j.is_object() && p != j.end() && p->is_string() ? p->get<std::string>()
                                                         : std::string();
}
bool usb_id(const std::string &id) {
  return !id.empty() && id.size() <= 40 &&
         id.find_first_not_of("0123456789") == std::string::npos;
}
bool usb_name(const std::string &n) {
  return n.size() > 17 && n.rfind("usb-playlist-", 0) == 0 &&
         n.ends_with(".m3u") && usb_id(n.substr(13, n.size() - 17));
}
bool allowed(const std::string &n) {
  return n == "profile-settings.json" || n == "live-tv.json" ||
         n == "tvdb.json" || usb_name(n);
}
Json project(const Json &j, const char *file) {
  Json out = Json::object();
  if (!j.is_object())
    return out;
  if (std::string(file) == "profile-settings.json") {
    for (const auto *k :
         {"extra_addons", "provider_enabled", "provider_settings",
          "subtitle_languages", "audio_languages", "auto_subtitles",
          "source_preferences", "sports_hub"})
      if (j.contains(k))
        out[k] = j[k];
  } else if (std::string(file) == "live-tv.json") {
    for (const auto *k : {"sources", "selected", "favorites", "recent",
                          "guide_enabled", "guide_grid"})
      if (j.contains(k))
        out[k] = j[k];
  } else if (std::string(file) == "tvdb.json")
    for (const auto *k : {"api_key", "pin", "orders"})
      if (j.contains(k))
        out[k] = j[k];
  return out;
}
bool valid_file(const std::string &name, const std::string &bytes,
                std::string &error) {
  if (!is_valid_utf8(bytes)) {
    error = "Configuration contains unsupported text encoding. Existing data "
            "was retained.";
    return false;
  }
  if (!allowed(name) || bytes.size() > max_plain) {
    error = "Backup contains an unsupported configuration file.";
    return false;
  }
  if (usb_name(name)) {
    if (bytes.empty() || !live_tv::parse(bytes, "").error.empty()) {
      error = "Backup contains an invalid imported playlist.";
      return false;
    }
    return true;
  }
  auto j = parse(bytes);
  if (!j.is_object() || project(j, name.c_str()) != j) {
    error = "Backup contains invalid configuration data.";
    return false;
  }
  if (name == "profile-settings.json") {
    for (const auto *k : {"provider_enabled", "provider_settings",
                          "source_preferences", "sports_hub"})
      if (j.contains(k) && !j[k].is_object()) {
        error = "Invalid profile preferences.";
        return false;
      }
    if (j.contains("extra_addons")) {
      if (!j["extra_addons"].is_array() || j["extra_addons"].size() > 128) {
        error = "Invalid addon configuration.";
        return false;
      }
      for (const auto &u : j["extra_addons"])
        if (!u.is_string() || !live_tv::http_url(u.get<std::string>())) {
          error = "Invalid addon URL in backup.";
          return false;
        }
    }
    for (const auto *k : {"subtitle_languages", "audio_languages"})
      if (j.contains(k) &&
          (!j[k].is_string() ||
           j[k].get_ref<const std::string &>().size() > 4096)) {
        error = "Invalid language preferences.";
        return false;
      }
    if (j.contains("auto_subtitles") && !j["auto_subtitles"].is_boolean()) {
      error = "Invalid subtitle preference.";
      return false;
    }
  }
  if (name == "tvdb.json") {
    if ((j.contains("api_key") &&
         (!j["api_key"].is_string() || str(j, "api_key").size() > 4096)) ||
        (j.contains("pin") &&
         (!j["pin"].is_string() || str(j, "pin").size() > 256)) ||
        (j.contains("orders") &&
         (!j["orders"].is_object() || j["orders"].size() > 2000))) {
      error = "Invalid TVDB preferences.";
      return false;
    }
    if (j.contains("orders"))
      for (const auto &order : j["orders"].items())
        if (order.key().size() > 256 || !order.value().is_string() ||
            order.value().get_ref<const std::string &>().size() > 64) {
          error = "Invalid TVDB order.";
          return false;
        }
  }
  if (name == "live-tv.json") {
    if (j.contains("sources")) {
      if (!j["sources"].is_array() || j["sources"].size() > 16) {
        error = "Invalid Live TV sources.";
        return false;
      }
      std::set<std::string> ids;
      for (const auto &s : j["sources"]) {
        auto id = str(s, "id"), type = str(s, "type");
        if (!s.is_object() || id.empty() || id.size() > 128 ||
            id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRS"
                                 "TUVWXYZ0123456789_-") != std::string::npos ||
            !ids.insert(id).second ||
            (type != "usb" && type != "m3u" && type != "xtream" &&
             type != "stalker") ||
            str(s, "name").size() > 256 ||
            (type == "usb" ? !usb_id(id)
                           : !feature_http::safe_url(str(s, "url"), false)) ||
            str(s, "username").size() > 256 ||
            str(s, "password").size() > 256 || str(s, "mac").size() > 17 ||
            str(s, "serial").size() > 256) {
          error = "Invalid Live TV source in backup.";
          return false;
        }
      }
    }
    for (const auto *k : {"favorites", "recent"})
      if (j.contains(k)) {
        if (!j[k].is_array() || j[k].size() > 100000) {
          error = "Invalid channel preferences.";
          return false;
        }
        for (const auto &v : j[k])
          if (!v.is_string() || v.get_ref<const std::string &>().size() > 512) {
            error = "Invalid channel preference.";
            return false;
          }
      }
    for (const auto *k : {"guide_enabled", "guide_grid"})
      if (j.contains(k) && !j[k].is_boolean()) {
        error = "Invalid guide preference.";
        return false;
      }
  }
  return true;
}
struct Key {
  unsigned char bytes[32]{};
  ~Key() { OPENSSL_cleanse(bytes, sizeof bytes); }
};
bool derive(Key &key, const std::string &pass, const unsigned char *salt) {
  return valid_passphrase(pass) &&
         PKCS5_PBKDF2_HMAC(pass.data(), int(pass.size()), salt, 16, 600000,
                           EVP_sha256(), sizeof key.bytes, key.bytes) == 1;
}
bool sync_directory(const std::string &directory) {
  int fd = open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
  if (fd < 0)
    return false;
  const bool ok = fsync(fd) == 0;
  const int e = errno;
  close(fd);
  return ok || e == EINVAL || e == ENOTSUP;
}
bool rollback(const std::string &dir, const Json &originals,
              std::string &error) {
  bool ok = originals.is_object() && originals.size() <= 19;
  if (ok)
    for (const auto &i : originals.items())
      if (!allowed(i.key()) || (!i.value().is_null() && !i.value().is_string()))
        ok = false;
  if (!ok) {
    error = "Configuration recovery journal is invalid. Profile settings were "
            "not loaded.";
    return false;
  }
  for (const auto &i : originals.items()) {
    if (i.value().is_null()) {
      if (unlink((dir + "/" + i.key()).c_str()) && errno != ENOENT)
        ok = false;
    } else if (!save(dir + "/" + i.key(), i.value().get<std::string>()))
      ok = false;
  }
  if (ok)
    ok = sync_directory(dir) && unlink((dir + journal_name).c_str()) == 0 &&
         sync_directory(dir);
  if (!ok)
    error = "Configuration restore needs recovery. Check app storage and "
            "reopen this profile.";
  return ok;
}
} // namespace
bool valid_passphrase(const std::string &pass) {
  return pass.size() <= 1024 && is_valid_utf8(pass) &&
         std::count_if(pass.begin(), pass.end(), [](unsigned char c) {
           return (c & 0xc0) != 0x80;
         }) >= 12;
}
std::string hash(const std::string &bytes) {
  unsigned char out[EVP_MAX_MD_SIZE];
  unsigned n = 0;
  if (!EVP_Digest(bytes.data(), bytes.size(), out, &n, EVP_sha256(), nullptr))
    return {};
  const char *hex = "0123456789abcdef";
  std::string s;
  for (unsigned i = 0; i < n; ++i) {
    s += hex[out[i] >> 4];
    s += hex[out[i] & 15];
  }
  return s;
}
std::string scope(const std::string &account, int profile) {
  return hash((account.empty() ? "guest" : account) + ":" +
              std::to_string(profile));
}
bool read(const std::string &path, std::string &out, size_t limit,
          bool *missing) {
  out.clear();
  if (missing)
    *missing = false;
  int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) {
    if (missing)
      *missing = errno == ENOENT;
    return false;
  }
  struct stat before{}, after{};
  bool ok = fstat(fd, &before) == 0 && S_ISREG(before.st_mode) &&
            before.st_size >= 0 && uint64_t(before.st_size) <= limit;
  char b[65536];
  while (ok) {
    auto n = ::read(fd, b, sizeof b);
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0) {
      ok = false;
      break;
    }
    if (!n)
      break;
    if (size_t(n) > limit - out.size()) {
      ok = false;
      break;
    }
    out.append(b, size_t(n));
  }
  ok = ok && fstat(fd, &after) == 0 && before.st_size == after.st_size &&
       before.st_mtime == after.st_mtime &&
       out.size() == size_t(before.st_size);
  close(fd);
  if (!ok)
    out.clear();
  return ok;
}
bool save(const std::string &path, const std::string &bytes) {
  if (bytes.size() > max_plain * 2)
    return false;
  const auto tmp = path + ".tmp-" + std::to_string(getpid()) + "-" +
                   std::to_string(sequence++);
  int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd < 0)
    return false;
  bool ok = fchmod(fd, 0600) == 0;
  size_t at = 0;
  while (ok && at < bytes.size()) {
    auto n = write(fd, bytes.data() + at, bytes.size() - at);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      ok = false;
      break;
    }
    at += size_t(n);
  }
  if (ok)
    ok = fsync(fd) == 0;
  if (close(fd))
    ok = false;
  if (ok)
    ok = rename(tmp.c_str(), path.c_str()) == 0;
  if (!ok)
    unlink(tmp.c_str());
  return ok;
}
bool validate(const Json &j, const std::string &expected, std::string &error) {
  error.clear();
  if (!j.is_object() || j.size() != 3 || expected.size() != 64 ||
      expected.find_first_not_of("0123456789abcdef") != std::string::npos ||
      !j.contains("version") || j["version"] != 1 ||
      str(j, "scope") != expected || !j.contains("files") ||
      !j["files"].is_object() || j["files"].size() > 19) {
    error = "Backup format or account/profile does not match.";
    return false;
  }
  const auto &files = j["files"];
  for (const auto *f : {"profile-settings.json", "live-tv.json", "tvdb.json"})
    if (!files.contains(f)) {
      error = "Backup configuration is incomplete.";
      return false;
    }
  for (const auto &i : files.items()) {
    if (!i.value().is_string() ||
        !valid_file(i.key(), i.value().get<std::string>(), error)) {
      if (error.empty())
        error = "Backup contains invalid configuration data.";
      return false;
    }
  }
  if (j.dump().size() > max_plain) {
    error = "Configuration backup exceeds 16 MiB.";
    return false;
  }
  auto live = parse(files["live-tv.json"].get<std::string>());
  std::set<std::string> usb;
  if (live.contains("sources"))
    for (const auto &s : live["sources"])
      if (str(s, "type") == "usb") {
        const auto name = "usb-playlist-" + str(s, "id") + ".m3u";
        usb.insert(name);
        if (!files.contains(name)) {
          error = "Backup is missing an imported USB playlist.";
          return false;
        }
      }
  for (const auto &i : files.items())
    if (usb_name(i.key()) && !usb.count(i.key())) {
      error = "Backup contains an unreferenced playlist.";
      return false;
    }
  return true;
}
Result capture(const std::string &dir, const std::string &expected,
               const std::atomic<bool> *cancel) {
  std::lock_guard<std::recursive_mutex> lock(io_mutex);
  Result r;
  if (!recover(dir, r.error))
    return r;
  Json files = Json::object();
  std::map<std::string, std::string> originals;
  size_t total = 0;
  for (const auto *f : {"profile-settings.json", "live-tv.json", "tvdb.json"}) {
    std::string bytes;
    bool missing = false;
    if (!read(dir + "/" + f, bytes, max_plain, &missing) && !missing) {
      r.error = "Could not read profile configuration.";
      return r;
    }
    auto j = missing ? Json::object() : parse(bytes);
    if (!j.is_object()) {
      r.error = "Profile configuration is invalid.";
      return r;
    }
    originals[f] = bytes;
    files[f] = project(j, f).dump();
  }
  auto live = parse(files["live-tv.json"].get<std::string>());
  if (live.contains("sources") && live["sources"].is_array())
    for (const auto &s : live["sources"])
      if (str(s, "type") == "usb") {
        const auto name = "usb-playlist-" + str(s, "id") + ".m3u";
        std::string bytes;
        if (!usb_name(name) || !read(dir + "/" + name, bytes, max_plain)) {
          r.error = "Could not read an imported playlist for backup.";
          return r;
        }
        total += bytes.size();
        if (total > max_plain) {
          r.error = "Configuration backup exceeds 16 MiB.";
          return r;
        }
        files[name] = bytes;
        originals[name] = bytes;
      }
  // Detect configuration writes during collection; retry later, never upload a
  // torn snapshot.
  for (const auto &[name, bytes] : originals) {
    std::string now;
    bool missing = false;
    if ((!read(dir + "/" + name, now, max_plain, &missing) && !missing) ||
        now != bytes) {
      r.error = "Configuration changed while making the backup. Retry.";
      return r;
    }
  }
  r.snapshot = {{"version", 1}, {"scope", expected}, {"files", files}};
  if (stopped(cancel)) {
    r.error = "Cancelled";
    r.snapshot = Json();
    return r;
  }
  if (!validate(r.snapshot, expected, r.error)) {
    r.snapshot = Json();
    return r;
  }
  r.hash = hash(r.snapshot.dump());
  return r;
}
Result encrypt(const Json &snapshot, const std::string &pass,
               const std::atomic<bool> *cancel) {
  Result r;
  if (!validate(snapshot, str(snapshot, "scope"), r.error))
    return r;
  auto plain = snapshot.dump();
  if (stopped(cancel)) {
    r.error = "Cancelled";
    return r;
  }
  std::string header = "NMXDBK01";
  header.resize(36);
  if (RAND_bytes(reinterpret_cast<unsigned char *>(header.data() + 8), 28) !=
      1) {
    r.error = "Secure random source unavailable.";
    return r;
  }
  Key key;
  if (!derive(key, pass,
              reinterpret_cast<const unsigned char *>(header.data() + 8))) {
    r.error = "Use at least 12 characters and at most 1024 bytes for the "
              "backup passphrase.";
    return r;
  }
  auto *ctx = EVP_CIPHER_CTX_new();
  int n = 0, last = 0;
  std::string cipher(plain.size() + 16, '\0');
  unsigned char tag[16]{};
  bool ok =
      ctx &&
      EVP_EncryptInit_ex(
          ctx, EVP_aes_256_gcm(), nullptr, key.bytes,
          reinterpret_cast<const unsigned char *>(header.data() + 24)) == 1 &&
      EVP_EncryptUpdate(ctx, nullptr, &n,
                        reinterpret_cast<const unsigned char *>(header.data()),
                        header.size()) == 1 &&
      EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(cipher.data()),
                        &n,
                        reinterpret_cast<const unsigned char *>(plain.data()),
                        plain.size()) == 1;
  const int count = n;
  if (ok)
    ok = EVP_EncryptFinal_ex(
             ctx, reinterpret_cast<unsigned char *>(cipher.data() + count),
             &last) == 1 &&
         EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) == 1;
  if (ctx)
    EVP_CIPHER_CTX_free(ctx);
  OPENSSL_cleanse(plain.data(), plain.size());
  if (!ok || stopped(cancel)) {
    r.error = stopped(cancel) ? "Cancelled" : "Backup encryption failed.";
    return r;
  }
  cipher.resize(count + last);
  r.bytes = header + cipher + std::string(reinterpret_cast<char *>(tag), 16);
  r.hash = hash(snapshot.dump());
  return r;
}
Result decrypt(const std::string &bytes, const std::string &pass,
               const std::string &expected, const std::atomic<bool> *cancel) {
  Result r;
  if (bytes.size() < 52 || bytes.size() > max_encrypted ||
      bytes.substr(0, 8) != "NMXDBK01") {
    r.error = "Invalid or oversized encrypted backup.";
    return r;
  }
  if (stopped(cancel)) {
    r.error = "Cancelled";
    return r;
  }
  Key key;
  if (!derive(key, pass,
              reinterpret_cast<const unsigned char *>(bytes.data() + 8))) {
    r.error = "Use at least 12 characters and at most 1024 bytes for the "
              "backup passphrase.";
    return r;
  }
  auto *ctx = EVP_CIPHER_CTX_new();
  std::string plain(bytes.size() - 52 + 16, '\0');
  int n = 0, last = 0;
  bool ok =
      ctx &&
      EVP_DecryptInit_ex(
          ctx, EVP_aes_256_gcm(), nullptr, key.bytes,
          reinterpret_cast<const unsigned char *>(bytes.data() + 24)) == 1 &&
      EVP_DecryptUpdate(ctx, nullptr, &n,
                        reinterpret_cast<const unsigned char *>(bytes.data()),
                        36) == 1 &&
      EVP_DecryptUpdate(
          ctx, reinterpret_cast<unsigned char *>(plain.data()), &n,
          reinterpret_cast<const unsigned char *>(bytes.data() + 36),
          bytes.size() - 52) == 1;
  const int count = n;
  if (ok)
    ok = EVP_CIPHER_CTX_ctrl(
             ctx, EVP_CTRL_GCM_SET_TAG, 16,
             const_cast<char *>(bytes.data() + bytes.size() - 16)) == 1 &&
         EVP_DecryptFinal_ex(
             ctx, reinterpret_cast<unsigned char *>(plain.data() + count),
             &last) == 1;
  if (ctx)
    EVP_CIPHER_CTX_free(ctx);
  if (!ok) {
    OPENSSL_cleanse(plain.data(), plain.size());
    r.error = "Backup passphrase is incorrect or the backup was changed.";
    return r;
  }
  plain.resize(count + last);
  r.snapshot = parse(plain);
  OPENSSL_cleanse(plain.data(), plain.size());
  if (stopped(cancel)) {
    r.error = "Cancelled";
    r.snapshot = Json();
    return r;
  }
  if (!validate(r.snapshot, expected, r.error)) {
    r.snapshot = Json();
    return r;
  }
  r.hash = hash(r.snapshot.dump());
  return r;
}
bool recover(const std::string &dir, std::string &error) {
  std::lock_guard<std::recursive_mutex> lock(io_mutex);
  std::string body;
  bool missing = false;
  if (!read(dir + journal_name, body, max_plain * 2, &missing)) {
    if (missing)
      return true;
    error = "Cannot read configuration recovery journal.";
    return false;
  }
  return rollback(dir, parse(body), error);
}
bool restore(const std::string &dir, const Json &snapshot,
             const std::string &expected, std::string &error) {
  std::lock_guard<std::recursive_mutex> lock(io_mutex);
  if (!validate(snapshot, expected, error) || !recover(dir, error))
    return false;
  Json originals = Json::object();
  size_t journal_bytes = 2;
  for (const auto &i : snapshot["files"].items()) {
    std::string bytes;
    bool missing = false;
    if (!read(dir + "/" + i.key(), bytes, max_plain, &missing) && !missing) {
      error = "Could not preserve the current configuration.";
      return false;
    }
    if (!is_valid_utf8(bytes)) {
      error = "Cannot preserve a configuration file with unsupported text "
              "encoding.";
      return false;
    }
    auto original = missing ? Json() : Json(bytes);
    const size_t encoded = original.dump().size() + i.key().size() + 8;
    if (encoded > max_plain * 2 - journal_bytes) {
      error = "Current configuration exceeds the recovery journal limit. "
              "Restore cancelled.";
      return false;
    }
    journal_bytes += encoded;
    originals[i.key()] = std::move(original);
  }
  if (!save(dir + journal_name, originals.dump()) || !sync_directory(dir)) {
    error = "Could not create the configuration recovery journal.";
    return false;
  }
  for (const auto &i : snapshot["files"].items())
    if (!save(dir + "/" + i.key(), i.value().get<std::string>())) {
      std::string recovery;
      if (!rollback(dir, originals, recovery)) {
        error = recovery;
        return false;
      }
      error =
          "Could not restore configuration. Previous settings were recovered.";
      return false;
    }
  if (!sync_directory(dir) || unlink((dir + journal_name).c_str()) ||
      !sync_directory(dir)) {
    error = "Could not finish the restore. Reopen the profile to recover it.";
    return false;
  }
  return true;
}
} // namespace config_backup
