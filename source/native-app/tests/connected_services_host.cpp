// Provider protocol, credential storage and exact file selection tests.
#include "connected_services.h"
#include "../gl/stream_json.hpp"
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace {
int tests = 0;
void expect(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
HttpResponse response(json data, long status = 200) { HttpResponse h; h.status = status; h.body = data.dump(); return h; }
const std::string hash(40, 'a');
::Stream stream(const std::string& provider = "torbox") {
    ::Stream s; s.name = "4K HDR10+ HEVC Atmos"; s.addon = "AIOStreams";
    s.request_headers = {"Authorization: Bearer addon-only"};
    s.client_resolve = {{"type", "debrid"}, {"service", provider}, {"infoHash", hash}, {"isCached", true}};
    return s;
}
debrid::Settings settings() { debrid::Settings s; s.enabled = true; s.torbox.key = "torbox-test"; s.premiumize.key = "premiumize-test"; return s; }
void check(const char* name, const std::function<void()>& test) {
    test(); ++tests; std::cout << "PASS " << name << '\n';
}
} // namespace

int main() {
try {
    check("clientResolve survives parsing and resume without accepting injected credentials", [] {
        json resource{{"streams", json::array({json{{"name", "Instant 4K"}, {"clientResolve", stream().client_resolve}}})}};
        resource["streams"][0]["clientResolve"]["apiKey"] = "must-not-persist";
        const auto list = parse_streams(resource, "AIOStreams");
        expect(list.size() == 1 && !list[0].torrent() && !list[0].allowed(true), "client result dropped or treated as P2P");
        const auto restored = stream_from_json(stream_to_json(list[0]));
        expect(restored.client_resolve == stream().client_resolve, "resume instructions changed");
        expect(stream_to_json(list[0]).dump().find("must-not-persist") == std::string::npos, "injected key persisted");
    });
    check("raw torrents resolve through the preferred service with P2P off", [] {
        ::Stream s; s.info_hash = hash;
        auto cfg = settings(); cfg.preferred = "premiumize";
        expect(!s.allowed(false) && cfg.can_resolve(s), "cached torrent needs local P2P");
        cfg.preferred = "torbox"; cfg.torbox.key.clear();
        expect(!cfg.can_resolve(s), "missing preferred key silently uses another provider");
    });
    check("provider-specific instructions respect Resolve with", [] {
        auto cfg = settings(); cfg.preferred = "premiumize";
        expect(!cfg.can_resolve(stream("torbox")) && cfg.can_resolve(stream("PM")), "service preference ignored");
        auto s = stream(); s.client_resolve["isCached"] = false; cfg.preferred.clear();
        expect(!cfg.can_resolve(s), "uncached instruction accepted");
        s.client_resolve["service"] = "realdebrid"; expect(!cfg.can_resolve(s), "unsupported service accepted");
        cfg.enabled = false; expect(!cfg.can_resolve(stream()), "disabled resolver accepted");
    });
    check("direct URLs remain unchanged without contacting providers", [] {
        debrid::Client client([](auto&, auto*, auto&, auto&, auto*) -> HttpResponse { throw std::runtime_error("unnecessary request"); });
        auto s = stream(); s.url = "https://cdn.invalid/direct-original.mkv";
        const auto r = client.resolve(s, settings(), -1, -1);
        expect(r.ok && r.stream.url == s.url && r.stream.request_headers == s.request_headers, "direct addon link changed");
    });
    check("TorBox resolves the exact original file with cached-only creation", [] {
        int calls = 0;
        debrid::Client client([&](const auto& url, const auto* body, const auto& type, const auto& headers, const auto*) {
            expect(headers.back() == "Authorization: Bearer torbox-test", "wrong provider credential"); ++calls;
            if (url.find("checkcached") != std::string::npos) {
                expect(body && json::parse(*body)["hashes"][0] == hash, "cache check hash wrong");
                return response({{"success", true}, {"data", {{hash, {{"hash", hash}}}}}});
            }
            if (url.find("createtorrent") != std::string::npos) {
                expect(body && body->find("add_only_if_cached=true") != std::string::npos && type == "application/x-www-form-urlencoded", "uncached job possible");
                return response({{"success", true}, {"data", {{"torrent_id", 12}}}});
            }
            if (url.find("mylist") != std::string::npos) return response({{"success", true}, {"data", json::array({json{{"id", 99}, {"files", json::array()}},
                json{{"id", 12}, {"files", json::array({json{{"id", 77}, {"name", "Movie.2160p.HEVC.mkv"}, {"size", 20000000000LL}}})}}})}});
            expect(url.find("file_id=77") != std::string::npos && url.find("token=torbox-test") != std::string::npos, "wrong selected file");
            return response({{"success", true}, {"data", "https://cdn.invalid/original.mkv"}});
        });
        const auto r = client.resolve(stream(), settings(), -1, -1);
        expect(r.ok && calls == 4 && r.stream.url == "https://cdn.invalid/original.mkv", "TorBox failed");
        expect(r.stream.name == stream().name && r.stream.video_size == 20000000000.0 && r.stream.request_headers.empty(), "quality changed or auth leaked to CDN");
    });
    check("Premiumize selects original link instead of transcoded stream_link", [] {
        int calls = 0;
        debrid::Client client([&](const auto& url, const auto* body, const auto& type, const auto& headers, const auto*) {
            ++calls; expect(headers.back() == "Authorization: Bearer premiumize-test" && body && type == "application/x-www-form-urlencoded", "Premiumize request wrong");
            if (url.find("cache/check") != std::string::npos) return response({{"status", "success"}, {"response", json::array({true})}});
            return response({{"status", "success"}, {"content", json::array({json{{"path", "Show.S01E01.2160p.mkv"}, {"link", "https://cdn.invalid/original.mkv"},
                {"stream_link", "https://cdn.invalid/transcoded.mp4"}, {"size", 9999999999LL}}})}});
        });
        const auto r = client.resolve(stream("premiumize"), settings(), 1, 1);
        expect(r.ok && calls == 2 && r.stream.url == "https://cdn.invalid/original.mkv", "transcoded variant selected");
    });
    check("cache misses never create jobs on either provider", [] {
        for (const auto& provider : {"torbox", "premiumize"}) {
            int calls = 0;
            debrid::Client client([&](auto&, auto*, auto&, auto&, auto*) {
                expect(++calls == 1, "uncached torrent was submitted");
                return std::string(provider) == "torbox" ? response({{"success", true}, {"data", json::object()}})
                    : response({{"status", "success"}, {"response", json::array({false})}});
            });
            expect(!client.resolve(stream(provider), settings(), -1, -1).ok && calls == 1, "cache miss accepted");
        }
    });
    check("malformed cache responses do not authorize creation", [] {
        for (json value : {json("cached"), json::object(), json(nullptr), json(false)}) {
            int calls = 0;
            debrid::Client client([&](auto&, auto*, auto&, auto&, auto*) { ++calls; return response({{"success", true}, {"data", {{hash, value}}}}); });
            expect(!client.resolve(stream(), settings(), -1, -1).ok && calls == 1, "bad cache status accepted");
        }
    });
    check("errors do not reveal provider keys or playable URLs", [] {
        debrid::Client client([](auto&, auto*, auto&, auto&, auto*) {
            auto h = response({{"message", "private https://cdn.invalid/?token=torbox-test"}}, 401);
            h.error = "private torbox-test"; return h;
        });
        const auto r = client.resolve(stream(), settings(), -1, -1);
        expect(!r.ok && r.error.find("torbox-test") == std::string::npos && r.error.find("https://") == std::string::npos, "error contains credentials");
    });
    check("cancellation stops before another provider request", [] {
        std::atomic<bool> cancel{false}; int calls = 0;
        debrid::Client client([&](auto&, auto*, auto&, auto&, auto*) { ++calls; cancel = true; return response({{"success", true}, {"data", {{hash, true}}}}); });
        const auto r = client.resolve(stream(), settings(), -1, -1, &cancel);
        expect(!r.ok && r.error == "cancelled" && calls == 1, "cancel did not stop resolution");
    });
    check("episode matching rejects prefix collisions and unrelated episodes", [] {
        const json files = json::array({json{{"name", "Show.S01E010.mkv"}}, json{{"name", "Show.S01E01.mkv"}}, json{{"name", "Show.S01E02.mkv"}}});
        auto s = stream(); s.client_resolve["fileIdx"] = 0;
        expect(debrid::select_file(files, s, 1, 1) == &files[1], "S01E010 mistaken for E01");
        expect(debrid::select_file(files, s, 2, 1) == nullptr, "wrong season selected");
        expect(debrid::select_file(files, s, 1, 3) == nullptr, "missing episode replaced");
    });
    check("filename and fileIdx preserve the selected encode", [] {
        const json files = json::array({json{{"name", "Movie.2160p.mkv"}, {"size", 50}}, json{{"name", "Movie.1080p.mkv"}, {"size", 100}}});
        auto s = stream(); s.filename = "Movie.2160p.mkv";
        expect(debrid::select_file(files, s, -1, -1) == &files[0], "larger different encode selected");
        s.filename = "Missing.2160p.mkv"; expect(!debrid::select_file(files, s, -1, -1), "missing explicit file replaced");
        s.client_resolve["fileIdx"] = 0; expect(debrid::select_file(files, s, -1, -1) == &files[0], "zero-based fileIdx ignored");
    });
    check("keys reject header injection and remain in a private atomic file", [] {
        expect(!debrid::valid_key("test\r\nInjected: token") && !debrid::valid_key("test key") && debrid::valid_key("opaque-key"), "invalid header key accepted");
        char folder[] = "/tmp/nuvio-services-XXXXXX"; expect(mkdtemp(folder), "temp directory failed");
        const std::string path = std::string(folder) + "/connected-services.json";
        auto cfg = settings(); cfg.preferred = "premiumize"; cfg.instant_limit = 4;
        expect(debrid::save(path, cfg), "credential save failed");
        struct stat st{}; expect(stat(path.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600, "keys world-readable");
        debrid::Settings restored; expect(debrid::load(path, restored) && restored.premiumize.key == cfg.premiumize.key && restored.preferred == cfg.preferred && restored.instant_limit == 4, "settings not restored");
        unlink(path.c_str()); rmdir(folder);
    });
    check("sync import supports JSON rows and clears only account-imported keys", [] {
        auto cfg = settings();
        const json snapshot{{"owner_id", "owner"}, {"instant_limit", 3}, {"credentials", json::array({json{{"provider", "debrid:premiumize"},
            {"credential_json", R"({"api_key":"synced-test"})"}}})}};
        expect(debrid::import_snapshot(cfg, snapshot) && cfg.premiumize.key == "synced-test" && cfg.instant_limit == 3, "sync credentials not imported");
        cfg.clear_synced(); expect(cfg.premiumize.key.empty() && cfg.torbox.key == "torbox-test", "manual credentials erased on sign-out");
        expect(!debrid::import_snapshot(cfg, json{{"credentials", json::array()}}), "unscoped credentials accepted");
    });
    check("TorBox QR flow handles pending, approved and expired codes", [] {
        int step = 0;
        debrid::Client client([&](const auto& url, const auto* body, auto&, const auto& headers, auto*) {
            expect(headers.size() == 1, "account token leaked to QR auth");
            if (++step == 1) {
                expect(url.find("device/start") != std::string::npos && !body, "device auth start wrong");
                return response({{"success", true}, {"data", {{"device_code", "private-code"}, {"code", "ABCD"},
                    {"verification_url", "https://torbox.app/auth"}, {"interval", 1}, {"expires_in", 300}}}});
            }
            expect(body && json::parse(*body)["device_code"] == "private-code", "device code wrong");
            if (step == 2) return response({{"success", false}, {"detail", "Authorization pending"}}, 409);
            if (step == 3) return response({{"success", true}, {"data", {{"access_token", "qr-key"}}}});
            return response({{"detail", "expired"}}, 410);
        });
        const auto code = client.start_torbox(); expect(code.ok && code.interval >= 5 && code.lifetime == 300, "QR code malformed");
        expect(client.poll_torbox(code.device_code).state == debrid::DeviceToken::pending, "pending rejected");
        const auto token = client.poll_torbox(code.device_code); expect(token.state == debrid::DeviceToken::authorized && token.key == "qr-key", "approved token lost");
        expect(client.poll_torbox(code.device_code).state == debrid::DeviceToken::expired, "expiry missed");
    });
    check("malformed sync snapshots cannot partially replace working credentials", [] {
        auto cfg = settings();
        const json snapshot{{"owner_id", "owner"}, {"credentials", json::array({
            json{{"provider", "debrid:torbox"}, {"credential_json", {{"api_key", "replacement"}}}},
            json{{"provider", "debrid:premiumize"}, {"credential_json", "invalid-json"}}})}};
        expect(!debrid::import_snapshot(cfg, snapshot) && cfg.torbox.key == "torbox-test", "partial import overwrote working key");
    });
    std::cout << tests << " Connected Services tests passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
