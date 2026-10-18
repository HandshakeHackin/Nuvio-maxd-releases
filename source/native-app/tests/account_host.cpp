// Host harness for the production account adapter and HTTP transport.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "nuvio_account.h"
#include "stremio.h"
#include "http.h"
#include "../gl/model.hpp"
#include <iostream>
#include <thread>
#include <vector>

static json stream_report(const std::vector<::Stream>& streams, bool torrents) {
    json out{{"count", streams.size()}, {"allowed_count", 0}, {"torrents_default", sx::Prefs{}.builtin_torrents},
             {"streams", json::array()}};
    for (const auto& s : streams) {
        out["streams"].push_back(json{{"url", s.url}, {"headers", s.request_headers},
            {"torrent", s.torrent()}, {"direct", s.direct_http()}, {"allowed", s.allowed(torrents)}});
        if (s.allowed(torrents)) out["allowed_count"] = out["allowed_count"].get<int>() + 1;
    }
    if (!streams.empty()) out["headers"] = streams[0].request_headers;
    return out;
}

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    http_init(argv[3]);
    if (!nuvio_account_init(argv[1], argv[2])) return 3;
    std::string line;
    while (std::getline(std::cin, line)) {
        const json cmd = json::parse(line, nullptr, false);
        const std::string op = jstr(cmd, "op");
        json out;
        if (op == "https_update") {
            const auto r = http_get_https(jstr(cmd, "url"));
            out = {{"ok", r.ok()}, {"error", r.error}, {"status", r.status}};
        } else if (op == "provider_request") {
            const auto body = jstr(cmd, "body");
            const auto r = http_provider_request(jstr(cmd, "url"), jbool(cmd, "post") ? &body : nullptr,
                "application/x-www-form-urlencoded", {"Authorization: Bearer provider-test"});
            out = {{"ok", r.ok()}, {"status", r.status}, {"error", r.error}};
        } else if (op == "start") {
            const auto r = link_create();
            out = {{"ok", r.ok}, {"code", r.code}, {"display", r.display_code},
                   {"link", r.link}, {"interval", r.poll_interval}, {"expires_in", r.expires_in}, {"error", r.error}};
        } else if (op == "cancel") { nuvio_pairing_cancel(); out["ok"] = true; }
        else if (op == "restore") out["ok"] = nuvio_account_init(argv[1], argv[2]);
        else if (op == "saved") out["saved"] = nuvio_account_saved();
        else if (op == "context") out = {{"path", nuvio_profile_path()}, {"key", nuvio_profile_key()}};
        else if (op == "parallel") {
            std::vector<std::thread> threads;
            std::atomic<int> count{0};
            for (int i = 0; i < 8; ++i) threads.emplace_back([&] { if (api_get_user("").ok) ++count; });
            for (auto& t : threads) t.join();
            out["count"] = count.load();
        } else if (op == "streams") {
            const auto streams = parse_streams(jobj(cmd, "resource"), "AIOStreams");
            out = stream_report(streams, jbool(cmd, "torrents", sx::Prefs{}.builtin_torrents));
        } else if (op == "addon_url") {
            const std::string url = normalize_addon_url(jstr(cmd, "url"));
            ::Addon addon;
            const bool ok = parse_addon(json{{"id", "configured-addon"}}, url, addon);
            out = {{"ok", ok}, {"manifest", url}, {"resource", addon.resource_url("stream", "movie", "tt123")}};
        } else if (op == "account_streams") {
            const auto account = api_addon_collection("");
            std::vector<::Stream> streams;
            if (account.ok) for (const auto& item : jobj(account.result, "addons")) {
                ::Addon addon;
                if (!parse_addon(jobj(item, "manifest"), jstr(item, "transportUrl"), addon) ||
                    !addon.supports("stream", "movie", "tt123")) continue;
                json resource; std::string error;
                if (!fetch_json(addon.resource_url("stream", "movie", "tt123"), resource, error)) continue;
                const auto found = parse_streams(resource, addon.name);
                streams.insert(streams.end(), found.begin(), found.end());
            }
            out = stream_report(streams, jbool(cmd, "torrents", sx::Prefs{}.builtin_torrents));
            out["ok"] = account.ok; out["error"] = account.error;
        } else if (op == "catalog_manifest") {
            ::Addon addon;
            const bool ok = parse_addon(jobj(cmd, "manifest"), normalize_addon_url(jstr(cmd, "url")), addon);
            out = {{"ok", ok}, {"catalogs", json::array()}};
            for (const auto& c : addon.catalogs)
                out["catalogs"].push_back(json{{"id", c.id}, {"home", c.on_home()},
                    {"url", addon.resource_url("catalog", c.type, c.id)}});
        } else if (op == "account_catalogs") {
            const auto account = api_addon_collection("");
            out = {{"ok", account.ok}, {"error", account.error}, {"rows", json::array()}};
            if (account.ok) for (const auto& item : jobj(account.result, "addons")) {
                ::Addon addon;
                if (!parse_addon(jobj(item, "manifest"), jstr(item, "transportUrl"), addon)) continue;
                for (const auto& c : addon.catalogs) {
                    if (!c.on_home()) continue;
                    json resource; std::string error;
                    const bool ok = fetch_json(addon.resource_url("catalog", c.type, c.id), resource, error);
                    out["rows"].push_back(json{{"id", c.id}, {"ok", ok}, {"error", error},
                        {"metas", jobj(resource, "metas")}});
                }
            }
        } else {
            ApiResult r;
            if (op == "tracking") r = api_tracking(jstr(cmd,"auth_key"),jstr(cmd,"action"),jobj(cmd,"args"));
            else if (op == "watched_get") r = api_watched_get(jstr(cmd, "auth_key"));
            else if (op == "watched_update") r = api_watched_update(jstr(cmd, "auth_key"), jstr(cmd, "video"), jstr(cmd, "title"), jbool(cmd, "watched"), jstr(cmd, "content"), int(jnum(cmd, "season", -1)), int(jnum(cmd, "episode", -1)));
            else if (op == "profiles") r = api_profiles_get(jstr(cmd, "auth_key"));
            else if (op == "select_profile") r = api_profile_select(jstr(cmd, "auth_key"), int(jnum(cmd, "profile")), jstr(cmd, "pin"));
            else if (op == "create_profile") r = api_profile_create(jstr(cmd, "auth_key"), jstr(cmd, "name"));
            else if (op == "source_update") r = api_profile_source_update(jstr(cmd, "auth_key"), jstr(cmd, "url"), jbool(cmd, "plugin"), jbool(cmd, "remove"));
            else if (op == "poll") r = link_read(jstr(cmd, "code"));
            else if (op == "user") r = api_get_user("");
            else if (op == "addons") r = api_addon_collection(jstr(cmd, "auth_key"));
            else if (op == "logout") r = api_logout("");
            else if (op == "library") r = api_library_get(jstr(cmd, "auth_key"));
            else if (op == "connected_services") r = api_connected_services_pull();
            else if (op == "progress_write") r = api_library_put(jstr(cmd, "auth_key"), jobj(cmd, "changes"));
            out = {{"ok", r.ok}, {"result", r.result}, {"error", r.error}};
        }
        std::cout << out.dump() << std::endl;
    }
}
