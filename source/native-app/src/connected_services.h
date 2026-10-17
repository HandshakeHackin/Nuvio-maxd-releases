// Connected Services: original cached files from TorBox and Premiumize, plus the
// Anime-Skip client ID used only to look up episode timings.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "stremio.h"
#include "http.h"
#include <functional>

namespace debrid {
std::string provider_id(const std::string& value);
std::string provider_name(const std::string& value);
bool valid_key(const std::string& key);
bool valid_client_id(const std::string& id);

struct Credential { std::string key, sync_owner; };
struct Settings {
    bool enabled = false;
    int instant_limit = 2; // 0 disables background preparation; at most five candidates.
    std::string preferred;
    Credential torbox, premiumize;
    Credential animeskip; // never used to resolve streams
    std::string key(const std::string& provider) const;
    std::string active_provider() const;
    bool can_resolve(const ::Stream& stream) const;
    void clear_synced();
};
bool load(const std::string& path, Settings& settings);
bool save(const std::string& path, const Settings& settings);
// Import is read-only at Nuvio: local disconnects never erase another device's credentials.
bool import_snapshot(Settings& settings, const json& snapshot);
const json* select_file(const json& files, const ::Stream& stream, int season, int episode);

struct Result {
    bool ok = false;
    ::Stream stream;
    std::string error;
};
struct DeviceCode {
    bool ok = false;
    std::string device_code, user_code, url, link, error;
    int interval = 5, lifetime = 600;
};
struct DeviceToken {
    enum State { pending, authorized, expired, failed } state = failed;
    std::string key, error;
};

// Transport injection lets host tests exercise the production provider protocol.
using Transport = std::function<HttpResponse(const std::string&, const std::string*, const std::string&,
                                            const std::vector<std::string>&, const std::atomic<bool>*)>;
class Client {
public:
    explicit Client(Transport transport = http_provider_request) : transport_(std::move(transport)) {}
    std::string validate(const std::string& provider, const std::string& key,
                         const std::atomic<bool>* cancel = nullptr) const;
    Result resolve(const ::Stream& stream, const Settings& settings, int season, int episode,
                   const std::atomic<bool>* cancel = nullptr) const;
    DeviceCode start_torbox(const std::atomic<bool>* cancel = nullptr) const;
    DeviceToken poll_torbox(const std::string& code, const std::atomic<bool>* cancel = nullptr) const;
private:
    HttpResponse request(const std::string& provider, const std::string& path, const std::string& key,
                         const std::string* body, bool form, const std::atomic<bool>* cancel) const;
    Transport transport_;
};
} // namespace debrid
