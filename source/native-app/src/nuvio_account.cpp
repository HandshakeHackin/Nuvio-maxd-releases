// Nuvio account protocol: device pairing, refresh rotation and addon sync.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "nuvio_account.h"
#include "stremio.h"
#include "http.h"
#include "watch_sync.h"
#include "tracking.h"
#include <memory>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <atomic>
#include <algorithm>
#include <set>
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
json profiles = json::array();
int active_profile = 1;
std::string profile_root, profile_dir;
unsigned context_revision = 0;
ApiResult update_source_locked(const std::string&, bool, bool);
bool flush_sources(std::string&);
json filter_pending_removals(json);

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
    if (!keep_refresh) { session_binding = "nuvio-session-" + pairing.nonce; ++context_revision; }
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
// Account/profile identifiers never become raw path components. Auth tokens
// remain in account.json; data caches live under a stable account hash.
std::string owner_id() { return jstr(jobj(session, "user"), "id"); }
std::string owner_root() {
    const auto id = owner_id();
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(id.data()), id.size(), digest);
    std::string hash;
    for (auto b : digest) { char hex[3]; snprintf(hex, sizeof hex, "%02x", b); hash += hex; }
    return dir + "/profiles/" + hash;
}
json selected_profile() {
    for (const auto& p : profiles) if (jnum(p, "profile_index") == active_profile) return p;
    return json{{"profile_index", active_profile}, {"name", "Profile " + std::to_string(active_profile)}};
}
int source_profile(bool plugin) {
    return jbool(selected_profile(), plugin ? "uses_primary_plugins" : "uses_primary_addons") ? 1 : active_profile;
}
std::string context_key() {
    return session_binding + ":" + std::to_string(active_profile) + ":" + std::to_string(context_revision);
}
bool valid_context(const std::string& key) {
    return saved && (key.empty() || key == context_key());
}
bool establish_context() {
    if (!saved) return false;
    if (owner_id().empty()) {
        const auto h = authorized("/auth/v1/user");
        if (!h.ok() || jstr(parse(h), "id").empty()) return false;
        session["user"] = parse(h);
        if (!persist(session)) return false;
    }
    const auto root = owner_root();
    if (profile_root != root) {
        profile_root = root; active_profile = 1; profiles = json::array();
        json state;
        if (load_json(root + "/selection.json", state)) {
            const int selected = int(jnum(state, "profile", 1));
            if (selected >= 1 && selected <= 6) active_profile = selected;
        }
        load_json(root + "/profiles.json", profiles);
        if (!profiles.is_array()) profiles = json::array();
        ++context_revision;
    }
    profile_dir = root + "/" + std::to_string(active_profile);
    if (!make_dirs(profile_dir)) return false;
    chmod(root.c_str(), 0700); chmod(profile_dir.c_str(), 0700);
    json claim;
    const std::string marker = dir + "/legacy-profile-owner.json";
    if (!load_json(marker, claim)) {
        json old; load_json(dir + "/watch-sync.json", old);
        const auto old_owner = jstr(old, "owner");
        if (!save_json(marker, json{{"owner", old_owner.empty() ? "unassigned" : old_owner}})) return false;
        load_json(marker, claim);
    }
    const auto primary = root + "/1";
    json migrated;
    if (jstr(claim, "owner") == owner_id() && !load_json(root + "/legacy-migrated.json", migrated)) {
        if (!make_dirs(primary)) return false;
        for (const auto* file : {"progress.json", "native-library.json", "watch-sync.json", "account-addons.json", "connected-services.json"}) {
            json old, existing;
            if (!load_json(primary + "/" + file, existing) && load_json(dir + "/" + file, old))
                if (!save_json(primary + "/" + file, old)) return false;
        }
        json settings, existing;
        if (!load_json(primary + "/profile-settings.json", existing) && load_json(dir + "/settings.json", settings)) {
            json scoped;
            for (const auto* field : {"extra_addons", "provider_enabled", "provider_settings", "subtitle_languages", "audio_languages", "auto_subtitles"})
                if (settings.contains(field)) scoped[field] = settings[field];
            if (!save_json(primary + "/profile-settings.json", scoped)) return false;
        }
        if (!save_json(root + "/legacy-migrated.json", json{{"done", true}})) return false;
    }
    return true;
}
json context_result() {
    return json{{"profiles", profiles}, {"profile", selected_profile()},
                {"path", profile_dir}, {"authKey", context_key()}, {"userEmail", jstr(jobj(session, "user"), "email")}};
}
json canonical_profiles(const json& rows) {
    json result = json::array();
    std::set<int> ids;
    if (!rows.is_array() || rows.size() > 6) return result;
    for (auto p : rows) {
        const double value = jnum(p, "profile_index", jnum(p, "profileIndex", jnum(p, "id", 0)));
        if (!std::isfinite(value) || value < 1 || value > 6) return json::array();
        const int id = int(value);
        if (!p.is_object() || value != id || !ids.insert(id).second) return json::array();
        p["profile_index"] = id;
        p["name"] = jstr(p, "name", "Profile " + std::to_string(id));
        p["avatar_color_hex"] = jstr(p, "avatar_color_hex", jstr(p, "avatarColorHex", "#1E88E5"));
        for (const auto* flag : {"uses_primary_addons", "uses_primary_plugins"}) {
            const char* camel = std::string(flag) == "uses_primary_addons" ? "usesPrimaryAddons" : "usesPrimaryPlugins";
            const auto& v = p.contains(flag) ? p[flag] : jobj(p, camel);
            p[flag] = v == true || v == 1 || v == "true" || v == "1";
        }
        result.push_back(p);
    }
    std::sort(result.begin(), result.end(), [](const json& a, const json& b) {
        return jnum(a, "profile_index") < jnum(b, "profile_index");
    });
    return result;
}
bool pull_profiles(std::string& error) {
    const json empty = json::object();
    auto h = authorized("/rest/v1/rpc/sync_pull_profiles", &empty);
    if (missing(h)) {
        auto owner = authorized("/rest/v1/rpc/get_sync_owner", &empty);
        auto id = parse(owner);
        if (!owner.ok() || !id.is_string()) { error = "Could not resolve profile owner"; return false; }
        h = authorized("/rest/v1/profiles?user_id=eq." + url_encode(id.get<std::string>()) + "&select=*&order=profile_index.asc");
        if (missing(h)) h = authorized("/rest/v1/tv_profiles?owner_id=eq." + url_encode(id.get<std::string>()) + "&select=*&order=profile_index.asc");
    }
    const auto rows = parse(h);
    if (!h.ok() || !rows.is_array()) { error = "Could not sync profiles (" + h.describe() + ")"; return false; }
    json next = canonical_profiles(rows);
    if (rows.empty()) next = json::array({json{{"profile_index", 1}, {"name", "Profile 1"}, {"avatar_color_hex", "#1E88E5"}}});
    if (next.empty() || std::none_of(next.begin(), next.end(), [](const json& p) { return jnum(p, "profile_index") == 1; })) { error = "Nuvio returned unsupported profile identifiers"; return false; }
    const auto locks = authorized("/rest/v1/rpc/sync_pull_profile_locks", &empty);
    const auto lock_rows = parse(locks);
    if (!locks.ok() || !lock_rows.is_array()) {
        error = "Could not check profile locks. Retry when Nuvio Sync is available."; return false;
    }
    for (auto& p : next) {
        p["pin_enabled"] = false;
        for (const auto& l : lock_rows)
            if (jnum(l, "profile_index", jnum(l, "profileIndex")) == jnum(p, "profile_index"))
                p["pin_enabled"] = jbool(l, "pin_enabled", jbool(l, "pinEnabled"));
    }
    if (!save_json(profile_root + "/profiles.json", next)) { error = "Could not save profiles"; return false; }
    profiles = std::move(next); return true;
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
    if (!watch_sync || watch_sync->owner() != owner) watch_sync = std::make_unique<WatchSync>(profile_dir, owner, active_profile);
    return watch_sync.get();
}
HttpResponse progress_rpc(const std::string& name, const json& body) {
    return authorized("/rest/v1/rpc/" + name, &body);
}
std::string source_url(const json& r) {
    for (const auto* field : {"url", "url_template", "urlTemplate", "base_url", "baseUrl", "transportUrl", "manifest_url"}) {
        const auto value = jstr(r, field); if (!value.empty()) return value;
    }
    return "";
}
bool enabled(const json& r) {
    const auto& v = jobj(r, "enabled");
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number()) return v.get<double>() != 0;
    return !v.is_string() || (lower(v.get<std::string>()) != "false" && v != "0");
}
ApiResult cached_addons(const std::string& error) {
    json cached;
    if (saved && load_json(profile_dir + "/account-addons.json", cached) && cached.is_array())
        return success(json{{"addons", filter_pending_removals(cached)}, {"offline", true}});
    return failure(error);
}
}

bool nuvio_account_init(const std::string& path, const std::string& data_dir) {
    std::lock_guard<std::mutex> lock(mutex);
    json tracker_applications;
    load_json(path_dir(path) + "/tracker-applications.json", tracker_applications);
    tracking::set_applications(tracker_applications);

    json config;
    if (!load_json(path, config)) return false;
    const json& env = jobj(config, "env");
    base = jstr(env, "NUVIO_SUPABASE_URL");
    public_key = jstr(env, "NUVIO_SUPABASE_ANON_KEY");
    link_base = jstr(env, "DEVICE_LOGIN_WEB_BASE_URL", "https://nuvio.tv/link");
    legacy_base = jstr(env, "TV_LOGIN_WEB_BASE_URL", "https://nuvio.tv/tv-login");
    if (!starts_with(base, "https://") || public_key.empty()) return false;
    dir = data_dir;
    profile_root.clear(); profile_dir = dir + "/guest"; active_profile = 1; profiles = json::array();
    ++context_revision;
    session = json::object();
    watch_sync.reset();
    session_binding = "nuvio-session";
    server_epoch = 0; server_observed_at = 0;
    pairing = Pairing(); ++generation;
    load_json(dir + "/account.json", session);
    saved = !jstr(session, "access_token").empty() && !jstr(session, "refresh_token").empty();
    if (saved && !owner_id().empty()) establish_context();
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
    // Pairing has succeeded once the session is durably stored. A temporary
    // profile/user lookup failure must not turn that into a failed sign-in.
    establish_context();
    return success(json{{"authKey", context_key()}});
}

ApiResult api_get_user(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key)) return failure("Nuvio session changed");
    const auto h = authorized("/auth/v1/user");
    if (!h.ok()) return failure(h.describe());
    const auto user = parse(h);
    if (user.is_object()) { session["user"] = user; persist(session); }
    return success(user);
}
ApiResult api_addon_collection(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key) || !establish_context()) return failure("Nuvio profile changed; reload addons");
    std::string sync_error;
    flush_sources(sync_error);
    const json empty = json::object();
    auto h = authorized("/rest/v1/rpc/get_sync_owner", &empty);
    json owner = parse(h);
    if (missing(h)) {
        h = authorized("/auth/v1/user"); owner = jstr(parse(h), "id");
    }
    if (!h.ok() || !owner.is_string() || owner.get<std::string>().empty())
        return cached_addons("Could not resolve Nuvio addon owner (" + h.describe() + ")");
    const std::string id = url_encode(owner.get<std::string>());
    h = authorized("/rest/v1/addons?user_id=eq." + id + "&profile_id=eq." + std::to_string(source_profile(false)) + "&select=*&order=sort_order.asc");
    if (missing(h) && source_profile(false) == 1) h = authorized("/rest/v1/tv_addons?owner_id=eq." + id + "&select=*&order=position.asc");
    if (missing(h)) {
        const json body{{"p_profile_id", source_profile(false)}};
        h = authorized("/rest/v1/rpc/sync_pull_addons", &body);
    }
    json rows = parse(h);
    if (!h.ok() || !rows.is_array()) return cached_addons("Nuvio addons unavailable (" + h.describe() + ")");
    const auto plugin_response = authorized("/rest/v1/plugins?user_id=eq." + id + "&profile_id=eq." +
        std::to_string(source_profile(true)) + "&select=*&order=sort_order.asc");
    const auto plugin_rows = parse(plugin_response);
    if (plugin_response.ok() && plugin_rows.is_array()) {
        for (auto plugin : plugin_rows) {
            const auto type = jstr(plugin, "repo_type");
            if (!enabled(plugin) || (!type.empty() && type != "NUVIO_JS")) continue;
            plugin["is_plugin"] = true;
            rows.push_back(std::move(plugin));
        }
    } else if (!missing(plugin_response)) return cached_addons("Could not sync this profile's plugins");
    json list = json::array();
    json cached; load_json(profile_dir + "/account-addons.json", cached);
    int failed = 0;
    for (const auto& r : rows) {
        const int expected = source_profile(jbool(r, "is_plugin"));
        if (jnum(r, "profile_id", expected) != expected || !enabled(r)) continue;
        std::string url = source_url(r);
        if (url.empty()) continue;
        url = normalize_addon_url(url);
        Addon a; std::string error;
        if (fetch_addon(url, a, error)) list.push_back(json{{"transportUrl", url}, {"manifest", a.manifest}, {"plugin", jbool(r, "is_plugin")}});
        else {
            ++failed;
            if (cached.is_array()) for (const auto& c : cached)
                if (jstr(c, "transportUrl") == url) { list.push_back(c); break; }
        }
    }
    if (failed && list.empty()) return failure("Nuvio addon manifests could not be reached; retry in Settings");
    if (!save_json(profile_dir + "/account-addons.json", list)) return failure("Could not save account addons");
    chmod((profile_dir + "/account-addons.json").c_str(), 0600);
    return success(json{{"addons", filter_pending_removals(list)}, {"syncError", sync_error}});
}
ApiResult api_library_get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!saved || (!key.empty() && key != context_key())) return failure("Nuvio session changed; refresh Watch Progress");
    if (!establish_context()) return failure("Could not initialize profile");
    tracking::Store trackers(profile_dir);
    const auto tracker_status = trackers.status();
    const auto progress_source = jstr(tracker_status, "progressPreferred", "nuvio");
    const auto local_path = profile_dir + (tracking::provider(progress_source) ? "/native-library-"+progress_source+".json" : "/native-library.json");
    json local; load_json(local_path, local);
    if (!local.is_array()) local = json::array();
    json remote = json::array();
    for (int offset = 0; offset < 10000; offset += 500) {
        const json body{{"p_profile_id", active_profile}, {"p_limit", 500}, {"p_offset", offset}};
        const auto h = authorized("/rest/v1/rpc/sync_pull_library", &body);
        const json rows = parse(h);
        if (!h.ok() || !rows.is_array()) { remote = local; break; }
        for (const auto& r : rows) {
            if (jnum(r, "profile_id", active_profile) != active_profile) continue;
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
    std::string tracker_error, notice;
    if (trackers.source(false) != "nuvio") {
        json external;
        if (trackers.library(external, tracker_error)) {
            remote = std::move(external);
        } else notice = tracker_error + " Using Nuvio Library until it reconnects.";
    } else if (jstr(tracker_status, "libraryPreferred", "nuvio") != "nuvio")
        notice = "Library tracker is disconnected. Using Nuvio Library until it reconnects.";
    // Resume is independently selected, and never inherited from library membership.
    if (progress_source != "nuvio") for (auto& item : remote) item.erase("state");
    bool progress_unavailable = progress_source != "nuvio";
    json progress_rows;
    if (trackers.source(true) != "nuvio" && trackers.progress(progress_rows, tracker_error)) {
        progress_unavailable = false;
        apply_watch_progress(remote, progress_rows);
    } else {
        if (auto* progress = account_progress()) apply_watch_progress(remote, progress->pull(progress_rpc, now_seconds()));
        if (progress_source != "nuvio") notice += (notice.empty() ? "" : " ") +
            (tracker_error.empty() ? tracking::name(progress_source)+" is disconnected." : tracker_error) + " Using Nuvio Sync until it reconnects.";
    }
    // Preserve source-scoped local progress during tracker outages, including
    // previously synced resume entries. Never upload it to the fallback service. Remote membership is not changed by these temporary rows.
    if (tracking::provider(progress_source)) for (const auto& item : local) {
        const auto& state = jobj(item, "state");
        if (jnum(state, "lastWatchedMs") <= 0 || (!progress_unavailable && !trackers.unsent(item))) continue;
        auto it = std::find_if(remote.begin(), remote.end(), [&](const auto& r) { return jstr(r, "_id") == jstr(item, "_id"); });
        if (it != remote.end() && jnum(jobj(*it, "state"), "lastWatchedMs") >= jnum(state, "lastWatchedMs")) continue;
        if (it == remote.end()) { auto extra = item; extra["removed"] = true; extra["temp"] = true; remote.push_back(extra); }
        else (*it)["state"] = state;
    }
    auto result = success(remote); result.error = notice; return result;
}
ApiResult api_library_put(const std::string& key, const json& changes) {
    // Keep the native library schema on-device. Only portable resume fields
    // go to Nuvio's separate Watch Progress RPCs.
    std::lock_guard<std::mutex> lock(mutex);
    if (!saved || (!key.empty() && key != context_key())) return failure("Watch Progress update belongs to a previous login");
    if (!establish_context()) return failure("Could not initialize profile");
    tracking::Store trackers(profile_dir);
    const auto progress_source = jstr(trackers.status(), "progressPreferred", "nuvio");
    const auto local_path = profile_dir + (tracking::provider(progress_source) ? "/native-library-"+progress_source+".json" : "/native-library.json");
    json items; load_json(local_path, items);
    if (!items.is_array()) items = json::array();
    if (changes.is_array()) for (const auto& c : changes) {
        json item = c;
        if (jobj(item, "state").is_object()) item["state"]["nuvioSyncManaged"] = true;
        bool found = false;
        for (auto& v : items) if (jstr(v, "_id") == jstr(c, "_id")) { v = item; found = true; break; }
        if (!found) items.push_back(item);
    }
    if (!save_json(local_path, items)) return failure("Could not save local progress");
    if (tracking::provider(progress_source)) {
        std::string error;
        if (!trackers.write(changes, error)) return failure(error);
        return success(json{{"progressSynced", true}});
    }
    auto* progress = account_progress();
    if (!progress) return success(json{{"progressSynced", false}});
    std::string error;
    if (!progress->write(changes, progress_rpc, now_seconds(), int64_t(account_time() * 1000), error)) return failure(error);
    return success(json{{"progressSynced", progress->synced()}});
}
ApiResult api_connected_services_pull(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key) || !establish_context()) return failure("Sign in to the current Nuvio profile to import Connected Services.");
    std::string user_id = jstr(jobj(session, "user"), "id");
    if (user_id.empty()) {
        const auto user = authorized("/auth/v1/user");
        if (!user.ok()) return failure("Could not verify your Nuvio account. Try again.");
        session["user"] = parse(user); user_id = jstr(session["user"], "id"); persist(session);
    }
    if (user_id.empty()) return failure("Could not verify your Nuvio account. Sign in again.");
    const json empty = json::object(), body{{"p_profile_id", active_profile}};
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
    for (const auto& r : rows) if (jstr(r, "provider") == "debrid:torbox" || jstr(r, "provider") == "debrid:premiumize")
        credentials.push_back(json{{"provider", jstr(r, "provider")}, {"credential_json", r.contains("credential_json") ? r["credential_json"] : jobj(r, "credentialJson")}});
    json result{{"owner_id", owner}, {"account_user_id", user_id}, {"credentials", credentials}};
    const json settings_body{{"p_profile_id", active_profile}, {"p_platform", "tv"}};
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

std::string nuvio_profile_path() {
    std::lock_guard<std::mutex> lock(mutex); return profile_dir;
}
std::string nuvio_profile_key() {
    std::lock_guard<std::mutex> lock(mutex); return saved ? context_key() : "";
}
ApiResult api_profiles_get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key) || !establish_context()) return failure("Nuvio session changed");
    std::string error;
    if (!pull_profiles(error)) return failure(error);
    return success(context_result());
}
ApiResult api_profile_select(const std::string& key, int id, const std::string& pin) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key) || !establish_context()) return failure("Nuvio session changed");
    std::string error;
    if (!pull_profiles(error)) return failure(error);
    json selected;
    for (const auto& p : profiles) if (jnum(p, "profile_index") == id) selected = p;
    if (!selected.is_object()) return failure("This profile no longer exists. Refresh profiles.");
    if (jbool(selected, "pin_enabled")) {
        const json body{{"p_profile_id", id}, {"p_pin", pin}};
        const auto h = authorized("/rest/v1/rpc/verify_profile_pin", &body);
        if (!h.ok() || !jbool(row(parse(h)), "unlocked")) return failure("Profile PIN not accepted. Check the PIN or retry later.");
    }
    const auto next_dir = profile_root + "/" + std::to_string(id);
    if (!make_dirs(next_dir) || !save_json(profile_root + "/selection.json", json{{"profile", id}}))
        return failure("Could not save profile selection");
    active_profile = id; profile_dir = next_dir; watch_sync.reset(); ++context_revision;
    return success(context_result());
}
ApiResult api_profile_create(const std::string& key, const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key) || !establish_context()) return failure("Nuvio session changed");
    if (trim(name).empty() || name.size() > 80) return failure("Enter a profile name of 1 to 80 bytes");
    std::string error;
    if (!pull_profiles(error)) return failure(error);
    std::set<int> used; for (const auto& p : profiles) used.insert(int(jnum(p, "profile_index")));
    int id = 2; while (id <= 6 && used.count(id)) ++id;
    if (id > 6) return failure("This Nuvio account already has six profiles");
    json next = profiles;
    next.push_back(json{{"profile_index", id}, {"name", trim(name)}, {"avatar_color_hex", "#1E88E5"},
                        {"uses_primary_addons", false}, {"uses_primary_plugins", false}});
    json payload = json::array();
    for (const auto& p : next) {
        json entry;
        for (const auto* field : {"profile_index", "name", "avatar_color_hex", "avatar_id", "avatar_url",
                                  "profile_background_id", "profile_background_url", "uses_primary_addons", "uses_primary_plugins"})
            entry[field] = jobj(p, field);
        entry["avatar_color_hex"] = jstr(p, "avatar_color_hex", "#1E88E5");
        entry["uses_primary_addons"] = jbool(p, "uses_primary_addons");
        entry["uses_primary_plugins"] = jbool(p, "uses_primary_plugins");
        payload.push_back(entry);
    }
    const json body{{"p_client_max_profiles", 6}, {"p_profiles", payload}};
    const auto h = authorized("/rest/v1/rpc/sync_push_profiles", &body);
    if (!h.ok()) return failure("Could not create the Nuvio profile (" + h.describe() + "). Existing profiles are unchanged locally.");
    if (!pull_profiles(error)) return failure("Profile creation was sent; refresh profiles to confirm it.");
    auto result = context_result(); result["createdProfile"] = id;
    return success(std::move(result));
}
namespace {
std::string source_client() {
    json cached; const auto path = dir + "/profile-sync-client.json";
    load_json(path, cached);
    auto id = jstr(cached, "id");
    if (!id.empty()) return id;
    unsigned char bytes[16];
    if (RAND_bytes(bytes, sizeof bytes) != 1) return "";
    id = "nuvio-maxd-";
    for (auto b : bytes) { char hex[3]; snprintf(hex, sizeof hex, "%02x", b); id += hex; }
    if (!save_json(path, json{{"id", id}})) return "";
    chmod(path.c_str(), 0600); return id;
}
ApiResult update_source_locked(const std::string& url, bool plugin, bool remove_source) {
    if (source_profile(plugin) != active_profile) return failure("This profile inherits its primary profile's sources. Manage them in the primary profile.");
    const json empty = json::object();
    auto h = authorized("/rest/v1/rpc/get_sync_owner", &empty);
    auto owner = parse(h);
    if (!h.ok() || !owner.is_string()) return failure("Could not resolve sync owner");
    const std::string table = plugin ? "plugins" : "addons";
    h = authorized("/rest/v1/" + table + "?user_id=eq." + url_encode(owner.get<std::string>()) +
        "&profile_id=eq." + std::to_string(active_profile) + "&select=*&order=sort_order.asc");
    json rows = parse(h);
    if (!h.ok() || !rows.is_array()) return failure("Could not read sources; cloud sources were not overwritten");
    json next = json::array(); bool found = false;
    for (const auto& r : rows) {
        const std::string existing = source_url(r);
        if (existing.empty()) return failure("Unrecognized cloud source row; sources were not overwritten");
        if (plugin && jstr(r, "repo_type") != "NUVIO_JS" && jstr(r, "repo_type") != "EXTERNAL_DEX")
            return failure("Cloud contains a newer plugin type; manage sources in Nuvio to preserve it");
        if (normalize_addon_url(existing) == url) { found = true; if (remove_source) continue; }
        // Preserve opaque plugin rows and configured addon URLs unchanged.
        json entry;
        for (const char* field : {"url", "name", "enabled", "sort_order"}) entry[field] = jobj(r, field);
        if (plugin) entry["repo_type"] = jobj(r, "repo_type");
        else entry["url"] = existing;
        next.push_back(entry);
    }
    if (!found && !remove_source) {
        Addon addon; std::string error;
        if (!fetch_addon(url, addon, error)) return failure("Could not read source manifest");
        if (addon.provider_repository != plugin) return failure("Source manifest type changed; remove and add the source again");
        json entry{{"url", url}, {"name", addon.name}, {"enabled", true}, {"sort_order", next.size()}};
        if (plugin) entry["repo_type"] = "NUVIO_JS";
        next.push_back(entry);
    }
    const auto client = source_client();
    if (client.empty()) return failure("Could not save the source sync client identifier");
    const json body{{"p_profile_id", active_profile}, {plugin ? "p_plugins" : "p_addons", next},
                    {"p_origin_client_id", client}};
    h = authorized("/rest/v1/rpc/sync_push_" + table, &body);
    if (!h.ok()) return failure("Source saved on this PS5, but Nuvio Sync failed. Retry from Addons.");
    return success(json::object());
}

json filter_pending_removals(json list) {
    json pending;
    if (!load_json(profile_dir + "/source-pending.json", pending) || !pending.is_array() || !list.is_array()) return list;
    for (const auto& op : pending) if (jbool(op, "remove")) {
        const auto url = jstr(op, "url");
        list.erase(std::remove_if(list.begin(), list.end(), [&](const json& item) {
            return jstr(item, "transportUrl") == url;
        }), list.end());
    }
    return list;
}
bool flush_sources(std::string& error) {
    json pending;
    const auto path = profile_dir + "/source-pending.json";
    if (!load_json(path, pending)) return true;
    if (!pending.is_array() || pending.size() > 64) { error = "Source sync queue is unreadable; cloud sources were not changed"; return false; }
    while (!pending.empty()) {
        const auto op = pending.front();
        const auto result = update_source_locked(jstr(op, "url"), jbool(op, "plugin"), jbool(op, "remove"));
        if (!result.ok) { error = "Source changes are saved on this PS5; sync is pending. " + result.error; return false; }
        pending.erase(pending.begin());
        if (!save_json(path, pending)) { error = "Could not save source sync status"; return false; }
        chmod(path.c_str(), 0600);
    }
    return true;
}
} // namespace
ApiResult api_profile_source_update(const std::string& key, const std::string& url, bool plugin, bool remove_source) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key) || !establish_context()) return failure("Nuvio profile changed");
    if (source_profile(plugin) != active_profile) return failure("This profile inherits its primary profile's sources. Manage them in the primary profile.");
    if (!starts_with(url, "https://")) return failure("Use an HTTPS source manifest URL");
    const auto path = profile_dir + "/source-pending.json";
    json pending = json::array();
    load_json(path, pending);
    if (!pending.is_array() || pending.size() >= 64) return failure("Source sync queue is unavailable or full");
    pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const json& op) {
        return jstr(op, "url") == url && jbool(op, "plugin") == plugin;
    }), pending.end());
    pending.push_back(json{{"url", url}, {"plugin", plugin}, {"remove", remove_source}});
    if (!save_json(path, pending)) return failure("Could not save source sync change");
    chmod(path.c_str(), 0600);
    std::string error;
    if (!flush_sources(error)) return failure(error);
    return success(json::object());
}

namespace {
std::string watched_key(const json& entry) {
    return jstr(entry, "content_id") + ":" + jobj(entry, "season").dump() + ":" + jobj(entry, "episode").dump();
}
json watched_entry(const std::string& video, const std::string& title) {
    auto parts = split(video, ':');
    json entry{{"content_id", video}, {"content_type", "movie"}, {"title", title},
               {"season", nullptr}, {"episode", nullptr}, {"watched_at", int64_t(account_time() * 1000)}};
    if (parts.size() >= 3) {
        const auto integer = [](const std::string& v) {
            return !v.empty() && v.size() <= 6 && std::all_of(v.begin(), v.end(), [](char c) { return c >= '0' && c <= '9'; });
        };
        if (integer(parts.back()) && integer(parts[parts.size() - 2])) {
            entry["episode"] = std::stoi(parts.back()); parts.pop_back();
            entry["season"] = std::stoi(parts.back()); parts.pop_back();
            entry["content_id"] = join(parts, ":"); entry["content_type"] = "series";
        }
    }
    return entry;
}
json watched_state() {
    json state;
    if (!load_json(profile_dir + "/watched-sync.json", state)) {
        state = json{{"entries", json::array()}, {"pending", json::array()}, {"deletes", json::array()}};
        json legacy; load_json(profile_dir + "/progress.json", legacy);
        if (jobj(legacy, "watched").is_array()) for (const auto& video : legacy["watched"])
            if (video.is_string() && !video.get<std::string>().empty()) {
                auto entry = watched_entry(video.get<std::string>(), "");
                state["entries"].push_back(entry); state["pending"].push_back(entry);
            }
    }
    return state;
}
bool save_watched(const json& state) {
    const auto path = profile_dir + "/watched-sync.json";
    if (!save_json(path, state)) return false;
    chmod(path.c_str(), 0600); return true;
}
bool sync_watched(json& state) {
    if (!jobj(state, "entries").is_array() || !jobj(state, "pending").is_array() || !jobj(state, "deletes").is_array()) return false;
    if (!state["deletes"].empty()) {
        const json body{{"p_profile_id", active_profile}, {"p_keys", state["deletes"]}};
        const auto h = authorized("/rest/v1/rpc/sync_delete_watched_items", &body);
        if (!h.ok()) return false;
        state["deletes"] = json::array(); if (!save_watched(state)) return false;
    }
    json remote = json::array();
    bool complete = false;
    for (int page = 1; page <= 50; ++page) {
        const json body{{"p_profile_id", active_profile}, {"p_page", page}, {"p_page_size", 900}};
        const auto h = authorized("/rest/v1/rpc/sync_pull_watched_items", &body);
        const auto rows = parse(h);
        if (!h.ok() || !rows.is_array()) return false;
        for (const auto& row : rows) {
            if (jnum(row, "profile_id", active_profile) != active_profile) continue;
            if (jstr(row, "content_id").empty()) return false;
            json entry;
            for (const auto* field : {"content_id", "content_type", "title", "season", "episode", "watched_at"}) entry[field] = jobj(row, field);
            if (entry["watched_at"].is_string()) entry["watched_at"] = int64_t(expiry_epoch(entry["watched_at"]) * 1000);
            remote.push_back(entry);
        }
        if (rows.size() < 900) { complete = true; break; }
    }
    if (!complete) return false;
    for (const auto& pending : state["pending"]) {
        const auto key = watched_key(pending);
        remote.erase(std::remove_if(remote.begin(), remote.end(), [&](const json& r) { return watched_key(r) == key; }), remote.end());
        remote.push_back(pending);
    }
    state["entries"] = remote;
    if (!save_watched(state)) return false;
    if (!state["pending"].empty()) {
        // Push a merged complete snapshot; never erase another device's rows.
        const json body{{"p_profile_id", active_profile}, {"p_items", remote}};
        const auto h = authorized("/rest/v1/rpc/sync_push_watched_items", &body);
        if (!h.ok()) return false;
        state["pending"] = json::array(); if (!save_watched(state)) return false;
    }
    return true;
}
}
ApiResult api_watched_get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key) || !establish_context()) return failure("Nuvio profile changed");
    tracking::Store trackers(profile_dir);
    if (trackers.source(true) != "nuvio") {
        json rows; std::string error;
        if (trackers.watched(rows,error)) return success(json{{"entries",rows},{"synced",true}});
        return failure(error.empty() ? "Tracker watched history is unavailable; local marks were retained" : error);
    }
    auto state = watched_state();
    if (!jobj(state, "entries").is_array() || !jobj(state, "pending").is_array() || !jobj(state, "deletes").is_array())
        return failure("Watched history cache is unreadable; cloud history was not changed");
    // Persist legacy marks before their first network attempt.
    if (!save_watched(state)) return failure("Could not save watched history");
    const bool synced = sync_watched(state);
    return success(json{{"entries", state["entries"]}, {"synced", synced}});
}
ApiResult api_watched_update(const std::string& key, const std::string& video, const std::string& title, bool watched, const std::string& content, int season, int episode) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key) || !establish_context()) return failure("Nuvio profile changed");
    if (video.empty()) return failure("Missing watched title identifier");
    tracking::Store trackers(profile_dir);
    if (tracking::provider(jstr(trackers.status(),"progressPreferred"))) {
        std::string error;
        if (!trackers.mark(content.empty() ? video : content,season,episode,watched,error)) return failure(error);
        return success(json{{"synced",true}});
    }
    auto state = watched_state();
    if (!jobj(state, "entries").is_array() || !jobj(state, "pending").is_array() || !jobj(state, "deletes").is_array())
        return failure("Watched history cache is unreadable; cloud history was not changed");
    auto entry = watched_entry(video, title);
    if (!content.empty()) {
        entry["content_id"] = content;
        entry["content_type"] = season >= 0 && episode >= 0 ? "series" : "movie";
        entry["season"] = season >= 0 ? json(season) : json(nullptr);
        entry["episode"] = episode >= 0 ? json(episode) : json(nullptr);
    }
    const auto id = watched_key(entry);
    for (const auto* field : {"entries", "pending", "deletes"}) {
        auto& rows = state[field];
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const json& r) { return watched_key(r) == id; }), rows.end());
    }
    if (watched) { state["entries"].push_back(entry); state["pending"].push_back(entry); }
    else {
        json removed{{"content_id", entry["content_id"]}};
        if (entry["season"].is_number()) removed["season"] = entry["season"];
        if (entry["episode"].is_number()) removed["episode"] = entry["episode"];
        state["deletes"].push_back(removed);
    }
    if (!save_watched(state)) return failure("Could not save watched history");
    const bool synced = sync_watched(state);
    return success(json{{"synced", synced}});
}

ApiResult api_logout(const std::string& key) {
    ++generation;
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key)) return failure("Nuvio profile changed; retry sign out");
    if (!persist(json::object())) return failure("Could not clear saved login");
    session = json::object(); saved = false;
    session_binding.clear(); ++context_revision;
    watch_sync.reset();
    profile_root.clear(); profiles = json::array(); active_profile = 1;
    profile_dir = dir + "/guest";
    return success(json::object());
}

ApiResult api_tracking(const std::string& key, const std::string& action, const json& args, const std::atomic<bool>* cancel) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid_context(key) || !establish_context()) return failure("Choose a Nuvio profile before connecting a tracker");
    tracking::Store trackers(profile_dir,{},cancel);
    std::string error;
    if (action == "import_sync") {
        const json body{{"p_profile_id", active_profile}};
        auto response = authorized("/rest/v1/rpc/sync_pull_provider_credentials", &body);
        auto rows = parse(response);
        if (!response.ok() || !rows.is_array()) return failure("Tracker connection import is unavailable; connect here instead");
        const auto p = jstr(args, "provider");
        for (const auto& row : rows) if (jnum(row,"profile_id",active_profile) == active_profile && (jstr(row, "provider") == p || jstr(row, "provider") == "tracking:"+p)) {
            auto value = jobj(row, "credential_json");
            if (value.is_string()) value = json::parse(value.get<std::string>(), nullptr, false);
            auto result = trackers.action("import", {{"provider", p}, {"credential", value}}, error);
            return error.empty() ? success(result) : failure(error);
        }
        return failure("No compatible synced "+tracking::name(p)+" connection found. Connect it on this PS5 instead.");
    }
    auto result = trackers.action(action, args, error);
    return error.empty() ? success(result) : failure(error);
}
