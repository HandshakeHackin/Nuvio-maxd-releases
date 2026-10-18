// Tracker requests stay on fixed HTTPS origins, with no credential redirects.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tracking.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <mutex>
#include <sys/stat.h>

namespace tracking {
namespace {
std::mutex applications_mutex;
json applications = json::object();
const char* providers[]{"trakt", "simkl", "mdblist"};
json parsed(const HttpResponse& h) { return json::parse(h.body, nullptr, false); }
bool credential(const std::string& value) {
    return !value.empty() && value.size() <= 4096 && std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 33 && c <= 126; });
}
std::string failure(const std::string& p, const HttpResponse& h) {
    // Never echo URL, response body, error buffers, tokens or account data.
    return name(p) + (h.status ? " returned HTTP " + std::to_string(h.status) : " could not be reached") +
        (h.status == 401 ? ". Reconnect this tracker." : h.status == 429 ? ". Rate limit reached; retry later." : ". Try again later.");
}
json ids(const std::string& content) {
    if (starts_with(content, "tt") && content.size() > 2 && std::all_of(content.begin()+2, content.end(), [](unsigned char c){return c>='0' && c<='9';})) return {{"imdb", content}};
    auto parts = split(content, ':');
    if (parts.size() == 2 && (parts[0] == "tmdb" || parts[0] == "tvdb" || parts[0] == "trakt" || parts[0] == "simkl") &&
        !parts[1].empty() && parts[1].size() < 12 && std::all_of(parts[1].begin(), parts[1].end(), [](unsigned char c){return c>='0' && c<='9';}))
        return {{parts[0], std::stoll(parts[1])}};
    return json::object(); // Never identify a different title by fuzzy name alone.
}
std::string content_id(const json& media) {
    const auto& external = jobj(media, "ids");
    auto imdb = jstr(external, "imdb", jstr(media, "imdb_id"));
    if (!ids(imdb).empty()) return imdb;
    for (const char* field : {"tmdb", "tvdb", "trakt", "simkl"}) {
        const auto& v = jobj(external, field);
        auto text = v.is_number_integer() ? std::to_string(v.get<int64_t>()) : v.is_string() ? v.get<std::string>() : "";
        if (!text.empty() && text.size() < 12 && std::all_of(text.begin(), text.end(), [](unsigned char c){return c>='0' && c<='9';})) return std::string(field) + ":" + text;
    }
    return "";
}
void append_media(json& items, const json& media, bool movie) {
    const auto id = content_id(media); if (id.empty()) return;
    for (const auto& item : items) if (jstr(item, "_id") == id) return;
    items.push_back({{"_id", id}, {"name", jstr(media, "title", "Untitled")}, {"type", movie ? "movie" : "series"},
        {"poster", jstr(media, "poster")}, {"removed", false}});
}
void append_playback(json& rows, const json& entry) {
    const bool movie = jobj(entry, "movie").is_object();
    const auto& media = movie ? entry["movie"] : jobj(entry, "show").is_object() ? entry["show"] : jobj(entry, "anime");
    const auto id = content_id(media); if (id.empty()) return;
    const auto& ep = jobj(entry, "episode");
    int season = int(jnum(ep, "season", jnum(ep, "tvdb_season", -1)));
    int episode = int(jnum(ep, "number", jnum(ep, "episode", jnum(ep, "tvdb_number", -1))));
    if (jobj(entry,"anime").is_object() && !jobj(entry,"show").is_object()) {
        season=int(jnum(ep,"tvdb_season",-1)); episode=int(jnum(ep,"tvdb_number",-1));
    }
    if (!movie && (season < 0 || episode < 1)) return;
    double duration = jnum(entry, "runtime", jnum(movie ? media : ep, "runtime", jnum(media, "runtime"))) * 60000;
    double percent = jnum(entry, "progress", -1);
    if (!std::isfinite(duration) || duration < 60000 || duration > 86400000 || !std::isfinite(percent) || percent < 0 || percent > 100) return;
    auto stamp = iso8601_to_ms(jstr(entry, "paused_at", jstr(entry, "updated_at")));
    if (!stamp) stamp = int64_t(jnum(entry, "updated_at_ts") * 1000);
    // Unknown timestamps must not override an unsent local update.
    rows.push_back({{"content_id", id}, {"content_type", movie ? "movie" : "series"},
        {"video_id", movie ? id : id+":"+std::to_string(season)+":"+std::to_string(episode)},
        {"season", movie ? json(nullptr) : json(season)}, {"episode", movie ? json(nullptr) : json(episode)},
        {"duration", duration}, {"position", duration * percent / 100.0}, {"last_watched", stamp},
        {"name", jstr(media, "title", id)}, {"poster", jstr(media, "poster")}, {"playback_id", jobj(entry, "id")}});
}
std::string progress_key(const json& item) {
    const auto& s = jobj(item, "state");
    return std::string(s.contains("trackingWatched") ? "history:" : "progress:") + jstr(item, "_id") + ":" + std::to_string(int(jnum(s, "nuvioSeason", -1))) + ":" + std::to_string(int(jnum(s, "nuvioEpisode", -1)));
}
json scrobble(const std::string& p, const json& item) {
    auto external = ids(jstr(item, "_id")); if (external.empty()) return {};
    const auto& state = jobj(item, "state");
    bool movie = jstr(item, "type") == "movie";
    json media{{"ids", external}};
    json result{{"progress", jbool(state, "nuvioCompleted") ? 100.0 : std::clamp(jnum(state, "timeOffset") / std::max(1.0, jnum(state, "duration")) * 100, 0.0, 100.0)}};
    if (movie) result["movie"] = media;
    else {
        int season = int(jnum(state, "nuvioSeason", -1)), episode = int(jnum(state, "nuvioEpisode", -1));
        if (season < 0 || episode < 1) return {};
        if (p == "mdblist") { media["season"] = season; media["episode"] = episode; }
        else result["episode"] = {{"season", season}, {"number", episode}};
        result["show"] = media;
    }
    if (p == "mdblist") result["app_name"] = "Nuvio Max'd";
    return result;
}
}
void set_applications(const json& configured) {
    std::lock_guard<std::mutex> guard(applications_mutex);
    applications = configured.is_object() ? configured : json::object();
}
std::string name(const std::string& p) { return p == "trakt" ? "Trakt" : p == "simkl" ? "SIMKL" : p == "mdblist" ? "MDBList" : "Nuvio"; }
bool provider(const std::string& p) { return p == "trakt" || p == "simkl" || p == "mdblist"; }
Store::Store(std::string directory, Transport transport, const std::atomic<bool>* cancel) : dir_(std::move(directory)), path_(dir_ + "/tracking.json"), cancel_(cancel), transport_(std::move(transport)) {
    if (!transport_) transport_ = [cancel](const auto& url, const auto* body, const auto& headers) {
        return http_provider_request(url, body, "application/json", headers, cancel);
    };
    if (file_exists(path_)) readable_ = load_json(path_, state_) && state_.is_object() && jnum(state_, "version") == 1;
    if (!readable_) return;
    if (!state_.is_object()) state_ = {{"version", 1}, {"library", "nuvio"}, {"progress", "nuvio"}};
    if (!jobj(state_, "accounts").is_object()) state_["accounts"] = json::object();
    {
        std::lock_guard<std::mutex> guard(applications_mutex);
        for (auto p : {"trakt","simkl"}) {
            auto& account=state_["accounts"][p];
            if (account.is_null()) account=json::object();
            const auto& app=jobj(applications,p);
            if (account.is_object() && jstr(account,"client_id").empty() && credential(jstr(app,"client_id")) &&
                (std::string(p)!="trakt" || credential(jstr(app,"client_secret")))) {
                account["client_id"]=app["client_id"];
                if(std::string(p)=="trakt") account["client_secret"]=app["client_secret"];
            }
        }
    }
    for (auto p : providers) {
        if (state_["accounts"].contains(p) && !state_["accounts"][p].is_object()) readable_ = false;
        if (state_.contains("pending_"+std::string(p)) && !state_["pending_"+std::string(p)].is_array()) readable_ = false;
    }
}
bool Store::save(std::string& error) {
    if (cancel_ && *cancel_) { error="Tracker connection cancelled"; return false; }
    if (!readable_) { error = "Tracker settings are unreadable; existing connections were not overwritten."; return false; }
    // Restrict the temporary file before any credential bytes are written.
    const auto tmp = path_ + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { error = "Could not save tracker settings"; return false; }
    if (fchmod(fileno(f), 0600) != 0) { fclose(f); remove(tmp.c_str()); error = "Could not protect tracker settings"; return false; }
    auto data = state_.dump(); bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = fclose(f) == 0 && ok;
    if (!ok || rename(tmp.c_str(), path_.c_str()) != 0) { remove(tmp.c_str()); error = "Could not save tracker settings"; return false; }
    return true;
}
bool Store::connected(const std::string& p) const {
    const auto& account = jobj(jobj(state_, "accounts"), p.c_str());
    return provider(p) && jbool(account, "verified") && !jbool(account, "reconnect") &&
        credential(jstr(account, p == "mdblist" ? "api_key" : "access_token"));
}
std::string Store::source(bool progress) const {
    auto selected = jstr(state_, progress ? "progress" : "library", "nuvio");
    return connected(selected) ? selected : "nuvio";
}
json Store::status() const {
    json out{{"library", jstr(state_,"library_error").empty() ? source(false) : "nuvio"}, {"progress", jstr(state_,"progress_error").empty() ? source(true) : "nuvio"}, {"libraryPreferred", jstr(state_, "library", "nuvio")},
        {"progressPreferred", jstr(state_, "progress", "nuvio")}, {"accounts", json::object()}, {"readable", readable_}};
    for (auto p : providers) out["accounts"][p] = {{"connected", connected(p)}, {"reconnect", jbool(jobj(jobj(state_, "accounts"), p), "reconnect")},
        {"configured", credential(jstr(jobj(jobj(state_, "accounts"), p), "client_id"))}, {"error", jstr(jobj(jobj(state_, "accounts"), p), "error")}};
    return out;
}
HttpResponse Store::request(const std::string& p, const std::string& path, const json* body, bool auth) {
    if ((cancel_ && *cancel_) || !provider(p) || path.empty() || path[0] != '/' || path.find_first_of("\r\n") != std::string::npos) return {};
    const auto& account = state_["accounts"][p];
    std::string base = p == "trakt" ? "https://api.trakt.tv" : p == "simkl" ? "https://api.simkl.com" : "https://api.mdblist.com";
    std::string url = base + path;
    std::vector<std::string> headers{"Accept: application/json"};
    if (p == "trakt") { headers.push_back("trakt-api-version: 2"); headers.push_back("trakt-api-key: " + jstr(account, "client_id")); }
    if (p == "simkl") headers.push_back("simkl-api-key: " + jstr(account, "client_id"));
    if (auth) {
        if (p == "mdblist") url += (url.find('?') == std::string::npos ? "?apikey=" : "&apikey=") + url_encode(jstr(account, "api_key"));
        else headers.push_back("Authorization: Bearer " + jstr(account, "access_token"));
    }
    std::string encoded = body ? body->dump() : "";
    return transport_(url, body ? &encoded : nullptr, headers);
}
bool Store::refresh(const std::string& p, std::string& error) {
    if (p != "trakt") return true;
    auto& a = state_["accounts"][p];
    const double expires = jnum(a, "expires_at");
    if (expires <= 0 || expires > double(now_epoch_ms()) / 1000 + 120) return true;
    json body{{"refresh_token", jstr(a, "refresh_token")}, {"client_id", jstr(a, "client_id")}, {"client_secret", jstr(a, "client_secret")},
        {"redirect_uri", "urn:ietf:wg:oauth:2.0:oob"}, {"grant_type", "refresh_token"}};
    auto response = request(p, "/oauth/token", &body, false); auto token = parsed(response);
    if (!response.ok() || !credential(jstr(token, "access_token")) || !credential(jstr(token, "refresh_token"))) {
        if (response.status == 400 || response.status == 401) a["reconnect"] = true;
        error = failure(p, response); a["error"] = error; std::string ignored; save(ignored); return false;
    }
    a["access_token"] = token["access_token"]; a["refresh_token"] = token["refresh_token"];
    a["expires_at"] = jnum(token, "created_at", double(now_epoch_ms()) / 1000) + jnum(token, "expires_in");
    return save(error);
}
bool Store::fetch(const std::string& p, const std::string& path, json& out, std::string& error) {
    if (!connected(p)) { error = "Connect " + name(p) + " first"; return false; }
    if (!refresh(p, error)) return false;
    if (p=="simkl" && starts_with(path,"/sync/")) {
        const double checked=jnum(state_,"simkl_checked"), now=now_seconds();
        if (checked<=0 || now<checked || now-checked>=30) {
            const json empty=json::object();
            auto activities=request(p,"/sync/activities",&empty);
            auto snapshot=parsed(activities);
            if (!activities.ok() || !snapshot.is_object() || snapshot.empty()) {
                error=failure(p,activities);
                if(activities.status==401) state_["accounts"][p]["reconnect"]=true;
                state_["accounts"][p]["error"]=error;
                std::string ignored;save(ignored);return false;
            }
            if(jobj(state_,"simkl_activities")!=snapshot) state_["simkl_cache"]=json::object();
            state_["simkl_activities"]=snapshot;state_["simkl_checked"]=now;
        }
        if (jobj(state_,"simkl_cache").is_object() && state_["simkl_cache"].contains(path)) {
            out=state_["simkl_cache"][path];return true;
        }
    }
    auto response = request(p, path); out = parsed(response);
    if (!response.ok() || out.is_discarded()) {
        error = failure(p, response); auto& a = state_["accounts"][p]; a["error"] = error;
        if (response.status == 401) a["reconnect"] = true;
        std::string ignored; save(ignored); return false;
    }
    state_["accounts"][p]["error"] = "";
    if(p=="simkl" && starts_with(path,"/sync/")) {
        state_["simkl_cache"][path]=out;std::string ignored;save(ignored);
    }
    return true;
}
bool Store::validate(const std::string& p, std::string& error) {
    const json empty = json::object();
    const auto response = request(p, p == "mdblist" ? "/user" : "/users/settings", p == "simkl" ? &empty : nullptr);
    const auto user = parsed(response);
    if (!response.ok() || !user.is_object() || (p == "mdblist" ? jnum(user, "user_id") <= 0 : !jobj(user, "user").is_object())) { error = failure(p, response); return false; }
    return true;
}
json Store::action(const std::string& operation, const json& args, std::string& error) {
    if (!readable_) { error = "Tracker settings are unreadable; connections were not overwritten."; return {}; }
    const auto p = jstr(args, "provider");
    if (operation == "status") return status();
    if (operation == "source") {
        const auto field = jstr(args, "field"), value = jstr(args, "value");
        if ((field != "library" && field != "progress") || (value != "nuvio" && !connected(value))) { error = "Connect the tracker before choosing it as a source"; return {}; }
        state_[field] = value; state_[field+"_error"] = "";
    } else {
        if (!provider(p)) { error = "Unknown tracker"; return {}; }
        auto& a = state_["accounts"][p];
        if (operation == "device_cancel") {
            state_.erase("pairing_"+p);
        } else if (operation == "configure") {
            if (p == "mdblist" || !credential(jstr(args, "client_id")) || (p == "trakt" && !credential(jstr(args, "client_secret")))) { error = "Enter the application's tracker credentials"; return {}; }
            if (connected(p)) { error = "Disconnect before changing application credentials"; return {}; }
            a = {{"client_id", jstr(args, "client_id")}, {"client_secret", jstr(args, "client_secret")}};
        } else if (operation == "disconnect") {
            // Local disconnect does not revoke credentials shared with another Nuvio device.
            a = json::object();
            remove((dir_+"/native-library-"+p+".json").c_str());
            remove((dir_+"/progress-"+p+".json").c_str());
            for (const auto* field : {"library", "progress"}) if (jstr(state_, field) == p) state_[field] = "nuvio";
            if(p=="simkl") {state_.erase("simkl_cache");state_.erase("simkl_activities");state_.erase("simkl_checked");}
            state_.erase(("pending_"+p)); state_.erase("failed_"+p); state_.erase("cache_"+p); state_.erase("pairing_"+p);
        } else if (operation == "key") {
            if (connected(p)) { error = "Disconnect the current tracker before replacing its account"; return {}; }
            if (p != "mdblist" || !credential(jstr(args, "api_key"))) { error = "Enter a valid MDBList API key"; return {}; }
            auto previous = a; a = {{"api_key", jstr(args, "api_key")}};
            if (!validate(p, error)) { a = previous; return {}; }
            a["verified"] = true;
        } else if (operation == "import") {
            if (connected(p)) { error = "Disconnect the current tracker before importing another account"; return {}; }
            const auto& imported = jobj(args, "credential");
            auto previous = a;
            if (!imported.is_object() || !credential(jstr(imported, p == "mdblist" ? "api_key" : "access_token")) ||
                (p != "mdblist" && !credential(jstr(imported, "client_id")))) { error = "No compatible synced " + name(p) + " connection found"; return {}; }
            a = json::object();
            for (const auto* field : {"api_key", "access_token", "refresh_token", "client_id", "client_secret"})
                if (credential(jstr(imported, field))) a[field] = imported[field];
            a["expires_at"] = jnum(imported, "expires_at");
            if (!refresh(p, error) || !validate(p, error)) { a = previous; return {}; }
            a["verified"] = true;
        } else if (operation == "device_start") {
            if (connected(p)) { error="Disconnect the current tracker before connecting another account"; return {}; }
            if (p == "mdblist" || !credential(jstr(a, "client_id")) || (p == "trakt" && !credential(jstr(a, "client_secret")))) { error = "Set up the application's tracker credentials first"; return {}; }
            json body{{"client_id", jstr(a, "client_id")}};
            auto response = p == "trakt" ? request(p, "/oauth/device/code", &body, false) : request(p, "/oauth/pin?client_id="+url_encode(jstr(a, "client_id")), nullptr, false);
            auto code = parsed(response);
            if (!response.ok() || !credential(jstr(code, "user_code")) || jnum(code, "expires_in") <= 0 || (p == "trakt" && !credential(jstr(code, "device_code")))) { error = failure(p, response); return {}; }
            state_["pairing_"+p] = {{"code", p == "trakt" ? jstr(code, "device_code") : jstr(code, "user_code")},
                {"expires", now_seconds()+std::clamp(jnum(code, "expires_in"), 1.0, 3600.0)}};
            if (!save(error)) return {};
            // Fixed vendor verification origins prevent QR substitution.
            return {{"code", jstr(code, "user_code")}, {"url", p == "trakt" ? "https://trakt.tv/activate" : "https://simkl.com/pin"},
                {"interval", std::clamp(jnum(code, "interval", 5), 2.0, 60.0)}, {"expires_in", std::clamp(jnum(code, "expires_in"), 1.0, 3600.0)}};
        } else if (operation == "device_poll") {
            const auto pair = jobj(state_, ("pairing_"+p).c_str());
            if (jstr(pair, "code").empty() || jnum(pair, "expires") <= now_seconds()) { error = "Code expired; connect again"; return {}; }
            json body{{"code", jstr(pair, "code")}, {"client_id", jstr(a, "client_id")}, {"client_secret", jstr(a, "client_secret")}};
            auto response = p == "trakt" ? request(p, "/oauth/device/token", &body, false) : request(p, "/oauth/pin/"+url_encode(jstr(pair, "code"))+"?client_id="+url_encode(jstr(a, "client_id")), nullptr, false);
            auto token = parsed(response);
            if ((p == "trakt" && (response.status == 400 || response.status == 429)) || (p == "simkl" && jstr(token, "result") == "KO"))
                return {{"pending", true}, {"slow", response.status == 429}};
            if (!response.ok() || !credential(jstr(token, "access_token")) || (p=="trakt" && !credential(jstr(token,"refresh_token")))) { error = failure(p, response); return {}; }
            auto previous = a; a["access_token"] = token["access_token"];
            if (p == "trakt") {
                a["refresh_token"] = jstr(token, "refresh_token");
                a["expires_at"] = jnum(token, "created_at", double(now_epoch_ms())/1000)+jnum(token, "expires_in");
            }
            if (!validate(p, error)) { a = previous; return {}; }
            a["verified"] = true; a["reconnect"] = false; a["error"] = ""; state_.erase("pairing_"+p);
            state_.erase("pending_"+p); state_.erase("failed_"+p);
            if(p=="simkl") {state_.erase("simkl_cache");state_.erase("simkl_activities");state_.erase("simkl_checked");}
            remove((dir_+"/native-library-"+p+".json").c_str());
            remove((dir_+"/progress-"+p+".json").c_str());
        } else { error = "Unknown tracker action"; return {}; }
    }
    if (!save(error)) return {};
    return status();
}
bool Store::library(json& items, std::string& error) {
    error.clear(); const bool ok=read_library(items,error);
    if (source(false)!="nuvio" || !error.empty()) {
        state_["library_error"] = ok ? "" : error;
        std::string ignored; save(ignored);
    }
    return ok;
}
bool Store::progress(json& rows, std::string& error) {
    error.clear(); const bool ok=read_progress(rows,error);
    if (source(true)!="nuvio" || !error.empty()) {
        state_["progress_error"] = ok ? "" : error;
        std::string ignored; save(ignored);
    }
    return ok;
}
bool Store::read_library(json& items, std::string& error) {
    const auto p = source(false); if (p == "nuvio") return false;
    json result = json::array(), response;
    if (p == "trakt") {
        for (int page = 1; page <= 100; ++page) {
            if (!fetch(p, "/sync/watchlist?extended=full&limit=100&page="+std::to_string(page), response, error) || !response.is_array()) {
                if (error.empty()) error = "Trakt returned an unreadable watchlist";
                return false;
            }
            for (const auto& row : response) {
                bool movie = jobj(row, "movie").is_object();
                append_media(result, movie ? row["movie"] : jobj(row, "show"), movie);
            }
            if (response.size() < 100) { items = result; return true; }
        }
        error = "Trakt watchlist exceeds the 10,000 item limit"; return false;
    } else if (p == "simkl") {
        if (!fetch(p, "/sync/all-items?extended=full", response, error) || !response.is_object() || (!response.contains("movies") && !response.contains("shows") && !response.contains("anime"))) {
            if (error.empty()) error = "SIMKL returned an unreadable library";
            return false;
        }
        for (auto type : {"movies", "shows", "anime"}) if (jobj(response, type).is_array()) for (const auto& row : response[type]) {
            const auto& media = jobj(row, type == std::string("movies") ? "movie" : type == std::string("shows") ? "show" : "anime");
            append_media(result, media, type == std::string("movies") || jstr(media, "anime_type") == "movie");
        }
    } else {
        std::string cursor; std::set<std::string> cursors;
        for (int page = 0; page < 100; ++page) {
            if (!fetch(p, "/watchlist/items?limit=100&append_to_response=poster"+(cursor.empty() ? "" : "&cursor="+url_encode(cursor)), response, error) || !response.is_object() || (!response.contains("movies") && !response.contains("shows"))) {
                if (error.empty()) error = "MDBList returned an unreadable watchlist";
                return false;
            }
            for (auto type : {"movies", "shows"}) if (jobj(response, type).is_array()) for (auto row : response[type]) {
                if (!jobj(row, "ids").is_object()) row["ids"] = json::object();
                if (!jstr(row, "imdb_id").empty()) row["ids"]["imdb"] = row["imdb_id"];
                else if (jobj(row, "id").is_number_integer()) row["ids"]["tmdb"] = row["id"];
                append_media(result, row, std::string(type) == "movies");
            }
            cursor = jstr(jobj(response, "pagination"), "next_cursor", jstr(response, "next_cursor"));
            if (cursor.empty()) { items = result; return true; }
            if (!cursors.insert(cursor).second) break;
        }
        error = "MDBList returned an incomplete paginated watchlist"; return false;
    }
    items = result; return true;
}
bool Store::read_progress(json& rows, std::string& error) {
    const auto p = source(true); if (p == "nuvio") return false;
    if (!flush(p, error)) return false;
    json response, result = json::array();
    for (int page=1;page<=100;++page) {
        const auto path = p == "trakt" ? "/sync/playback?extended=full&limit=100&page="+std::to_string(page) : "/sync/playback";
        if (!fetch(p, path, response, error) || !response.is_array()) {
            if (error.empty()) error = name(p) + " returned unreadable playback progress";
            return false;
        }
        for (const auto& row : response) append_playback(result, row);
        if (p != "trakt" || response.size()<100) break;
        if (page==100) { error="Trakt playback snapshot exceeds the 10,000 item limit"; return false; }
    }
    rows = result; return true;
}
bool Store::flush(const std::string& p, std::string& error) {
    const auto field = "pending_"+p;
    if (!jobj(state_, field.c_str()).is_array() || state_[field].empty()) return true;
    if (!connected(p) || !refresh(p, error)) return false;
    // Keep failed events bound to the exact tracker account they were created for.
    // Replacing/disconnecting a connection clears them; do not migrate across accounts.
    bool rejected=false;
    while (!state_[field].empty()) {
        const auto item = state_[field].front();
        const auto& s = jobj(item, "state");
        auto body = scrobble(p, item);
        if (!body.is_object() || body.empty()) { error = name(p)+" cannot identify this title or episode; progress stays on this PS5"; return false; }
        std::string path = "/scrobble/" + std::string(jbool(s, "nuvioCompleted") || jbool(s, "trackingFinal") ? "stop" : "pause");
        if (s.contains("trackingWatched")) {
            auto media = json{{"ids", ids(jstr(item, "_id"))}};
            const bool movie = jstr(item, "type") == "movie";
            if (!movie) media["seasons"] = json::array({{{"number", int(jnum(s,"nuvioSeason"))},
                {"episodes", json::array({{{"number", int(jnum(s,"nuvioEpisode"))}}})}}});
            body = {{movie ? "movies" : "shows", json::array({media})}};
            path = p == "mdblist" ? "/sync/watched" : "/sync/history";
            if (!jbool(s,"trackingWatched")) path += "/remove";
        } else if (jbool(s, "nuvioRemoveProgress")) {
            // Only MDBList supports a POST clear operation. Never misrepresent
            // removing a resume row as marking a film watched.
            if (p != "mdblist") { error = "Removing Continue Watching from "+name(p)+" is not supported here yet"; return false; }
            path = "/scrobble/clear";
        }
        auto response = request(p, path, &body);
        const auto reply=parsed(response);
        const auto& not_found=jobj(reply,"not_found");
        bool missing_item=not_found.is_number() && not_found.get<double>()>0;
        if(not_found.is_object()) for(const auto& value:not_found) if(!value.empty()) missing_item=true;
        if(response.status==400 || response.status==404 || response.status==422 || (response.ok() && missing_item)) {
            const auto failed="failed_"+p;
            if(!jobj(state_,failed.c_str()).is_array()) state_[failed]=json::array();
            auto& rejected_items=state_[failed];
            rejected_items.erase(std::remove_if(rejected_items.begin(),rejected_items.end(),[&](const auto& old){return progress_key(old)==progress_key(item);}),rejected_items.end());
            rejected_items.push_back(item);state_[field].erase(state_[field].begin());rejected=true;
            if(!save(error)) return false;
            continue; // A permanently rejected title must not block other titles.
        }
        // A duplicate completed scrobble is already represented in watch history.
        if (!response.ok() && !((p == "trakt" || p == "simkl") && path == "/scrobble/stop" && response.status == 409)) {
            error = failure(p, response); state_["accounts"][p]["error"] = error;
            if (response.status == 401) state_["accounts"][p]["reconnect"] = true;
            std::string ignored; save(ignored);
            return false;
        }
        if(p=="simkl") {state_.erase("simkl_cache");state_.erase("simkl_checked");}
        state_[field].erase(state_[field].begin());
        if (!save(error)) return false;
    }
    if(rejected) {
        error=name(p)+" could not match a title or episode. Its progress stays on this PS5; other titles can still sync.";
        state_["accounts"][p]["error"]=error;std::string ignored;save(ignored);return false;
    }
    state_["accounts"][p]["error"] = ""; return save(error);
}
bool Store::write(const json& items, std::string& error) {
    const auto p = jstr(state_, "progress", "nuvio");
    if (!provider(p)) return true;
    const auto field = "pending_"+p;
    if (!jobj(state_, field.c_str()).is_array()) state_[field] = json::array();
    if (items.is_array()) for (const auto& item : items) {
        const auto& progress=jobj(item,"state");
        if (!progress.contains("trackingWatched") && !jbool(progress,"nuvioRemoveProgress") &&
            (!std::isfinite(jnum(progress,"duration")) || jnum(progress,"duration") < 60000 || jnum(progress,"duration") > 86400000 ||
             !std::isfinite(jnum(progress,"timeOffset")) || jnum(progress,"timeOffset") < 0)) {
            error="Invalid playback progress; tracker history was not changed"; return false;
        }
        auto body = scrobble(p, item);
        if (!body.empty() && p=="trakt" && !progress.contains("trackingWatched") && !jbool(progress,"nuvioRemoveProgress") &&
            !jbool(progress,"nuvioCompleted") && jnum(body,"progress")<1) continue;
        if (body.empty()) { error = name(p)+" cannot identify this title or episode; progress stays on this PS5"; return false; }
        if (jbool(jobj(item,"state"),"nuvioRemoveProgress") && p != "mdblist") {
            error = "Remove this playback entry on "+name(p)+"; it has not been marked watched";
            return false;
        }
        if(jobj(state_,("failed_"+p).c_str()).is_array()) {
            auto& failed=state_["failed_"+p];
            failed.erase(std::remove_if(failed.begin(),failed.end(),[&](const auto& old){return progress_key(old)==progress_key(item);}),failed.end());
        }
        auto& queue = state_[field];
        queue.erase(std::remove_if(queue.begin(), queue.end(), [&](const auto& old) { return progress_key(old) == progress_key(item); }), queue.end());
        // Save only portable metadata, never stream links/headers or the full native item.
        const auto& s = jobj(item, "state");
        json portable{{"_id", jstr(item, "_id")}, {"type", jstr(item, "type")}, {"state", json::object()}};
        for (auto key : {"timeOffset", "duration", "nuvioCompleted", "trackingFinal", "nuvioSeason", "nuvioEpisode", "nuvioRemoveProgress", "trackingWatched"})
            if (s.contains(key)) portable["state"][key] = s[key];
        queue.push_back(portable);
    }
    if (!save(error)) return false;
    if (!connected(p)) { error = "Reconnect "+name(p)+"; progress is saved on this PS5"; return false; }
    return flush(p, error);
}
bool Store::unsent(const json& item) const {
    const auto p=jstr(state_,"progress","nuvio");
    if (!provider(p)) return false;
    if (p=="trakt" && jnum(scrobble(p,item),"progress")<1 && !jbool(jobj(item,"state"),"nuvioCompleted")) return true;
    if (scrobble(p,item).empty()) return true; // Unsupported ID: retain local-only resume.
    for (const auto* field : {"pending_","failed_"}) {
        const auto& pending=jobj(state_,(field+p).c_str());
        if (pending.is_array()) for (const auto& event : pending)
            if (progress_key(event)==progress_key(item)) return true;
    }
    return false;
}
bool Store::mark(const std::string& content, int season, int episode, bool watched, std::string& error) {
    json state{{"nuvioSeason",season}, {"nuvioEpisode",episode}, {"trackingWatched",watched}, {"duration",60000}, {"timeOffset",0}};
    return write(json::array({{{"_id",content}, {"type",episode > 0 ? "series" : "movie"}, {"state",state}}}), error);
}
bool Store::watched(json& rows, std::string& error) {
    const auto p = source(true); if (p == "nuvio") return false;
    if (!flush(p,error)) return false;
    json result = json::array(), data;
    const auto append = [&](const json& media, int season, int episode) {
        const auto id = content_id(media); if (id.empty()) return;
        json row{{"content_id",id}, {"season",nullptr}, {"episode",nullptr}};
        if (season >= 0 && episode > 0) { row["season"] = season; row["episode"] = episode; }
        result.push_back(row);
    };
    const auto show = [&](const json& row, const json& media, bool simkl) {
        if (jobj(row,"seasons").is_array()) for (const auto& season : row["seasons"])
            if (jobj(season,"episodes").is_array()) for (const auto& ep : season["episodes"])
                if (!simkl || !jstr(ep,"watched_at").empty()) append(media,int(jnum(season,"number",-1)),int(jnum(ep,"number",-1)));
    };
    if (p == "trakt") {
        for (const auto* type : {"movies", "shows"}) {
            for (int page=1;page<=100;++page) {
                if (!fetch(p,"/sync/watched/"+std::string(type)+"?limit=100&page="+std::to_string(page),data,error) || !data.is_array()) return false;
                for (const auto& row : data) {
                    if (std::string(type)=="movies") append(jobj(row,"movie"),-1,-1);
                    else show(row,jobj(row,"show"),false);
                }
                if (data.size()<100) break;
                if (page==100) { error="Trakt watched history exceeds the snapshot limit"; return false; }
            }
        }
    } else if (p == "simkl") {
        if (!fetch(p,"/sync/all-items?extended=full",data,error) || !data.is_object()) return false;
        for (auto type : {"movies","shows","anime"}) if (jobj(data,type).is_array()) for (const auto& row : data[type]) {
            if (std::string(type)=="movies") {
                if (jstr(row,"status")=="completed" || !jstr(row,"last_watched_at").empty()) append(jobj(row,"movie"),-1,-1);
            } else show(row,jobj(row,std::string(type)=="shows" ? "show" : "anime"),true);
        }
    } else {
        std::string cursor; std::set<std::string> cursors;
        for (int page=0;page<100;++page) {
            if (!fetch(p,"/sync/watched?limit=100"+(cursor.empty()?"":"&cursor="+url_encode(cursor)),data,error) || !data.is_object()) return false;
            if (jobj(data,"movies").is_array()) for (const auto& row : data["movies"]) append(jobj(row,"movie"),-1,-1);
            if (jobj(data,"shows").is_array()) for (const auto& row : data["shows"]) show(row,jobj(row,"show"),false);
            if (jobj(data,"episodes").is_array()) for (const auto& row : data["episodes"]) {
                const auto& ep = jobj(row,"episode");
                append(jobj(row,"show"),int(jnum(ep,"season",-1)),int(jnum(ep,"number",jnum(ep,"episode",-1))));
            }
            cursor = jstr(jobj(data,"pagination"),"next_cursor");
            if (cursor.empty()) break;
            if (!cursors.insert(cursor).second || page==99) { error="MDBList history pagination did not finish"; return false; }
        }
    }
    rows=result; return true;
}

}
