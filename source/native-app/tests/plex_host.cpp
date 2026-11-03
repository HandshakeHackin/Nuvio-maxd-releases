#include "http_credentials.hpp"
#include "plex_fixture.hpp"
#include <iostream>
int main(int argc, char **) {
  http_init("");
  if (argc > 1) {
    auto p = plex::Client().start();
    if (!p.error.empty()) {
      std::cerr << p.error << "\n";
      return 1;
    }
    auto r = plex::Client().poll(p);
    assert(r.error.empty() && r.pending);
    std::cout << "Live Plex PIN registration and signed pending poll passed; "
                 "no account connected\n";
    return 0;
  }
  PlexFixture f;
  auto api = f.client();
  auto pair = api.start();
  assert(pair.error.empty() && pair.code == "ABCD");
  assert(api.poll(pair).pending);
  f.approved = true;
  pair = api.poll(pair);
  assert(!pair.pending && pair.error.empty());
  auto resources = api.servers(pair);
  assert(resources.error.empty() && resources.items.size() == 1);
  auto server = resources.items[0];
  assert(server.urls.size() == 2 && server.urls[0] == "http://127.0.0.1:32400");
  media_server::Connection c;
  c.id = "connection-1";
  assert(api.connect(c, pair, server).ok() && c.valid());
  auto saved = c.saved();
  assert(media_server::Connection::restore(saved).valid() &&
         saved["plexAuth"]["key"] == pair.key);
  auto views = api.browse(c, "", "views", "", 0);
  assert(views.ok() && views.data["Items"][0]["Id"] == "lib-1");
  auto browse = api.browse(c, "lib-1", "", "", 100);
  assert(browse.ok() && browse.data["TotalRecordCount"] == 101);
  assert(browse.data["Items"][0]["UserData"]["PlaybackPositionTicks"] ==
         250000000);
  f.bad_page = true;
  assert(!api.browse(c, "lib-1", "", "", 100).ok());
  f.bad_page = false;
  f.bad_json = true;
  assert(!api.browse(c, "", "views", "", 0).ok());
  f.bad_json = false;
  auto search = api.browse(c, "", "", "movie & title", 0);
  assert(search.ok() && search.data["TotalRecordCount"] == 1);
  assert(api.browse(c, "42", "episodes", "", 0).ok());
  assert(f.paths.back().find("/library/metadata/42/allLeaves?") == 0);
  auto direct = api.playback(c, "42", "70", false);
  assert(direct.error.empty() && direct.url == c.base + f.part &&
         direct.duration == 120);
  assert(http_credentials::protected_request(
      http_credentials::filter(direct.headers, direct.url)));
  assert(!http_credentials::protected_request(http_credentials::filter(
      direct.headers, "https://foreign.invalid/file")));
  for (auto &h : http_credentials::filter(direct.headers, direct.url))
    assert(!h.starts_with("X-Maxd-"));
  auto hls = api.playback(c, "42", "70", true);
  assert(hls.error.empty() &&
         hls.url.find("start.m3u8?") != std::string::npos &&
         hls.url.find("token") == std::string::npos &&
         hls.url.find("audioCodec%3Daac") != std::string::npos);
  assert(api.report(c, hls, "42", 12.5, true, "progress").ok());
  assert(f.paths.back().find("state=paused") != std::string::npos);
  assert(api.report(c, hls, "42", 12.5, false, "stop").ok() &&
         f.timeline == 2 && f.stopped == 1);
  assert(api.image(c, "42").body == "image");
  assert(!api.image(c, "lib-1").ok());
  f.deny = true;
  assert(!api.playback(c, "42", "70", true).error.empty());
  f.deny = false;
  f.part = "https://foreign.invalid/file.mkv";
  assert(!api.playback(c, "42", "70", false).error.empty());
  f.part = "/library/parts/42/file.mkv";
  f.multipart = true;
  assert(!api.playback(c, "42", "70", false).error.empty());
  f.multipart = false;
  f.reject = true;
  assert(api.browse(c, "", "views", "", 0).ok() && f.refreshes == 1);
  assert(c.saved()["token"] == "new-server-token" &&
         c.saved()["plexAuth"]["accountToken"] == "new-account-token");
  assert(api.browse(c, "", "views", "", 0).ok() && f.refreshes == 1);
  // Proactive renewal must also work after process restart, before a server
  // rejects a token.
  json expired = {{"exp", 1}};
  auto raw = expired.dump();
  auto disk = c.saved();
  disk["token"] = "server-token";
  disk["plexAuth"]["accountToken"] =
      "header." +
      plex::base64url((const unsigned char *)raw.data(), raw.size()) +
      ".signature";
  auto restored = media_server::Connection::restore(disk);
  f.renewed = false;
  f.reject = false;
  assert(api.browse(restored, "", "views", "", 0).ok() && f.refreshes == 2);
  std::atomic<bool> cancel = true;
  int n = f.calls;
  assert(!api.browse(c, "", "views", "", 0, &cancel).ok() && f.calls == n);
  auto wrong = server;
  wrong.id = "different-server";
  media_server::Connection other;
  other.id = "other";
  wrong.token = "new-server-token";
  assert(!api.connect(other, pair, wrong).ok());
  std::cout << "Plex signed pairing, server discovery, browsing, versions, "
               "direct/HLS playback, progress, credential scope, renewal and "
               "cancellation passed\n";
}
