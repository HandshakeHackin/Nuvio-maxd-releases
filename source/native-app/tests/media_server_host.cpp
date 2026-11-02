#include "http_credentials.hpp"
#include "media_server.h"
#include "playback_log.hpp"
#include "trailers.h"
#include <cassert>
#include <iostream>
#include <thread>
using namespace media_server;
int main(int argc, char **argv) {
  assert(argc == 2);
  http_init("");
  std::string root = argv[1];
  assert(normalize(" example.org:8096/web/index.html/ ", "jellyfin") ==
         "http://example.org:8096");
  assert(normalize("https://example.org/reverse/emby/", "emby") ==
         "https://example.org/reverse/emby");
  for (auto b :
       {"file:///etc/passwd", "https://user:password@host",
        "http://host/?token=x", "http://host/\r\nx:y", "http://host\\evil"})
    assert(normalize(b, "jellyfin").empty());
  assert(origin("https://EXAMPLE.org:443/x") == "https://example.org");
  assert(!safe_id("../bad"));
  assert(playback_log::redact_urls(
             "Opening 'https://user:secret@host/stream?api_key=private' then "
             "http://other/x?token=private") ==
         "Opening '[media URL]' then [media URL]");
  for (auto kind : {std::string("jellyfin"), std::string("emby")}) {
    Connection c;
    c.kind = kind;
    c.base = root + (kind == "emby" ? "/reverse" : "/jellyfin");
    c.id = "connection-1";
    c.device = "ps5-test";
    auto auth = authenticate(c, "alice", "test-password");
    assert(auth.ok() && c.valid() && c.user == "user-1");
    assert(Connection::restore(c.saved()).valid());
    assert(c.saved().dump().find("test-password") == std::string::npos);
    auto bad = c;
    bad.token = "bad\r\nvalue";
    assert(!bad.valid());
    auto views = browse(c, "", "views", "", 0);
    assert(views.ok() && views.data["Items"][0]["Id"] == "library-1");
    auto titles = browse(c, "library-1", "items", "film & title", 100);
    assert(titles.ok() && titles.data["Items"][0]["Type"] == "Movie");
    auto episodes = browse(c, "series-1", "episodes", "", 0);
    assert(episodes.ok() && episodes.data["Items"][0]["IndexNumber"] == 2);
    assert(browse(c, "", "resume", "", 0).ok());
    assert(!browse(c, "../bad", "items", "", 0).ok());
    auto p = playback(c, "movie-1", "", false);
    assert(p.error.empty() && p.method == "DirectPlay" &&
           p.url.find("Static=true") != std::string::npos);
    auto headers = http_credentials::filter(p.headers, p.url);
    assert(http_credentials::protected_request(headers));
    for (auto &h : headers)
      assert(!h.starts_with("X-Maxd-Credential"));
    assert(!http_credentials::protected_request(http_credentials::filter(
        p.headers, "http://foreign.invalid/segment.ts")));
    auto compatible = playback(c, "movie-1", "", true);
    assert(compatible.error.empty() && compatible.method == "Transcode");
    assert(compatible.url.find("api_key") == std::string::npos &&
           compatible.url.find(c.token) == std::string::npos);
    auto bare = playback(c, "bare-relative", "", true);
    assert(bare.error.empty() && bare.url.starts_with(c.base + "/Videos/") &&
           bare.url.find(c.token) == std::string::npos);
    assert(!playback(c, "foreign", "", true).error.empty());
    assert(!playback(c, "missing", "", false).error.empty());
    assert(report(c, p, "movie-1", 12.5, false, "start").ok());
    assert(report(c, p, "movie-1", 25, true, "progress").ok());
    assert(report(c, p, "movie-1", 30, false, "stop").ok());
    assert(image(c, "movie-1").ok());
    assert(!request(c, "/denied").ok());
    assert(!request(c, "/redirect").ok());
    assert(!request(c, "/bad-json").ok());
    assert(!request(c, "/oversized").ok());
    assert(!browse(c, "too-many", "items", "", 0).ok());
    std::atomic<bool> cancel{true};
    assert(!request(c, "/slow", nullptr, &cancel).ok());
    {
      Reporter reporter;
      reporter.push(c, p, "movie-1", 1, false, "start");
      for (int i = 0; i < 100; ++i)
        reporter.push(c, p, "movie-1", i, false, "progress");
      reporter.push(c, p, "movie-1", 100, true, "stop");
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    json body = json::object();
    assert(request(c, "/Sessions/Logout", &body).ok());
  }
  assert(!trailers::records_history(false, "media-video"));
  assert(trailers::records_history(false, "movie"));
  std::cout << "Media server auth, library, episodes, resume, playback, "
               "progress, limits and credential isolation passed\n";
}
