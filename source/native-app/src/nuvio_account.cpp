// Nuvio account protocol: device pairing, refresh rotation and addon sync.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "nuvio_account.h"
#include "stremio.h"
#include "http.h"
#include "watch_sync.h"
#include <memory>
#include <openssl/rand.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <mutex>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

namespace {
std::mutex mutex;
std::atomic<unsigned> generation{0};
std::atomic<bool> saved{false};
std::string base, public_key, link_base, legacy_base, dir;
std::string session_binding;
json session;
std::unique_ptr<WatchSync> watch_sync;
double server_epoch = 0, server_observed_at = 0;
struct Pairing {
    std::string token, nonce, code;
    unsigned gen = 0;
} pairing;

double account_time() {
    return server_epoch > 0 ? server_epoch + now_seconds() - server_observed_at : std::time(nullptr);
}

// Parse UTC arithmetically rather than depending on the console libc's
// calendar conversion or local timezone. PostgREST also emits fractional
// seconds and explicit timezone offsets.
double expiry_epoch(const json& value) {
    if (value.is_number()) {
        const double n = value.get<double>();
        return std::isfinite(n) && n > 0 ? (n > 100000000000.0 ? n / 1000.0 : n) : 0;
    }
    if (!value.is_string()) return 0;
    const std::string s = value.get<std::string>();
    const auto digits = [&](size_t offset, size_t count) {
        if (offset + count > s.size()) return -1;
        int n = 0;
        for (size_t i = offset; i < offset + count; ++i) {
            if (s[i] < '0' || s[i] > '9') return -1;
            n = n * 10 + s[i] - '0';
        }
        return n;
    };
    if (s.size() < 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':') return 0;
    int year = digits(0, 4), month = digits(5, 2), day = digits(8, 2);
    const int hour = digits(11, 2), minute = digits(14, 2), second = digits(17, 2);
    if (year < 1970 || month < 1 || month > 12 || day < 1 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second > 59) return 0;
    static const int days_in_month[]{31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if (day > days_in_month[month - 1] + (month == 2 && leap ? 1 : 0)) return 0;
    size_t position = 19;
    double fraction = 0, place = 0.1;
    if (s[position] == '.') {
        const size_t begin = ++position;
        while (position < s.size() && s[position] >= '0' && s[position] <= '9') {
            fraction += (s[position++] - '0') * place; place *= 0.1;
        }
        if (position == begin) return 0;
    }
    int zone_seconds = 0;
    if (position + 1 == s.size() && (s[position] == 'Z' || s[position] == 'z')) { /* UTC */ }
    else if (position + 6 == s.size() && (s[position] == '+' || s[position] == '-') && s[position + 3] == ':') {
        const int zh = digits(position + 1, 2), zm = digits(position + 4, 2);
        if (zh < 0 || zh > 14 || zm < 0 || zm > 59 || (zh == 14 && zm != 0)) return 0;
        zone_seconds = (zh * 3600 + zm * 60) * (s[position] == '+' ? 1 : -1);
    } else return 0;
    year -= month <= 2;
    const int era = year / 400, yoe = year - era * 400;
    const int shifted_month = month + (month > 2 ? -3 : 9);
    const int doy = (153 * shifted_month + 2) / 5 + day - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const double days = double(era) * 146097 + doe - 719468;
    return days * 86400 + hour * 3600 + minute * 60 + second + fraction - zone_seconds;
}

int pairing_lifetime(const json& row, const HttpResponse& response) {
    double seconds = jnum(row, "expires_in", -1);
    if (!std::isfinite(seconds) || seconds < 0) {
        double absolute = expiry_epoch(jobj(row, "expires_at"));
        if (!absolute) absolute = expiry_epoch(jobj(row, "expires_at_millis"));
        // With no trusted server time, leave the code alive and let the
        // backend report expiry. Never subtract the PS5's wall clock.
        seconds = absolute > 0 && response.server_epoch > 0 ? absolute - response.server_epoch : 600;
    }
    return int(std::ceil(std::max(0.0, std::min(86400.0, seconds))));
}

ApiResult failure(const std::string& reason) {
    ApiResult r; r.error = reason; return r;
}
ApiResult success(json value) {
    ApiResult r; r.ok = true; r.result = std::move(value); return r;
}
json parse(const HttpResponse& h) { return json::parse(h.body, nullptr, false); }
json row(const json& j) { return j.is_array() && !j.empty() ? j[0] : j; }
bool missing(const HttpResponse& h) {
    const json j = parse(h);
    const std::string code = jstr(j, "code");
    return h.status == 404 || code == "PGRST202" || code == "PGRST205" || code == "42P01";
}
HttpResponse request(const std::string& path, const json* body, const std::string& token) {
    const std::vector<std::string> headers{"apikey: " + public_key, "Authorization: Bearer " + token};
    auto response = body ? http_post_json(base + path, body->dump(), 25, nullptr, headers)
                         : http_get(base + path, 25, nullptr, headers);
    if (response.error.empty() && response.server_epoch > 0) {
        server_epoch = response.server_epoch; server_observed_at = now_seconds();
    }
    return response;
}
bool persist(const json& value) {
    // Write the rotated access/refresh pair together. Never delete the previous
    // session if writing or renaming the replacement fails.
    const std::string path = dir + "/account.json", temp = path + ".tmp";
    FILE* f = fopen(temp.c_str(), "wb");
    if (!f) return false;
    chmod(temp.c_str(), 0600);
    const std::string data = value.dump();
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = fflush(f) == 0 && ok;
    ok = fsync(fileno(f)) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (!ok || rename(temp.c_str(), path.c_str()) != 0) {
        remove(temp.c_str()); return false;
    }
    return true;
}
bool install(json value, bool keep_refresh) {
    if (value.contains("session") && value["session"].is_object()) value = value["session"];
    if (!value.is_object() || jstr(value, "access_token").empty()) return false;
    if (jbool(jobj(value, "user"), "is_anonymous")) return false;
    if (jstr(value, "refresh_token").empty() && keep_refresh)
        value["refresh_token"] = jstr(session, "refresh_token");
    if (keep_refresh && !jobj(value, "user").is_object()) value["user"] = jobj(session, "user");
    if (jstr(value, "refresh_token").empty()) return false;
    value["expires_at"] = jnum(value, "expires_at", account_time() + jnum(value, "expires_in", 3600));
    if (!persist(value)) return false;
    session = std::move(value); saved = true;
    if (!keep_refresh) session_binding = "nuvio-session-" + pairing.nonce;
    return true;
}
bool refresh(std::string& error) {
    const json body{{"refresh_token", jstr(session, "refresh_token")}};
    const auto h = request("/auth/v1/token?grant_type=refresh_token", &body, public_key);
    if (!h.ok()) {
        // Transient errors leave the refresh token intact for a later launch.
        const json j = parse(h);
        const std::string code = jstr(j, "error_code", jstr(j, "code"));
        const bool revoked = code == "refresh_token_not_found" || code == "refresh_token_already_used" ||
                             code == "invalid_grant" || code == "session_not_found";
        if (revoked) { persist(json::object()); session = json::object(); saved = false; }
        error = revoked ? "Nuvio session expired; sign in again" : "Nuvio refresh unavailable (" + h.describe() + ")";
        return false;
    }
    if (!install(parse(h), true)) { error = "Could not save refreshed Nuvio session"; return false; }
    return true;
}
HttpResponse authorized(const std::string& path, const json* body = nullptr) {
    HttpResponse h;
    if (!saved) { h.error = "Nuvio session required"; return h; }
    if (jnum(session, "expires_at") < account_time() + 60 && !refresh(h.error)) return h;
    h = request(path, body, jstr(session, "access_token"));
    if (h.status == 401) {
        std::string error;
        if (!refresh(error)) { h.error = error; return h; }
        h = request(path, body, jstr(session, "access_token"));
    }
    return h;
}
WatchSync* account_progress() {
    if (!saved) return nullptr;
    std::string owner = jstr(jobj(session, "user"), "id");
    if (owner.empty()) {
        const auto h = authorized("/auth/v1/user");
        if (!h.ok()) return nullptr;
        session["user"] = parse(h); owner = jstr(session["user"], "id");
        persist(session);
    }
    if (owner.empty()) return nullptr;
    if (!watch_sync || watch_sync->owner() != owner) watch_sync = std::make_unique<WatchSync>(dir, owner);
    return watch_sync.get();
}
HttpResponse progress_rpc(const std::string& name, const json& body) {
    return authorized("/rest/v1/rpc/" + name, &body);
}
bool enabled(const json& r) {
    const auto& v = jobj(r, "enabled");
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number()) return v.get<double>() != 0;
    return !v.is_string() || (lower(v.get<std::string>()) != "false" && v != "0");
}
ApiResult cached_addons(const std::string& error) {
    json cached;
    if (saved && load_json(dir + "/account-addons.json", cached) && cached.is_array())
        return success(json{{"addons", cached}, {"offline", true}});
    return failure(error);
}
}

bool nuvio_account_init(const std::string& path, const std::string& data_dir) {
    std::lock_guard<std::mutex> lock(mutex);
    json config;
    if (!load_json(path, config)) return false;
    const json& env = jobj(config, "env");
    base = jstr(env, "NUVIO_SUPABASE_URL");
    public_key = jstr(env, "NUVIO_SUPABASE_ANON_KEY");
    link_base = jstr(env, "DEVICE_LOGIN_WEB_BASE_URL", "https://nuvio.tv/link");
    legacy_base = jstr(env, "TV_LOGIN_WEB_BASE_URL", "https://nuvio.tv/tv-login");
    if (!starts_with(base, "https://") || public_key.empty()) return false;
    dir = data_dir;
    session = json::object();
    watch_sync.reset();
    session_binding = "nuvio-session";
    server_epoch = 0; server_observed_at = 0;
    pairing = Pairing(); ++generation;
    load_json(dir + "/account.json", session);
    saved = !jstr(session, "access_token").empty() && !jstr(session, "refresh_token").empty();
    return true;
}
bool nuvio_account_saved() { return saved; }
std::string nuvio_account_user_id() {
    std::lock_guard<std::mutex> lock(mutex);
    return saved ? jstr(jobj(session, "user"), "id") : "";
}
void nuvio_pairing_cancel() { ++generation; }

LinkCode link_create() {
    std::lock_guard<std::mutex> lock(mutex);
    LinkCode out;
    pairing = Pairing(); pairing.gen = ++generation;
    const unsigned gen = pairing.gen;
    if (base.empty()) { out.error = "Nuvio account configuration missing"; return out; }
    json body{{"data", {{"tv_client", "ps5"}}}};
    auto h = request("/auth/v1/signup", &body, public_key);
    if (!h.ok()) {
        body = json::object();
        h = request("/auth/v1/token?grant_type=anonymous", &body, public_key);
    }
    if (!h.ok()) { out.error = h.describe(); return out; }
    json j = parse(h);
    pairing.token = jstr(j, "access_token", jstr(jobj(j, "session"), "access_token"));
    unsigned char random[32];
    if (pairing.token.empty() || RAND_bytes(random, sizeof(random)) != 1) {
        out.error = "Could not create secure device session"; return out;
    }
    static const char hex[] = "0123456789abcdef";
    for (auto b : random) { pairing.nonce += hex[b >> 4]; pairing.nonce += hex[b & 15]; }
    body = json{{"p_device_nonce", pairing.nonce}, {"p_redirect_base_url", link_base},
                {"p_device_type", "tv"}, {"p_device_name", "Nuvio Max'd (PS5)"}};
    h = request("/rest/v1/rpc/start_device_login_session", &body, pairing.token);
    if (missing(h)) {
        body.erase("p_device_type"); body["p_redirect_base_url"] = legacy_base;
        h = request("/rest/v1/rpc/start_tv_login_session", &body, pairing.token);
        if (missing(h)) {
            body.erase("p_device_name");
            h = request("/rest/v1/rpc/start_tv_login_session", &body, pairing.token);
        }
    }
    if (!h.ok()) { out.error = h.describe(); return out; }
    j = row(parse(h));
    out.code = jstr(j, "device_code", jstr(j, "code"));
    out.display_code = jstr(j, "user_code", out.code);
    out.verification_uri = jstr(j, "verification_uri", link_base);
    out.link = jstr(j, "verification_uri_complete", jstr(j, "qr_content", jstr(j, "web_url")));
    out.poll_interval = std::max(1, std::min(30, int(jnum(j, "poll_interval_seconds", 3))));
    out.expires_in = pairing_lifetime(j, h);
    if (out.expires_in == 0) { out.error = "Nuvio returned an expired sign-in code. Please retry."; return out; }
    dlog("nuvio: pairing lifetime %d seconds", out.expires_in);
    if (out.code.empty() || out.link.empty() || gen != generation) {
        out.error = "Incomplete or cancelled Nuvio pairing"; return out;
    }
    pairing.code = out.code; out.ok = true; return out;
}

ApiResult link_read(const std::string& code) {
    std::lock_guard<std::mutex> lock(mutex);
    if (code != pairing.code || pairing.gen != generation) return failure("Pairing cancelled");
    json body{{"p_code", code}, {"p_device_nonce", pairing.nonce}};
    auto h = request("/rest/v1/rpc/poll_tv_login_session", &body, pairing.token);
    if (!h.ok()) return failure("Pairing check unavailable (" + h.describe() + ")");
    const std::string state = lower(jstr(row(parse(h)), "status", "pending"));
    if (state != "approved") {
        auto result = failure(state == "pending" ? "" : "Pairing " + state);
        result.result = json{{"terminal", state == "expired" || state == "denied" || state == "rejected" ||
                                         state == "cancelled" || state == "consumed"}};
        return result;
    }
    body = json{{"code", code}, {"device_nonce", pairing.nonce}};
    h = request("/functions/v1/tv-logins-exchange", &body, pairing.token);
    if (!h.ok()) return failure("Nuvio sign-in unavailable (" + h.describe() + ")");
    if (pairing.gen != generation) return failure("Pairing cancelled");
    if (!install(parse(h), false)) return failure("Could not save Nuvio login; check app storage");
    return success(json{{"authKey", session_binding}});
}

ApiResult api_get_user(const std::string&) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto h = authorized("/auth/v1/user");
    if (!h.ok()) return failure(h.describe());
    const auto user = parse(h);
    if (user.is_object()) { session["user"] = user; persist(session); }
    return success(user);
}
ApiResult api_addon_collection(const std::string&) {
    std::lock_guard<std::mutex> lock(mutex);
    const json empty = json::object();
    auto h = authorized("/rest/v1/rpc/get_sync_owner", &empty);
    json owner = parse(h);
    if (missing(h)) {
        h = authorized("/auth/v1/user"); owner = jstr(parse(h), "id");
    }
    if (!h.ok() || !owner.is_string() || owner.get<std::string>().empty())
        return cached_addons("Could not resolve Nuvio addon owner (" + h.describe() + ")");
    const std::string id = url_encode(owner.get<std::string>());
    h = authorized("/rest/v1/addons?user_id=eq." + id + "&profile_id=eq.1&select=*&order=sort_order.asc");
    if (missing(h)) h = authorized("/rest/v1/tv_addons?owner_id=eq." + id + "&select=*&order=position.asc");
    if (missing(h)) {
        const json body{{"p_profile_id", 1}};
        h = authorized("/rest/v1/rpc/sync_pull_addons", &body);
    }
    json rows = parse(h);
    if (!h.ok() || !rows.is_array()) return cached_addons("Nuvio addons unavailable (" + h.describe() + ")");
    json list = json::array();
    json cached; load_json(dir + "/account-addons.json", cached);
    int failed = 0;
    for (const auto& r : rows) {
        if (!enabled(r)) continue;
        std::string url;
        for (const char* field : {"url", "base_url", "baseUrl", "transportUrl", "manifest_url"}) {
            url = jstr(r, field);
            if (!url.empty()) break;
        }
        if (url.empty()) continue;
        url = normalize_addon_url(url);
        Addon a; std::string error;
        if (fetch_addon(url, a, error)) list.push_back(json{{"transportUrl", url}, {"manifest", a.manifest}});
        else {
            ++failed;
            if (cached.is_array()) for (const auto& c : cached)
                if (jstr(c, "transportUrl") == url) { list.push_back(c); break; }
        }
    }
    if (failed && list.empty()) return failure("Nuvio addon manifests could not be reached; retry in Settings");
    if (!save_json(dir + "/account-addons.json", list)) return failure("Could not save account addons");
    chmod((dir + "/account-addons.json").c_str(), 0600);
    return success(json{{"addons", list}});
}
ApiResult api_library_get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!saved || (!key.empty() && key != session_binding)) return failure("Nuvio session changed; refresh Watch Progress");
    json local; load_json(dir + "/native-library.json", local);
    if (!local.is_array()) local = json::array();
    json remote = json::array();
    for (int offset = 0; offset < 10000; offset += 500) {
        const json body{{"p_profile_id", 1}, {"p_limit", 500}, {"p_offset", offset}};
        const auto h = authorized("/rest/v1/rpc/sync_pull_library", &body);
        const json rows = parse(h);
        if (!h.ok() || !rows.is_array()) { remote = local; break; }
        for (const auto& r : rows) {
            const std::string id = jstr(r, "content_id", jstr(r, "contentId"));
            if (id.empty()) continue;
            remote.push_back(json{{"_id", id}, {"type", jstr(r, "content_type", "movie")},
                {"name", jstr(r, "name", "Untitled")}, {"poster", jstr(r, "poster")},
                {"background", jstr(r, "background")}, {"removed", false}});
        }
        if (rows.size() < 500) break;
    }
    for (const auto& item : local) {
        bool found = false;
        for (auto& r : remote) if (jstr(r, "_id") == jstr(item, "_id")) {
            r["state"] = jobj(item, "state"); found = true; break;
        }
        if (!found) remote.push_back(item);
    }
    if (auto* progress = account_progress()) apply_watch_progress(remote, progress->pull(progress_rpc, now_seconds()));
    return success(remote);
}
ApiResult api_library_put(const std::string& key, const json& changes) {
    // Keep the native library schema on-device. Only portable resume fields
    // go to Nuvio's separate Watch Progress RPCs.
    std::lock_guard<std::mutex> lock(mutex);
    if (!saved || (!key.empty() && key != session_binding)) return failure("Watch Progress update belongs to a previous login");
    json items; load_json(dir + "/native-library.json", items);
    if (!items.is_array()) items = json::array();
    if (changes.is_array()) for (const auto& c : changes) {
        json item = c;
        if (jobj(item, "state").is_object()) item["state"]["nuvioSyncManaged"] = true;
        bool found = false;
        for (auto& v : items) if (jstr(v, "_id") == jstr(c, "_id")) { v = item; found = true; break; }
        if (!found) items.push_back(item);
    }
    if (!save_json(dir + "/native-library.json", items)) return failure("Could not save local progress");
    auto* progress = account_progress();
    if (!progress) return success(json{{"progressSynced", false}});
    std::string error;
    if (!progress->write(changes, progress_rpc, now_seconds(), int64_t(account_time() * 1000), error)) return failure(error);
    return success(json{{"progressSynced", progress->synced()}});
}
ApiResult api_connected_services_pull() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!saved) return failure("Sign in to Nuvio to import Connected Services.");
    std::string user_id = jstr(jobj(session, "user"), "id");
    if (user_id.empty()) {
        const auto user = authorized("/auth/v1/user");
        if (!user.ok()) return failure("Could not verify your Nuvio account. Try again.");
        session["user"] = parse(user); user_id = jstr(session["user"], "id"); persist(session);
    }
    if (user_id.empty()) return failure("Could not verify your Nuvio account. Sign in again.");
    const json empty = json::object(), body{{"p_profile_id", 1}};
    auto h = authorized("/rest/v1/rpc/get_sync_owner", &empty);
    json owner = parse(h);
    if (missing(h)) {
        h = authorized("/auth/v1/user"); owner = jstr(parse(h), "id");
    }
    if (!h.ok() || !owner.is_string() || owner.get<std::string>().empty())
        return failure("Could not identify the Nuvio profile. Reconnect or try again.");
    h = authorized("/rest/v1/rpc/sync_pull_provider_credentials", &body);
    const json rows = parse(h);
    if (!h.ok() || !rows.is_array()) return failure(missing(h)
        ? "This Nuvio account does not offer provider credential sync yet. Connect a service here."
        : "Connected Services sync is unavailable. Your existing connections are unchanged.");
    json credentials = json::array();
    for (const auto& r : rows) if (jstr(r, "provider") == "debrid:torbox" || jstr(r, "provider") == "debrid:premiumize" ||
                                   jstr(r, "provider") == "animeskip")
        credentials.push_back(json{{"provider", jstr(r, "provider")}, {"credential_json", r.contains("credential_json") ? r["credential_json"] : jobj(r, "credentialJson")}});
    json result{{"owner_id", owner}, {"account_user_id", user_id}, {"credentials", credentials}};
    const json settings_body{{"p_profile_id", 1}, {"p_platform", "tv"}};
    const auto prefs = authorized("/rest/v1/rpc/sync_pull_profile_settings_blob", &settings_body);
    if (prefs.ok()) {
        const auto data = row(parse(prefs));
        const json blob = jobj(data, "settings_json").is_object() ? data["settings_json"] : jobj(data, "settingsJson");
        const auto& settings = jobj(jobj(blob, "features"), "debrid_settings");
        if (settings.is_object()) {
            // Import resolver preferences, leaving cloud-library settings separate.
            if (jobj(settings, "preferred_resolver_provider_id").is_string())
                result["preferred"] = settings["preferred_resolver_provider_id"];
            if (jobj(settings, "debrid_enabled").is_boolean()) result["enabled"] = settings["debrid_enabled"];
            if (jobj(settings, "instant_playback_preparation_limit").is_number())
                result["instant_limit"] = int(std::clamp(jnum(settings, "instant_playback_preparation_limit"), 0.0, 5.0));
        }
    }
    return success(std::move(result));
}

ApiResult api_logout(const std::string&) {
    ++generation;
    std::lock_guard<std::mutex> lock(mutex);
    if (!persist(json::object())) return failure("Could not clear saved login");
    session = json::object(); saved = false;
    session_binding.clear();
    watch_sync.reset();
    remove((dir + "/account-addons.json").c_str());
    remove((dir + "/native-library.json").c_str());
    remove((dir + "/watch-sync.json").c_str());
    return success(json::object());
}
