// SPDX-License-Identifier: GPL-3.0-or-later
#include "connected_services.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <regex>
#include <sys/stat.h>
#include <unistd.h>

namespace debrid {
namespace {
bool supported(const std::string& provider) { return provider == "torbox" || provider == "premiumize"; }
json payload(const HttpResponse& response) { return json::parse(response.body, nullptr, false); }
bool accepted(const HttpResponse& response, const json& data) {
    return response.ok() && data.is_object() && !data.is_discarded() &&
           jbool(data, "success", true) && lower(jstr(data, "status")) != "error";
}
std::string failure(const HttpResponse& response, const std::string& provider) {
    // Never display provider response bodies or curl error strings: they may contain keys/URLs.
    const std::string name = provider_name(provider);
    if (response.error == "cancelled") return "cancelled";
    if (response.status == 401 || response.status == 403) return name + " connection expired. Reconnect in Connected Services.";
    if (response.status == 429) return name + " is limiting requests. Wait a moment and try again.";
    if (response.status >= 500) return name + " is temporarily unavailable. Try another stream or retry later.";
    return name + " could not complete the request. Check the connection or try another stream.";
}
std::string basename(std::string name) {
    const auto pos = name.find_last_of("/\\");
    return pos == std::string::npos ? name : name.substr(pos + 1);
}
std::string filename(const json& file) {
    return basename(jstr(file, "name", jstr(file, "short_name", jstr(file, "path"))));
}
std::string normalized(std::string text) {
    text = lower(basename(text));
    for (char& c : text) if (!std::isalnum(static_cast<unsigned char>(c))) c = ' ';
    std::string out;
    for (char c : text) if (c != ' ' || (!out.empty() && out.back() != ' ')) out += c;
    return trim(out);
}
bool video(const json& file) {
    const auto name = lower(filename(file));
    if (name.find("sample") != std::string::npos && jnum(file, "size", jnum(file, "bytes")) < 100000000) return false;
    for (const char* ext : {".mp4", ".mkv", ".webm", ".avi", ".mov", ".m4v", ".ts", ".m2ts"})
        if (name.size() >= std::char_traits<char>::length(ext) && name.ends_with(ext)) return true;
    return starts_with(lower(jstr(file, "mimetype", jstr(file, "mime_type"))), "video/");
}
// Exact numeric episode matching avoids S01E01 accidentally selecting S01E010.
int episode_match(const json& file, int season, int episode) {
    static const std::regex pattern(R"((?:s([0-9]{1,3})e([0-9]{1,4})|([0-9]{1,3})x([0-9]{1,4}))(?![0-9]))", std::regex::icase);
    const std::string name = filename(file);
    std::smatch m;
    if (!std::regex_search(name, m, pattern)) return 0;
    const int s = std::stoi(m[m[1].matched ? 1 : 3].str());
    const int e = std::stoi(m[m[2].matched ? 2 : 4].str());
    return s == season && e == episode ? 1 : -1;
}
bool hash_valid(const std::string& value) {
    return value.size() == 40 && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c); });
}
std::string stream_hash(const ::Stream& stream) {
    std::string hash = lower(trim(jstr(stream.client_resolve, "infoHash", stream.info_hash)));
    if (hash_valid(hash)) return hash;
    const std::string magnet = jstr(stream.client_resolve, "magnetUri");
    const auto pos = lower(magnet).find("xt=urn:btih:");
    if (pos != std::string::npos) hash = lower(magnet.substr(pos + 12, 40));
    return hash_valid(hash) ? hash : "";
}
std::string stream_provider(const ::Stream& stream, const Settings& settings) {
    if (stream.client_resolve.is_object()) return provider_id(jstr(stream.client_resolve, "service"));
    return settings.active_provider();
}
std::string scalar(const json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<int64_t>());
    return "";
}
} // namespace

std::string provider_id(const std::string& value) {
    const auto id = lower(trim(value));
    if (id == "tb") return "torbox";
    if (id == "pm") return "premiumize";
    return supported(id) ? id : "";
}
std::string provider_name(const std::string& value) {
    return provider_id(value) == "torbox" ? "TorBox" : provider_id(value) == "premiumize" ? "Premiumize" : "Connected service";
}
bool valid_key(const std::string& key) {
    return !key.empty() && key.size() <= 4096 && std::all_of(key.begin(), key.end(), [](unsigned char c) { return c > 32 && c < 127; });
}
bool valid_client_id(const std::string& id) {
    return !id.empty() && id.size() <= 256 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == '.';
    });
}
std::string Settings::key(const std::string& provider) const {
    const auto id = provider_id(provider);
    return id == "torbox" ? torbox.key : id == "premiumize" ? premiumize.key : "";
}
std::string Settings::active_provider() const {
    if (!preferred.empty()) return key(preferred).empty() ? "" : provider_id(preferred);
    return !torbox.key.empty() ? "torbox" : !premiumize.key.empty() ? "premiumize" : "";
}
bool Settings::can_resolve(const ::Stream& stream) const {
    if (!enabled || stream.direct_http() || !stream.url.empty() || stream_hash(stream).empty()) return false;
    const std::string provider = stream_provider(stream, *this);
    if (!supported(provider) || !valid_key(key(provider))) return false;
    // Provider-specific addon results are never silently sent to a different service.
    if (!preferred.empty() && provider != preferred) return false;
    return !stream.client_resolve.is_object() ||
           (lower(jstr(stream.client_resolve, "type")) == "debrid" && jbool(stream.client_resolve, "isCached"));
}
void Settings::clear_synced() {
    if (!torbox.sync_owner.empty()) torbox = {};
    if (!premiumize.sync_owner.empty()) premiumize = {};
    if (!animeskip.sync_owner.empty()) animeskip = {};
}
bool load(const std::string& path, Settings& settings) {
    json data;
    if (!load_json(path, data) || !data.is_object()) return false;
    Settings next;
    next.enabled = jbool(data, "enabled"); next.preferred = provider_id(jstr(data, "preferred"));
    next.instant_limit = int(std::clamp(jnum(data, "instant_limit", 2), 0.0, 5.0));
    for (const char* provider : {"torbox", "premiumize"}) {
        Credential& credential = std::string(provider) == "torbox" ? next.torbox : next.premiumize;
        credential.key = trim(jstr(jobj(data, provider), "key"));
        if (!valid_key(credential.key)) credential.key.clear();
        credential.sync_owner = jstr(jobj(data, provider), "sync_owner");
    }
    next.animeskip.key = trim(jstr(jobj(data, "animeskip"), "key"));
    if (!valid_client_id(next.animeskip.key)) next.animeskip.key.clear();
    next.animeskip.sync_owner = jstr(jobj(data, "animeskip"), "sync_owner");
    settings = next; chmod(path.c_str(), 0600); return true;
}
bool save(const std::string& path, const Settings& settings) {
    const json data{{"enabled", settings.enabled}, {"preferred", settings.preferred}, {"instant_limit", settings.instant_limit},
        {"torbox", {{"key", settings.torbox.key}, {"sync_owner", settings.torbox.sync_owner}}},
        {"premiumize", {{"key", settings.premiumize.key}, {"sync_owner", settings.premiumize.sync_owner}}},
        {"animeskip", {{"key", settings.animeskip.key}, {"sync_owner", settings.animeskip.sync_owner}}}};
    const std::string temp = path + ".tmp", body = data.dump();
    FILE* f = fopen(temp.c_str(), "wb");
    if (!f) return false;
    bool ok = fchmod(fileno(f), 0600) == 0;
    ok = fwrite(body.data(), 1, body.size(), f) == body.size() && ok;
    ok = fflush(f) == 0 && ok; ok = fsync(fileno(f)) == 0 && ok; ok = fclose(f) == 0 && ok;
    if (!ok || rename(temp.c_str(), path.c_str()) != 0) { remove(temp.c_str()); return false; }
    return true;
}
bool import_snapshot(Settings& settings, const json& snapshot) {
    const std::string owner = jstr(snapshot, "account_user_id", jstr(snapshot, "owner_id"));
    const json& rows = jobj(snapshot, "credentials");
    if (owner.empty() || !rows.is_array()) return false;
    Settings next = settings;
    for (const auto& row : rows) {
        const std::string provider = jstr(row, "provider");
        if (provider == "animeskip") {
            json value = jobj(row, "credential_json");
            if (value.is_string()) value = json::parse(value.get<std::string>(), nullptr, false);
            if (!value.is_object() || !jobj(value, "client_id").is_string()) return false;
            const std::string id = trim(jstr(value, "client_id"));
            if (!id.empty() && !valid_client_id(id)) return false;
            next.animeskip = {id, owner};
            continue;
        }
        if (!starts_with(provider, "debrid:")) continue;
        const auto id = provider_id(provider.substr(7));
        if (id.empty()) continue;
        json value = jobj(row, "credential_json");
        if (value.is_string()) value = json::parse(value.get<std::string>(), nullptr, false);
        if (!value.is_object() || !jobj(value, "api_key").is_string()) return false;
        const std::string key = trim(jstr(value, "api_key"));
        if (!key.empty() && !valid_key(key)) return false;
        Credential& credential = id == "torbox" ? next.torbox : next.premiumize;
        credential = {key, owner};
    }
    if (snapshot.contains("preferred")) {
        const auto id = provider_id(jstr(snapshot, "preferred"));
        if (jstr(snapshot, "preferred").empty() || !id.empty()) next.preferred = id;
    }
    if (jobj(snapshot, "enabled").is_boolean()) next.enabled = snapshot["enabled"].get<bool>();
    if (jobj(snapshot, "instant_limit").is_number()) next.instant_limit = int(std::clamp(jnum(snapshot, "instant_limit"), 0.0, 5.0));
    settings = std::move(next);
    return true;
}

const json* select_file(const json& files, const ::Stream& stream, int season, int episode) {
    if (!files.is_array()) return nullptr;
    std::vector<const json*> videos;
    for (const auto& f : files) if (video(f)) videos.push_back(&f);
    if (videos.empty()) return nullptr;
    const bool episodic = season >= 0 && episode >= 0;
    const auto fits = [&](const json* file) { return !episodic || episode_match(*file, season, episode) >= 0; };
    const std::string wanted = normalized(jstr(stream.client_resolve, "filename", stream.filename));
    if (!wanted.empty()) for (const json* f : videos) if (normalized(filename(*f)) == wanted && fits(f)) return f;
    const int index = int(jnum(stream.client_resolve, "fileIdx", stream.file_idx));
    if (index >= 0 && static_cast<size_t>(index) < files.size() && video(files[index]) && fits(&files[index])) return &files[index];
    if (episodic) {
        for (const json* f : videos) if (episode_match(*f, season, episode) == 1) return f;
        return videos.size() == 1 && fits(videos[0]) ? videos[0] : nullptr;
    }
    // An explicit filename cannot justify a different cut/encode in a multi-file torrent.
    if (!wanted.empty() && videos.size() > 1) return nullptr;
    return *std::max_element(videos.begin(), videos.end(), [](const json* a, const json* b) {
        return jnum(*a, "size", jnum(*a, "bytes")) < jnum(*b, "size", jnum(*b, "bytes"));
    });
}

HttpResponse Client::request(const std::string& provider, const std::string& path, const std::string& key,
                             const std::string* body, bool form, const std::atomic<bool>* cancel) const {
    HttpResponse failed;
    if (cancel && cancel->load()) { failed.error = "cancelled"; return failed; }
    if (!supported(provider) || (!key.empty() && !valid_key(key))) { failed.error = "invalid connection"; return failed; }
    const std::string base = provider == "torbox" ? "https://api.torbox.app/" : "https://www.premiumize.me/";
    std::vector<std::string> headers{"Accept: application/json"};
    if (!key.empty()) headers.push_back("Authorization: Bearer " + key);
    return transport_(base + path, body, form ? "application/x-www-form-urlencoded" : "application/json", headers, cancel);
}
std::string Client::validate(const std::string& provider, const std::string& key, const std::atomic<bool>* cancel) const {
    if (!supported(provider) || !valid_key(key)) return "Enter a valid API key.";
    const auto response = request(provider, provider == "torbox" ? "v1/api/user/me" : "api/account/info", key, nullptr, false, cancel);
    return accepted(response, payload(response)) ? "" : failure(response, provider);
}
Result Client::resolve(const ::Stream& stream, const Settings& settings, int season, int episode, const std::atomic<bool>* cancel) const {
    Result result;
    result.stream = stream;
    if (stream.direct_http()) { result.ok = true; return result; }
    if (!settings.can_resolve(stream)) { result.error = "Connect the stream's service and enable Resolve playable links in Connected Services."; return result; }
    const std::string provider = stream_provider(stream, settings), key = settings.key(provider), hash = stream_hash(stream);
    const std::string magnet = "magnet:?xt=urn:btih:" + hash;
    const auto failed = [&](const HttpResponse& h) { result.error = failure(h, provider); return result; };
    HttpResponse h;
    json data;
    bool cached = false;
    if (provider == "torbox") {
        const std::string body = json{{"hashes", json::array({hash})}}.dump();
        h = request(provider, "v1/api/torrents/checkcached?format=object", key, &body, false, cancel);
        data = payload(h);
        if (!accepted(h, data)) return failed(h);
        const json& item = jobj(jobj(data, "data"), hash.c_str());
        cached = (item.is_boolean() && item.get<bool>()) || (item.is_object() && !item.empty()) || (item.is_array() && !item.empty());
    } else {
        const std::string body = "items%5B%5D=" + url_encode(magnet);
        h = request(provider, "api/cache/check", key, &body, true, cancel); data = payload(h);
        if (!accepted(h, data)) return failed(h);
        const auto& list = jobj(data, "response");
        cached = list.is_array() && !list.empty() && list[0].is_boolean() && list[0].get<bool>();
    }
    if (!cached) { result.error = "This file is not cached on " + provider_name(provider) + ". Choose a cached stream."; return result; }
    json files;
    std::string torrent_id;
    if (provider == "torbox") {
        const std::string body = "magnet=" + url_encode(magnet) + "&add_only_if_cached=true&allow_zip=false";
        h = request(provider, "v1/api/torrents/createtorrent", key, &body, true, cancel); data = payload(h);
        if (!accepted(h, data)) return failed(h);
        const auto& torrent = jobj(data, "data");
        torrent_id = scalar(jobj(torrent, "torrent_id"));
        if (torrent_id.empty()) torrent_id = scalar(jobj(torrent, "id"));
        if (torrent_id.empty()) return failed(h);
        h = request(provider, "v1/api/torrents/mylist?id=" + url_encode(torrent_id) + "&bypass_cache=true", key, nullptr, false, cancel);
        data = payload(h); if (!accepted(h, data)) return failed(h);
        json item = jobj(data, "data");
        if (item.is_array()) {
            json match;
            for (const auto& row : item) if (scalar(jobj(row, "id")) == torrent_id) { match = row; break; }
            item = match;
        }
        files = jobj(item, "files");
    } else {
        const std::string body = "src=" + url_encode(magnet);
        h = request(provider, "api/transfer/directdl", key, &body, true, cancel); data = payload(h);
        if (!accepted(h, data)) return failed(h);
        files = jobj(data, "content");
    }
    const json* file = select_file(files, stream, season, episode);
    if (!file) { result.error = "Could not identify the selected video or episode. Choose another stream."; return result; }
    std::string url = jstr(*file, "link");
    if (provider == "torbox") {
        const std::string file_id = scalar(jobj(*file, "id"));
        if (file_id.empty()) return failed(h);
        h = request(provider, "v1/api/torrents/requestdl?token=" + url_encode(key) + "&torrent_id=" + url_encode(torrent_id) +
                    "&file_id=" + url_encode(file_id) + "&zip_link=false&redirect=false&append_name=false", key, nullptr, false, cancel);
        data = payload(h); if (!accepted(h, data)) return failed(h);
        url = jstr(data, "data");
    }
    if (cancel && cancel->load()) { result.error = "cancelled"; return result; }
    ::Stream resolved = stream;
    resolved.url = trim(url);
    // Provider-only credentials and original addon proxy headers never accompany a CDN request.
    resolved.request_headers.clear();
    if (!resolved.direct_http()) { result.error = "The service did not return a playable link."; return result; }
    resolved.filename = filename(*file);
    resolved.video_size = jnum(*file, "size", jnum(*file, "bytes", stream.video_size));
    result.stream = std::move(resolved); result.ok = true; return result;
}

DeviceCode Client::start_torbox(const std::atomic<bool>* cancel) const {
    DeviceCode code;
    const auto h = request("torbox", "v1/api/user/auth/device/start?app=Nuvio%20Max%27d", "", nullptr, false, cancel);
    const auto data = payload(h); const auto& row = jobj(data, "data");
    if (!accepted(h, data)) { code.error = failure(h, "torbox"); return code; }
    code.device_code = jstr(row, "device_code"); code.user_code = jstr(row, "code");
    code.url = jstr(row, "verification_url"); code.link = jstr(row, "friendly_verification_url", code.url);
    code.interval = std::clamp(int(jnum(row, "interval", 5)), 5, 60);
    code.lifetime = std::clamp(int(jnum(row, "expires_in", 600)), 30, 1800);
    code.ok = !code.device_code.empty() && !code.user_code.empty() && starts_with(code.url, "https://") && starts_with(code.link, "https://");
    if (!code.ok) code.error = "TorBox did not return a complete sign-in code.";
    return code;
}
DeviceToken Client::poll_torbox(const std::string& code, const std::atomic<bool>* cancel) const {
    DeviceToken result;
    const std::string body = json{{"device_code", code}}.dump();
    const auto h = request("torbox", "v1/api/user/auth/device/token", "", &body, false, cancel); const auto data = payload(h);
    result.key = trim(jstr(jobj(data, "data"), "access_token"));
    if (accepted(h, data) && valid_key(result.key)) { result.state = DeviceToken::authorized; return result; }
    result.key.clear();
    const auto message = lower(jstr(data, "error") + " " + jstr(data, "detail"));
    if (h.status == 410 || message.find("expired") != std::string::npos) result.state = DeviceToken::expired;
    else if (h.status == 404 || h.status == 409 || h.status == 425 || message.find("pending") != std::string::npos ||
             message.find("not authorized") != std::string::npos || message.find("not used") != std::string::npos ||
             message.find("not been used") != std::string::npos) result.state = DeviceToken::pending;
    else result.error = failure(h, "torbox");
    return result;
}
} // namespace debrid
