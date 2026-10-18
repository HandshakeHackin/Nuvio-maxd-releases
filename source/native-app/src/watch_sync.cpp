// Nuvio's sync_pull/push/delete_watch_progress RPC contract, scoped to the selected account profile.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "watch_sync.h"
#include <openssl/rand.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sys/stat.h>

namespace {
using Entries = std::map<std::string, json>;
double timestamp_ms(const json& value) {
    if (value.is_number()) {
        const double n = value.get<double>(); return n > 0 && n < 100000000000.0 ? n * 1000 : n;
    }
    if (!value.is_string()) return 0;
    const auto s = value.get<std::string>();
    int y, month, day, hour, minute, end = 0; double second;
    if (sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%lf%n", &y, &month, &day, &hour, &minute, &second, &end) != 6 ||
        y < 1970 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second >= 60 || size_t(end) >= s.size()) return 0;
    static const int days[]{31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    if (day > days[month - 1] + (month == 2 && leap ? 1 : 0)) return 0;
    int zone = 0;
    if (s[end] == '+' || s[end] == '-') {
        int zh, zm, consumed = 0;
        if (sscanf(s.c_str() + end + 1, "%2d:%2d%n", &zh, &zm, &consumed) != 2 || zh < 0 || zh > 14 || zm < 0 || zm > 59 ||
            (zh == 14 && zm != 0) ||
            size_t(end + 1 + consumed) != s.size()) return 0;
        zone = (zh * 3600 + zm * 60) * (s[end] == '+' ? 1 : -1);
    } else if ((s[end] != 'Z' && s[end] != 'z') || size_t(end + 1) != s.size()) return 0;
    y -= month <= 2; const int era = y / 400, year = y - era * 400;
    const int shifted = month + (month > 2 ? -3 : 9);
    const int doy = (153 * shifted + 2) / 5 + day - 1;
    const int doe = year * 365 + year / 4 - year / 100 + doy;
    return ((double(era) * 146097 + doe - 719468) * 86400 + hour * 3600 + minute * 60 + second - zone) * 1000;
}
std::string key(const json& r) {
    std::string id = jstr(r, "content_id");
    if (r.contains("season") && r["season"].is_number() && r.contains("episode") && r["episode"].is_number())
        id += "_s" + std::to_string(int(jnum(r, "season"))) + "e" + std::to_string(int(jnum(r, "episode")));
    return id;
}
Entries index(const json& rows) {
    Entries out;
    if (rows.is_array()) for (const auto& r : rows) {
        const auto k = key(r);
        if (!k.empty() && (!out.count(k) || jnum(r, "last_watched") > jnum(out[k], "last_watched"))) out[k] = r;
    }
    return out;
}
json array(const Entries& entries) {
    json out = json::array(); for (const auto& entry : entries) out.push_back(entry.second); return out;
}
void retain_metadata(json& remote, const json& local) {
    for (const char* name : {"name", "poster", "background", "logo"})
        if (!jstr(local, name).empty()) remote[name] = local[name];
}
json normalize(const json& row, int profile) {
    const std::string id = jstr(row, "content_id", jstr(row, "contentId"));
    if (id.empty() || jnum(row, "profile_id", jnum(row, "profileId", profile)) != profile) return {};
    double duration = jnum(row, "duration_ms", jnum(row, "durationMs", jnum(row, "duration")));
    double position = jnum(row, "position_ms", jnum(row, "positionMs", jnum(row, "position")));
    const bool explicit_ms = row.contains("duration_ms") || row.contains("durationMs");
    // Legacy RPC snapshots used seconds; current Nuvio writes milliseconds.
    if (!explicit_ms && duration > 0 && duration <= 8 * 60 * 60) { duration *= 1000; position *= 1000; }
    if (duration > 86400000 && duration / 1000 <= 86400000) { duration /= 1000; position /= 1000; }
    if (!std::isfinite(duration) || !std::isfinite(position) || duration < 0 ||
        (duration > 0 && duration < 60000) || duration > 86400000) return {};
    const auto& timestamp = !jobj(row, "updated_at").is_null() ? row["updated_at"] : jobj(row, "last_watched");
    const double updated = timestamp_ms(timestamp);
    if (!std::isfinite(updated) || updated <= 0) return {};
    json out{{"content_id", id}, {"content_type", jstr(row, "content_type", jstr(row, "contentType", "movie"))},
        {"video_id", jstr(row, "video_id", jstr(row, "videoId", id))},
        {"season", nullptr}, {"episode", nullptr}, {"position", int64_t(duration > 0 ? std::clamp(position, 0.0, duration) : std::max(0.0, position))},
        {"duration", int64_t(duration)}, {"last_watched", int64_t(updated)}};
    int season = int(jnum(row, "season", jnum(row, "season_number", -1)));
    int episode = int(jnum(row, "episode", jnum(row, "episode_number", -1)));
    if (season >= 0 && episode > 0) {
        out["season"] = season; out["episode"] = episode;
        if (jstr(out, "video_id").empty() || starts_with(jstr(out, "video_id"), "__nuvio_episode__:"))
            out["video_id"] = id + ":" + std::to_string(season) + ":" + std::to_string(episode);
    }
    out["progress_key"] = key(out);
    return out;
}
json portable(const json& r) {
    json out;
    for (const char* name : {"content_id", "content_type", "video_id", "season", "episode", "position", "duration", "last_watched", "progress_key"})
        out[name] = jobj(r, name);
    return out;
}
}

WatchSync::WatchSync(const std::string& dir, const std::string& owner, int profile) : profile_(profile), path_(dir + "/watch-sync.json"), owner_(owner) {
    load_json(path_, state_);
    if (!state_.is_object() || jstr(state_, "owner") != owner_ || jnum(state_, "profile", 1) != profile_) state_ = json{{"owner", owner_}, {"profile", profile_}};
    for (const char* field : {"entries", "pending", "baseline", "deletes"})
        if (!jobj(state_, field).is_array()) state_[field] = json::array();
    client_ = jstr(state_, "client");
    if (client_.empty()) {
        unsigned char bytes[16];
        if (RAND_bytes(bytes, sizeof bytes) == 1) {
            client_ = "nuvio-ps5-";
            for (unsigned char byte : bytes) { char hex[3]; snprintf(hex, sizeof hex, "%02x", byte); client_ += hex; }
            state_["client"] = client_;
        }
    }
}
bool WatchSync::save() {
    if (!save_json(path_, state_)) return false;
    chmod(path_.c_str(), 0600); return true;
}
bool WatchSync::synced() const { return state_["pending"].empty() && state_["deletes"].empty(); }

void WatchSync::synchronize(const Rpc& rpc, double now) {
    if (now < retry_at_ || owner_.empty() || client_.empty()) return;
    auto call = [&](const char* name, json body, json* result = nullptr) {
        const auto response = rpc(name, body);
        if (!response.ok()) {
            retry_at_ = now + 120;
            dlog("progress: Nuvio Sync unavailable (%s); saved updates will retry", response.describe().c_str());
            return false;
        }
        if (result) *result = json::parse(response.body, nullptr, false);
        return true;
    };
    if (!state_["deletes"].empty()) {
        if (!call("sync_delete_watch_progress", {{"p_profile_id", profile_}, {"p_keys", state_["deletes"]},
            {"p_origin_client_id", client_}})) return;
        state_["deletes"] = json::array(); if (!save()) return;
    }
    json rows;
    if (!call("sync_pull_watch_progress", {{"p_profile_id", profile_}}, &rows)) return;
    if (!rows.is_array()) { retry_at_ = now + 120; return; }
    Entries remote;
    size_t unreadable = 0;
    for (const auto& row : rows) {
        if (jnum(row, "profile_id", jnum(row, "profileId", profile_)) != profile_) continue;
        auto r = normalize(row, profile_); if (r.is_object() && !r.empty()) {
            auto k = key(r); if (!remote.count(k) || jnum(r, "last_watched") > jnum(remote[k], "last_watched")) remote[k] = r;
        } else ++unreadable;
    }
    Entries local = index(state_["entries"]), pending = index(state_["pending"]), baseline = index(state_["baseline"]);
    // A transient empty snapshot must not erase local data or cause an empty push.
    if (!remote.empty()) {
        for (auto it = local.begin(); it != local.end();) {
            if (!unreadable && !remote.count(it->first) && baseline.count(it->first) && !pending.count(it->first)) it = local.erase(it);
            else ++it;
        }
        for (auto& [k, r] : remote) {
            if (pending.count(k) && jnum(pending[k], "last_watched") > jnum(r, "last_watched")) continue;
            if (local.count(k)) retain_metadata(r, local[k]);
            local[k] = r; pending.erase(k);
        }
        if (!unreadable) {
            state_["baseline"] = array(remote);
            baseline = remote;
        }
    }
    state_["entries"] = array(local); state_["pending"] = array(pending);
    if (!save()) return;
    // One unsupported row must not hide the rest of a profile's Continue
    // Watching list. Keep readable rows for display, but do not infer deletions
    // or push a replacement snapshot while any cloud data is unreadable.
    if (unreadable) {
        retry_at_ = now + 120;
        dlog("progress: profile %d loaded %zu readable entries; %zu unreadable entries; cloud replacement deferred",
             profile_, remote.size(), unreadable);
        return;
    }
    if (pending.empty()) return;
    // Nuvio clients push the merged profile snapshot. Preserve other devices'
    // rows even if the server treats the payload as a complete replacement.
    json updates = json::array(); for (const auto& [k, r] : local) updates.push_back(portable(r));
    if (!call("sync_push_watch_progress", {{"p_profile_id", profile_}, {"p_entries", updates},
        {"p_origin_client_id", client_}})) return;
    for (const auto& [k, r] : pending) baseline[k] = r;
    state_["baseline"] = array(baseline); state_["pending"] = json::array(); save();
    dlog("progress: synced %zu updates through Nuvio Sync", pending.size());
}
json WatchSync::pull(const Rpc& rpc, double now) { synchronize(rpc, now); return state_["entries"]; }

bool WatchSync::write(const json& changes, const Rpc& rpc, double now, int64_t updated, std::string& error) {
    Entries entries = index(state_["entries"]), pending = index(state_["pending"]);
    std::set<std::string> deleted;
    for (const auto& k : state_["deletes"]) if (k.is_string()) deleted.insert(k.get<std::string>());
    if (changes.is_array()) for (const auto& item : changes) {
        const auto& s = jobj(item, "state"); const auto id = jstr(item, "_id");
        if (id.empty()) continue;
        if (jbool(s, "nuvioRemoveProgress")) {
            for (auto it = entries.begin(); it != entries.end();) {
                if (jstr(it->second, "content_id") == id) { deleted.insert(it->first); pending.erase(it->first); it = entries.erase(it); }
                else ++it;
            }
            deleted.insert(key(json{{"content_id", id}, {"season", jobj(s, "nuvioSeason")}, {"episode", jobj(s, "nuvioEpisode")}}));
            continue;
        }
        double duration = jnum(s, "duration"), position = jnum(s, "timeOffset");
        if (duration < 60000 || duration > 86400000 || !std::isfinite(position)) continue;
        if (jbool(s, "nuvioCompleted")) position = duration;
        json r{{"content_id", id}, {"content_type", jstr(item, "type", "movie")},
            {"video_id", jstr(s, "video_id", id)}, {"season", jobj(s, "nuvioSeason")}, {"episode", jobj(s, "nuvioEpisode")},
            {"position", int64_t(std::clamp(position, 0.0, duration))}, {"duration", int64_t(duration)}, {"last_watched", updated}};
        r["progress_key"] = key(r); retain_metadata(r, item);
        const auto k = key(r); entries[k] = r; pending[k] = r; deleted.erase(k);
    }
    state_["entries"] = array(entries); state_["pending"] = array(pending);
    state_["deletes"] = json(deleted);
    if (!save()) { error = "Could not save Watch Progress to app storage"; return false; }
    synchronize(rpc, now); return true;
}

void apply_watch_progress(json& library, const json& progress) {
    Entries latest;
    if (progress.is_array()) for (const auto& r : progress) {
        auto id = jstr(r, "content_id");
        if (!latest.count(id) || jnum(r, "last_watched") > jnum(latest[id], "last_watched")) latest[id] = r;
    }
    for (auto& item : library)
        if (jbool(jobj(item, "state"), "nuvioSyncManaged") && !latest.count(jstr(item, "_id"))) {
            item["state"]["timeOffset"] = 0;
            item["state"]["nuvioCompleted"] = false;
        }
    for (const auto& [id, r] : latest) {
        json* item = nullptr;
        for (auto& candidate : library) if (jstr(candidate, "_id") == id) { item = &candidate; break; }
        if (!item) {
            library.push_back({{"_id", id}, {"type", jstr(r, "content_type", "movie")},
                {"name", jstr(r, "name", "Untitled")}, {"poster", jstr(r, "poster")}, {"removed", true}, {"temp", true}});
            item = &library.back();
        }
        const bool completed = jnum(r, "duration") > 0 && jnum(r, "position") >= jnum(r, "duration") * 0.9;
        (*item)["state"] = {{"timeOffset", completed ? 0 : jnum(r, "position")}, {"duration", jnum(r, "duration")},
            {"video_id", jstr(r, "video_id", id)}, {"lastWatchedMs", jnum(r, "last_watched")},
            {"nuvioCompleted", completed}, {"nuvioSyncManaged", true},
            {"nuvioSeason", jobj(r, "season")}, {"nuvioEpisode", jobj(r, "episode")}};
    }
}
