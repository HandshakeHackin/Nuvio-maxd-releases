// Production feature cores with deterministic provider responses and real
// crypto/IO.
#include "config_backup.h"
#include "config_drive.h"
#include "episode_playback.hpp"
#include "source_policy.hpp"
#include "stalker.h"
#include "title_actions.hpp"
#include "tvdb.h"
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
using J = nlohmann::json;
int checks = 0;
void require(bool ok, const char *label) {
  if (!ok)
    throw std::runtime_error(label);
  ++checks;
}
feature_http::Response response(J j, long status = 200) {
  feature_http::Response r;
  r.status = status;
  r.body = j.dump();
  return r;
}
feature_http::Response portal(J j) { return response(J{{"js", j}}); }
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    const std::string dir = argv[1];
    const auto scope = config_backup::scope("test-account", 2);
    require(config_backup::save(
                dir + "/profile-settings.json",
                J{{"extra_addons",
                   J::array({"https://addon.invalid/manifest.json"})},
                  {"provider_settings", {{"private-key", "secret-fixture"}}},
                  {"account_session", "never-back-up"}}
                    .dump()),
            "write profile fixture");
    require(config_backup::save(
                dir + "/live-tv.json",
                J{{"sources", J::array({J{{"type", "usb"},
                                          {"id", "123"},
                                          {"name", "Private fixture"}}})},
                  {"favorites", J::array()}}
                    .dump()),
            "write Live TV fixture");
    const std::string playlist = "#EXTM3U\n#EXTINF:-1,Fixture\nhttps://"
                                 "media.invalid/test?secret=private\n";
    require(config_backup::save(dir + "/usb-playlist-123.m3u", playlist),
            "write imported fixture");
    require(config_backup::save(dir + "/connected-services.json",
                                "{\"token\":\"excluded\"}"),
            "write excluded fixture");
    auto captured = config_backup::capture(dir, scope);
    require(captured.error.empty() && captured.snapshot["files"].size() == 4,
            "capture configuration and imported playlist");
    require(captured.snapshot.dump().find("never-back-up") ==
                    std::string::npos &&
                !captured.snapshot["files"].contains("connected-services.json"),
            "exclude account session and connected-service tokens");
    require(
        !config_backup::valid_passphrase("备份密码") &&
            config_backup::valid_passphrase("安全备份密码安全备份密码"),
        "passphrase minimum counts Unicode characters instead of UTF-8 bytes");
    const std::string pass = "separate backup passphrase";
    auto encrypted = config_backup::encrypt(captured.snapshot, pass),
         second = config_backup::encrypt(captured.snapshot, pass);
    require(encrypted.error.empty() && encrypted.bytes != second.bytes &&
                encrypted.bytes.find("secret-fixture") == std::string::npos,
            "random authenticated encryption hides credentials");
    auto decrypted = config_backup::decrypt(encrypted.bytes, pass, scope);
    require(decrypted.error.empty() &&
                decrypted.snapshot == captured.snapshot &&
                decrypted.hash == captured.hash,
            "encrypted round trip");
    require(!config_backup::decrypt(encrypted.bytes, "wrong backup passphrase",
                                    scope)
                 .error.empty(),
            "wrong key rejected");
    require(!config_backup::decrypt(encrypted.bytes, pass,
                                    config_backup::scope("another-account", 2))
                 .error.empty(),
            "foreign account rejected");
    require(!config_backup::decrypt(encrypted.bytes, pass,
                                    config_backup::scope("test-account", 1))
                 .error.empty(),
            "foreign profile rejected");
    for (size_t at : {size_t(8), size_t(35), encrypted.bytes.size() - 1}) {
      auto damaged = encrypted.bytes;
      damaged[at] ^= 1;
      require(!config_backup::decrypt(damaged, pass, scope).error.empty(),
              "header nonce and tag tampering rejected");
    }
    require(!config_backup::encrypt(captured.snapshot, "short").error.empty(),
            "weak passphrase rejected");
    require(
        !config_backup::decrypt(
             std::string(config_backup::max_encrypted + 1, 'x'), pass, scope)
             .error.empty(),
        "oversized ciphertext rejected before KDF");
    std::atomic<bool> cancelled{true};
    require(config_backup::capture(dir, scope, &cancelled).error == "Cancelled",
            "backup cancellation");
    auto invalid = captured.snapshot;
    invalid["files"]["../outside"] = "{}";
    std::string error;
    require(!config_backup::restore(dir, invalid, scope, error),
            "archive path traversal rejected before writes");
    invalid = captured.snapshot;
    invalid["files"]["tvdb.json"] = J::object();
    require(!config_backup::encrypt(invalid, pass).error.empty(),
            "non-string file payload reports a failure");
    invalid = captured.snapshot;
    invalid["files"]["usb-playlist-123.m3u"] = std::string(1, char(0xff));
    require(!config_backup::encrypt(invalid, pass).error.empty(),
            "malformed UTF-8 fails before JSON serialization");
    invalid = captured.snapshot;
    invalid["files"]["usb-playlist-123.m3u"] = "not a playlist";
    require(!config_backup::validate(invalid, scope, error),
            "invalid imported playlist rejected");
    require(config_backup::save(dir + "/progress.json", "history-marker"),
            "write excluded history fixture");
    auto changed = captured.snapshot;
    changed["files"]["profile-settings.json"] =
        J{{"subtitle_languages", "deu"}}.dump();
    require(config_backup::restore(dir, changed, scope, error),
            "restore complete profile transaction");
    std::string bytes;
    require(config_backup::read(dir + "/progress.json", bytes, 1024) &&
                bytes == "history-marker",
            "restore preserves playback history");
    struct stat st{};
    require(stat((dir + "/profile-settings.json").c_str(), &st) == 0 &&
                (st.st_mode & 0777) == 0600,
            "restored credentials remain private");
    J originals{
        {"profile-settings.json", changed["files"]["profile-settings.json"]},
        {"tvdb.json", changed["files"]["tvdb.json"]}};
    require(config_backup::save(dir + "/.configuration-rollback.json",
                                originals.dump()) &&
                config_backup::save(dir + "/profile-settings.json",
                                    "{\"auto_subtitles\":false}"),
            "simulate interrupted restore after first replacement");
    require(
        config_backup::recover(dir, error) &&
            config_backup::read(dir + "/profile-settings.json", bytes, 1024) &&
            bytes == originals["profile-settings.json"].get<std::string>(),
        "recover interrupted restore before loading profile");
    std::filesystem::create_symlink(dir + "/progress.json", dir + "/symlink");
    require(!config_backup::read(dir + "/symlink", bytes, 1024),
            "private reads reject symlinks");
    Meta meta;
    meta.id = meta.imdb_id = "tt123";
    meta.type = "series";
    meta.name = "Fixture";
    Video a;
    a.id = "tt123:1:1";
    a.title = "First";
    a.released = "2020-01-01";
    a.season = 1;
    a.episode = 1;
    Video b = a;
    b.id = "tt123:1:2";
    b.title = "Second";
    b.released = "2020-01-02";
    b.episode = 2;
    meta.videos = {a, b};
    std::vector<tvdb::Episode> aired{{"100", "First", "2020-01-01", 1, 1},
                                     {"101", "Second", "2020-01-02", 1, 2}},
        dvd{{"101", "Second", "2020-01-02", 2, 1},
            {"100", "First", "2020-01-01", 2, 2}};
    auto ordered = meta;
    require(tvdb::reorder(ordered, aired, dvd, error),
            "verified alternate order");
    require(ordered.videos[0].id == b.id && ordered.videos[0].season == 1 &&
                ordered.videos[0].episode == 2 &&
                display_season(ordered.videos[0]) == 2 &&
                display_episode(ordered.videos[0]) == 1,
            "canonical identity stays separate from display order");
    require(stream_video_id(ordered, b.id) == "tt123:2:1" &&
                sx::source_policy::context(ordered, b.id).episode == 1,
            "source requests and pack filtering use selected numbering");
    const auto *next =
        sx::episode_playback::next_video(ordered.videos, b.id, 2000000000000LL);
    require(next && next->id == a.id, "next episode follows selected order");
    auto marks = sx::title_actions::episodes(ordered, meta.id, 2, true, {},
                                             2000000000000LL);
    require(marks.size() == 2 && marks[0]["season"] == 1 &&
                marks[0]["episode"] == 2,
            "mark displayed season sends canonical tracker coordinates");
    auto ambiguous = dvd;
    ambiguous[1].id = "101";
    auto untouched = meta;
    require(!tvdb::reorder(untouched, aired, ambiguous, error) &&
                untouched.videos[0].id == a.id &&
                untouched.videos[0].order_season == -1,
            "ambiguous order leaves provider metadata unchanged");
    auto split = dvd;
    split.push_back({"102", "First", "2020-01-01", 2, 3});
    untouched = meta;
    require(!tvdb::reorder(untouched, aired, split, error),
            "combined or duplicate content mapping rejected");
    auto mismatched = meta;
    mismatched.videos[0].title = "Different file";
    require(!tvdb::reorder(mismatched, aired, dvd, error),
            "coordinates alone cannot establish correspondence");
    int calls = 0;
    feature_http::Request tvrequest = [&](const auto &q, const auto *) {
      ++calls;
      require(q.url.rfind("https://api4.thetvdb.com/v4/", 0) == 0 &&
                  q.https_only,
              "TVDB requests stay on fixed HTTPS API");
      if (q.url.ends_with("/login"))
        return response(
            J{{"status", "success"}, {"data", {{"token", "fixture-token"}}}});
      if (q.url.find("search/remoteid") != std::string::npos)
        return response(J{{"status", "success"},
                          {"data", J::array({J{{"series", {{"id", 42}}}}})}});
      if (q.url.ends_with("extended"))
        return response(J{
            {"status", "success"},
            {"data",
             {{"seasons",
               J::array({J{{"type", {{"type", "official"}, {"name", "Aired"}}}},
                         J{{"type", {{"type", "dvd"}, {"name", "DVD"}}}}})}}}});
      const bool alt = q.url.find("/dvd/") != std::string::npos;
      J rows = J::array();
      for (const auto &e : alt ? dvd : aired)
        rows.push_back({{"id", e.id},
                        {"name", e.title},
                        {"aired", e.aired},
                        {"seasonNumber", e.season},
                        {"number", e.episode}});
      return response(J{{"status", "success"},
                        {"data", {{"episodes", rows}}},
                        {"links", {{"next", nullptr}}}});
    };
    auto loaded = tvdb::load(meta, {"own-key", ""}, "dvd", nullptr, tvrequest);
    require(loaded.error.empty() && calls == 5 &&
                loaded.meta.videos[0].id == b.id,
            "TVDB auth lookup orders and mapping protocol");
    require(!tvdb::test({"own-key", ""}, error, &cancelled, tvrequest),
            "TVDB cancellation");
    stalker::Source source{"https://portal.invalid/c/", "00:1A:79:12:34:56", "",
                           "", ""};
    require(stalker::valid(source) &&
                stalker::endpoint(source.portal) ==
                    "https://portal.invalid/server/load.php",
            "normalise MAG portal endpoint");
    require(
        stalker::endpoint("https://portal.invalid/stalker_portal/portal.php") ==
                "https://portal.invalid/stalker_portal/portal.php" &&
            stalker::endpoint("https://portal.invalid/?credentials=x").empty(),
        "explicit endpoint accepted and embedded query rejected");
    auto badsource = source;
    badsource.mac = "invalid";
    require(!stalker::valid(badsource), "reject invalid MAC");
    bool repeat = false, series_listing = false;
    int pages = 0;
    feature_http::Request portalrequest = [&](const auto &q, const auto *) {
      require(q.url.rfind("https://portal.invalid/server/load.php?", 0) == 0 &&
                  !q.https_only && q.limit <= 128u << 20,
              "portal stays on configured origin with bounded request");
      if (q.url.find("action=handshake") != std::string::npos)
        return portal(J{{"token", "session-only-token"}, {"random", "random"}});
      require(std::find(q.headers.begin(), q.headers.end(),
                        "Authorization: Bearer session-only-token") !=
                  q.headers.end(),
              "portal auth attached only after handshake");
      if (q.url.find("action=get_profile") != std::string::npos)
        return portal(J{{"status", 0}});
      if (q.url.find("action=get_genres") != std::string::npos)
        return portal(J::array({J{{"id", "1"}, {"title", "News"}}}));
      if (q.url.find("action=get_all_channels") != std::string::npos)
        return portal(
            J{{"data", J::array({J{{"id", "10"},
                                   {"name", "Fixture"},
                                   {"cmd", "ffmpeg http://localhost/ch/10"},
                                   {"tv_genre_id", "1"}}})}});
      if (q.url.find("action=create_link") != std::string::npos)
        return portal(
            J{{"cmd", "ffmpeg https://cdn.invalid/signed?token=media-only"}});
      if (q.url.find("action=get_epg_info") != std::string::npos)
        return portal(J{{"10", J::array({J{{"start_timestamp", 1700000000},
                                           {"stop_timestamp", 1700003600},
                                           {"name", "Guide fixture"}}})}});
      if (q.url.find("action=get_ordered_list") != std::string::npos) {
        ++pages;
        const bool empty = !repeat && pages > 1;
        const J row = series_listing
                          ? J{{"id", "20"},
                              {"name", "Season 1"},
                              {"series", J::array({"1", "2"})},
                              {"cmd", "ffmpeg http://localhost/series/20"}}
                          : J{{"id", "20"},
                              {"name", "Movie fixture"},
                              {"category_id", "2"},
                              {"cmd", "ffmpeg http://localhost/movie/20"}};
        return portal(J{{"data", empty ? J::array() : J::array({row})}});
      }
      return portal(J::array({J{{"id", "2"}, {"title", "Movies"}}}));
    };
    auto channels = stalker::channels(source, nullptr, portalrequest);
    require(channels.error.empty() && channels.channels.size() == 1 &&
                channels.channels[0].group == "News",
            "portal channel parsing");
    auto link = stalker::resolve(
        source,
        channels.channels[0].url.substr(std::string(stalker::marker).size()),
        false, 0, nullptr, portalrequest);
    require(
        link.error.empty() && link.url.rfind("https://cdn.invalid/", 0) == 0 &&
            link.headers.size() == 1 &&
            link.headers[0].rfind("User-Agent:", 0) == 0,
        "signed link resolved without forwarding portal credentials to CDN");
    auto guide =
        stalker::guide(source, "10", 1700000000, nullptr, portalrequest);
    require(guide.error.empty() && guide.channels["10"].size() == 1,
            "portal guide timestamp parsing");
    auto catalog = stalker::catalog(source, iptv::Kind::movies, "2", nullptr,
                                    portalrequest);
    require(catalog.error.empty() && catalog.entries.size() == 1 &&
                !catalog.entries[0].command.empty() && pages == 2,
            "bounded portal VOD pagination");
    std::filesystem::create_directory(dir + "/portal-cache");
    auto cached = catalog;
    cached.entries[0].id = "portal:20";
    require(iptv::save_cache(dir + "/portal-cache", "portal",
                             iptv::Kind::movies, "2", cached) &&
                iptv::load_cache(dir + "/portal-cache", "portal",
                                 iptv::Kind::movies, "2", cached) &&
                cached.entries.size() == 1 &&
                cached.entries[0].id == "portal:20" &&
                !cached.entries[0].command.empty(),
            "portal opaque IDs and commands survive private cache roundtrip");
    const auto accounts = iptv::accounts(J::array({J{{"id", "portal"},
                                                     {"type", "stalker"},
                                                     {"url", source.portal},
                                                     {"mac", source.mac}}}));
    require(accounts.size() == 1 && accounts[0].type == "stalker" &&
                accounts[0].mac == source.mac,
            "portal accounts reach isolated IPTV Library without Xtream "
            "credentials");
    repeat = true;
    pages = 0;
    catalog = stalker::catalog(source, iptv::Kind::movies, "2", nullptr,
                               portalrequest);
    require(!catalog.error.empty() && catalog.entries.empty() && pages == 2,
            "repeated portal page rejects whole partial catalogue");
    repeat = false;
    series_listing = true;
    pages = 0;
    auto episodes = stalker::catalog(source, iptv::Kind::episodes, "101",
                                     nullptr, portalrequest);
    pages = 0;
    auto other_series = stalker::catalog(source, iptv::Kind::episodes, "102",
                                         nullptr, portalrequest);
    require(episodes.error.empty() && other_series.error.empty() &&
                episodes.entries.size() == 2 &&
                other_series.entries.size() == 2 &&
                episodes.entries[0].season == 1 &&
                episodes.entries[1].episode == 2 &&
                episodes.entries[0].id != other_series.entries[0].id,
            "portal series selectors and resume identities remain isolated "
            "between titles");
    auto caps = stalker::probe(source, nullptr, portalrequest);
    require(caps.movies_ok && caps.series_ok && caps.movies.size() == 1,
            "portal movie and series capability probe");
    auto rejected =
        stalker::channels(source, nullptr, [](const auto &, const auto *) {
          return response(J{}, 403);
        });
    require(!rejected.error.empty() &&
                rejected.error.find(source.mac) == std::string::npos,
            "portal errors redact account details");
    auto numeric_array = [](size_t count) {
      std::string body = "[";
      body.reserve(count * 2 + 2);
      for (size_t i = 0; i < count; ++i)
        body += "0,";
      body += "0]";
      return body;
    };
    const auto huge_portal = "{\"js\":" + numeric_array(2000000) + "}";
    auto structural =
        stalker::channels(source, nullptr, [&](const auto &, const auto *) {
          feature_http::Response h;
          h.status = 200;
          h.body = huge_portal;
          return h;
        });
    require(structural.channels.empty() &&
                structural.error.find("structural limits") != std::string::npos,
            "portal structural bounds reject excessive JSON nodes within the "
            "byte budget");
    const auto huge_tvdb =
        "{\"status\":\"success\",\"data\":{\"token\":\"fixture\",\"extra\":" +
        numeric_array(500000) + "}}";
    require(!tvdb::test({"own-key", ""}, error, nullptr,
                        [&](const auto &, const auto *) {
                          feature_http::Response h;
                          h.status = 200;
                          h.body = huge_tvdb;
                          return h;
                        }),
            "TVDB structural response bounds");
    auto huge_snapshot = captured.snapshot;
    huge_snapshot["files"]["profile-settings.json"] =
        "{\"provider_settings\":{\"many\":" + numeric_array(500000) + "}}";
    require(!config_backup::encrypt(huge_snapshot, pass).error.empty(),
            "configuration structural bounds before encryption or restore");
    const auto huge_drive =
        "{\"device_code\":\"fixture\",\"user_code\":\"ABCD-EFGH\","
        "\"verification_url\":\"https://www.google.com/device\",\"extra\":" +
        numeric_array(100000) + "}";
    require(!config_drive::begin({"client", "secret", ""}, nullptr,
                                 [&](const auto &, const auto *) {
                                   feature_http::Response h;
                                   h.status = 200;
                                   h.body = huge_drive;
                                   return h;
                                 })
                 .error.empty(),
            "Drive structural response bounds");
    using P = config_drive::Plan;
    require(config_drive::plan("", "a", "", true) == P::first_upload &&
                config_drive::plan("a", "b", "", true) == P::conflict,
            "first upload and deleted remote planning");
    require(config_drive::plan("a", "b", "c", false) == P::conflict &&
                config_drive::plan("a", "a", "b", false) == P::restore &&
                config_drive::plan("a", "b", "a", false) == P::upload &&
                config_drive::plan("a", "b", "b", false) == P::equal,
            "three-way sync conflict planning");
    config_drive::Remote remote;
    remote.id = "file-id";
    remote.etag = "\"revision-1\"";
    int writes = 0;
    auto uploaded = config_drive::upload(
        "access", scope, remote, encrypted.bytes, nullptr,
        [&](const auto &q, const auto *) {
          if (q.method == "PATCH") {
            ++writes;
            require(std::find(q.headers.begin(), q.headers.end(),
                              "If-Match: \"revision-1\"") != q.headers.end(),
                    "conditional Drive update");
            return response(J{}, 412);
          }
          return response(J{});
        });
    require(writes == 1 && uploaded.error.find("changed on another device") !=
                               std::string::npos,
            "concurrent cloud update retained");
    remote.etag.clear();
    writes = 0;
    uploaded = config_drive::upload("access", scope, remote, encrypted.bytes,
                                    nullptr, [&](const auto &, const auto *) {
                                      ++writes;
                                      return response(J{});
                                    });
    require(writes == 0 && !uploaded.error.empty(),
            "refuse unconditional cloud overwrite");
    auto duplicate = config_drive::download(
        "access", scope, nullptr, [](const auto &, const auto *) {
          return response(
              J{{"files", J::array({J{{"id", "one"}}, J{{"id", "two"}}})}});
        });
    require(!duplicate.error.empty(), "duplicate Drive backups fail closed");
    config_drive::Application app{"client-id", "client-secret", "refresh"};
    auto device =
        config_drive::begin(app, nullptr, [](const auto &q, const auto *) {
          require(q.url == "https://oauth2.googleapis.com/device/code" &&
                      q.body.find("drive.file") != std::string::npos,
                  "minimal Drive file OAuth scope");
          return response(
              J{{"device_code", "private-code"},
                {"user_code", "ABCD-EFGH"},
                {"verification_url", "https://www.google.com/device"},
                {"interval", 5},
                {"expires_in", 600}});
        });
    require(device.error.empty() && device.code == "private-code",
            "Google TV device sign-in");
    auto pending = config_drive::poll(
        app, device.code, nullptr, [](const auto &, const auto *) {
          return response(J{{"error", "slow_down"}}, 400);
        });
    require(pending.pending && pending.slow, "OAuth slow-down honored");
    require(!feature_http::safe_url("https://user:secret@example.invalid/") &&
                !feature_http::safe_url("http://example.invalid/") &&
                feature_http::safe_url("http://example.invalid/", false),
            "transport URL credential and protocol policy");
    std::cout << "Feature cores: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Feature regression failed: " << e.what() << "\n";
    return 1;
  }
}
