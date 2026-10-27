#include "iptv_library.h"
#include "live_tv.h"
#include "trailers.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <thread>
const char *kUserAgent = "Maxd-IPTV-Test";
void http_setup_handle(void *) {}
const std::string &http_ca_bundle() {
  static std::string s;
  return s;
}
int main(int argc, char **argv) {
  using namespace iptv;
  // Provider failures must not blame decoding on an outage or echo credentials.
  for (int code : {500, 502, 503, 504, 520}) {
    auto message = failure_message("The stream stopped (HTTP " +
                                   std::to_string(code) + ")");
    assert(message.find("IPTV provider unavailable") != std::string::npos);
    assert(message.find("may be experiencing an outage") != std::string::npos);
  }
  for (int code : {401, 403})
    assert(failure_message("", code).find("rejected access") !=
           std::string::npos);
  for (int code : {404, 410})
    assert(failure_message("", code).find("title is unavailable") !=
           std::string::npos);
  assert(failure_message("HTTP 429").find("limiting requests") !=
         std::string::npos);
  for (const auto *error :
       {"Operation timed out", "Could not resolve host: private.invalid",
        "Failed to connect", "Connection reset by peer"}) {
    auto message = failure_message(error);
    assert(message.find("Check your internet connection") != std::string::npos);
    assert(message.find("private.invalid") == std::string::npos);
  }
  for (const auto *error :
       {"EVO could not present this video frame", "HTTP 5030",
        "Cannot prepare the decoded video picture", "PS5 audio output stopped"})
    assert(failure_message(error).find("outage") == std::string::npos);
  assert(failure_message("The source returned a web page instead of video")
             .find("provider returned a web page") != std::string::npos);
  assert(failure_message("https://private.invalid/movie/secret/password/id")
             .find("secret") == std::string::npos);
  assert(failure_message("", 0, false).find("Saved titles are retained") !=
         std::string::npos);
  assert(argc == 2);
  const std::string dir = argv[1];
  std::filesystem::create_directories(dir);
  auto sources = Json::array(
      {{{"id", "one"},
        {"name", "Provider"},
        {"type", "xtream"},
        {"url", "https://test.invalid:8443/base/"},
        {"username", "a/b"},
        {"password", "p?ss"}},
       {{"id", "plain"},
        {"type", "m3u"},
        {"url", "https://test.invalid/list.m3u"}},
       {{"id", "m3u"},
        {"type", "m3u"},
        {"url", "https://test.invalid:8443/base/"
                "get.php?username=u%2Fx&password=p%3Fss&type=m3u_plus"}}});
  auto list = accounts(sources);
  assert(list.size() == 2 &&
         list[0].server == "https://test.invalid:8443/base" &&
         list[1].user == "u/x" &&
         list[1].server == "https://test.invalid:8443/base");
  auto a = list[0];
  Entry e;
  e.id = "19";
  e.extension = "mkv";
  assert(playback_url(a, Kind::movies, e) ==
         "https://test.invalid:8443/base/movie/a%2Fb/p%3Fss/19.mkv");
  assert(playback_url(a, Kind::episodes, e) ==
         "https://test.invalid:8443/base/series/a%2Fb/p%3Fss/19.mkv");
  assert(playback_url(a, Kind::series, e).empty());
  e.id = "../19";
  assert(playback_url(a, Kind::movies, e).empty());
  assert(identity("one", Kind::movies, "19") !=
         identity("two", Kind::movies, "19"));
  assert(identity("one", Kind::episodes, "19") !=
         identity("one", Kind::movies, "19"));
  assert(library_type("iptv-movie") && !library_type("movie"));
  assert(!trailers::records_history(false, "iptv-movie") &&
         !trailers::records_history(false, "iptv-series") &&
         trailers::records_history(false, "movie") &&
         trailers::records_history(false, "series"));
  auto catalog = parse_catalog(
      R"([{"stream_id":19,"name":"Film","category_id":"4","stream_icon":"https://art.invalid/p.jpg","container_extension":"mkv","direct_source":"https://wrong.invalid/private","metadata":{"password":"never retain"}},{"stream_id":19,"name":"Duplicate"},{"stream_id":"../9","name":"Bad"}])",
      Kind::movies);
  assert(catalog.error.empty() && catalog.entries.size() == 1 &&
         catalog.entries[0].poster == "https://art.invalid/p.jpg");
  assert(!parse_catalog("{}", Kind::movies).error.empty());
  assert(!parse_catalog("[7]", Kind::movies).error.empty());
  assert(!parse_catalog("[[]]", Kind::movies).error.empty());
  assert(!parse_catalog("[{", Kind::movies).error.empty());
  assert(parse_catalog("[]", Kind::movies).error.empty());
  auto series = parse_catalog(
      R"([{"series_id":19,"name":"Show","cover":"https://art.invalid/p.jpg?password=private"}])",
      Kind::series);
  assert(series.error.empty() && series.entries.size() == 1 &&
         series.entries[0].poster.empty());
  auto episodes = parse_episodes(
      R"({"episodes":{"2":[{"id":"42","episode_num":1,"title":"Second season","container_extension":"mp4"}],"1":[{"id":19,"episode_num":2,"title":"Later"},{"id":18,"episode_num":1,"title":"First"}]}})");
  assert(episodes.error.empty() && episodes.entries.size() == 3 &&
         episodes.entries[0].id == "18" && episodes.entries[2].season == 2);
  assert(!parse_episodes(R"({"episodes":{"1":{}}})").error.empty());
  std::atomic<bool> stop{true};
  assert(!parse_catalog("[]", Kind::movies, &stop).error.empty());
  assert(
      !parse_episodes(R"({"episodes":{"1":[{"id":1,"episode_num":1}]}})", &stop)
           .error.empty());
  stop = false;
  std::vector<std::string> actions;
  Request fake = [&](const std::string &url, size_t,
                     const std::atomic<bool> *) {
    actions.push_back(url);
    HttpResponse r;
    r.status = 200;
    if (url.find("action=") == std::string::npos)
      r.body = R"({"user_info":{"auth":1,"status":"Active"}})";
    else if (url.find("get_vod_categories") != std::string::npos)
      r.body = R"([{"category_id":4,"category_name":"Movies"}])";
    else if (url.find("get_series_categories") != std::string::npos)
      r.body = "[]";
    else
      r.body = "[]";
    return r;
  };
  auto c = probe(a, &stop, fake);
  assert(c.movies_ok && c.series_ok && c.movies.size() == 1 &&
         c.series.empty());
  assert(actions.size() == 4);
  assert(actions[0].find("username=a%2Fb&password=p%3Fss") !=
         std::string::npos);
  Json state = Json::object();
  reconcile(state, list);
  assert(!available(state, list));
  apply_capabilities(state, a.id, c);
  assert(available(state, list));
  Request offline = [](const std::string &, size_t, const std::atomic<bool> *) {
    HttpResponse r;
    r.error = "sensitive url and password";
    return r;
  };
  auto failed = probe(a, &stop, offline);
  assert(failed.error.find("password") == std::string::npos);
  apply_capabilities(state, a.id, failed);
  assert(available(state, list));
  auto empty = c;
  empty.movies.clear();
  apply_capabilities(state, a.id, empty);
  assert(!available(state, list));
  apply_capabilities(state, a.id, c);
  assert(save_private(dir + "/state.json", state));
  assert(available(load_private(dir + "/state.json"), list));
  struct stat st{};
  assert(stat((dir + "/state.json").c_str(), &st) == 0 &&
         (st.st_mode & 0777) == 0600);
  assert(save_cache(dir + "/cache", a.id, Kind::movies, "4", catalog));
  iptv::Catalog saved;
  assert(load_cache(dir + "/cache", a.id, Kind::movies, "4", saved) &&
         saved.entries.size() == 1);
  assert(!load_cache(dir + "/cache", "two", Kind::movies, "4", saved));
  assert(!load_cache(dir + "/other-profile", a.id, Kind::movies, "4", saved));
  for (const auto &f : std::filesystem::directory_iterator(dir + "/cache")) {
    assert(stat(f.path().c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
    std::ifstream in(f.path());
    std::string body((std::istreambuf_iterator<char>(in)), {});
    assert(body.find("p?ss") == std::string::npos &&
           body.find("password") == std::string::npos &&
           body.find("direct_source") == std::string::npos);
  }
  auto error_catalog = catalog;
  error_catalog.error = "offline";
  assert(!save_cache(dir + "/cache", a.id, Kind::movies, "4", error_catalog));
  assert(load_cache(dir + "/cache", a.id, Kind::movies, "4", saved) &&
         saved.entries[0].name == "Film");
  stop = true;
  assert(!save_cache(dir + "/cache", a.id, Kind::movies, "4", catalog, &stop));
  stop = false;
  assert(load_cache(dir + "/cache", a.id, Kind::movies, "4", saved));
  state["progress"][identity(a.id, Kind::movies, "19")] = {{"time", 3}};
  reconcile(state, {});
  assert(!available(state, list) && state["progress"].empty());
  remove_cache(dir + "/cache", a.id);
  assert(!load_cache(dir + "/cache", a.id, Kind::movies, "4", saved));
  // Large per-category metadata is scanned one item at a time; nested fields
  // are discarded.
  std::string large = "[";
  for (size_t i = 0; i < 60000; ++i) {
    if (i)
      large += ',';
    large += "{\"stream_id\":" + std::to_string(i) + ",\"name\":\"Movie " +
             std::to_string(i) + "\",\"unused\":{\"large\":\"" +
             std::string(128, 'x') + "\"}}";
  }
  large += ']';
  assert(large.size() > (8u << 20));
  auto big = parse_catalog(large, Kind::movies);
  assert(big.error.empty() && big.entries.size() == 60000);
  std::string deep =
      "[{\"stream_id\":1,\"name\":\"X\",\"ignored\":" + std::string(70, '[') +
      "0" + std::string(70, ']') + "}]";
  assert(!parse_catalog(deep, Kind::movies).error.empty());
  Request private_art = [](const std::string &url, size_t,
                           const std::atomic<bool> *) {
    assert(url.find("action=get_vod_streams&category_id=4") !=
           std::string::npos);
    HttpResponse r;
    r.status = 200;
    r.body =
        R"([{"stream_id":19,"name":"Film","stream_icon":"https://art.invalid/a%2Fb/p%3Fss/p.jpg"}])";
    return r;
  };
  assert(fetch(a, Kind::movies, "4", &stop, private_art)
             .entries[0]
             .poster.empty());
  std::string overflow = "[";
  for (size_t i = 0; i <= max_items; ++i) {
    if (i)
      overflow += ',';
    overflow += "{\"stream_id\":" + std::to_string(i) + ",\"name\":\"M\"}";
  }
  overflow += ']';
  auto over = parse_catalog(overflow, Kind::movies);
  assert(!over.error.empty() && over.entries.empty());
  {
    std::ofstream file(dir + "/bad-state.json");
    file << "{bad";
  }
  assert(load_private(dir + "/bad-state.json").empty());
  auto first = catalog, second = catalog;
  first.entries[0].name = "First";
  second.entries[0].name = "Second";
  std::thread writer_one([&] {
    for (int i = 0; i < 10; ++i)
      assert(save_cache(dir + "/concurrent", a.id, Kind::movies, "4", first));
  });
  std::thread writer_two([&] {
    for (int i = 0; i < 10; ++i)
      assert(save_cache(dir + "/concurrent", a.id, Kind::movies, "4", second));
  });
  writer_one.join();
  writer_two.join();
  assert(load_cache(dir + "/concurrent", a.id, Kind::movies, "4", saved));
  assert(saved.entries.size() == 1 && (saved.entries[0].name == "First" ||
                                       saved.entries[0].name == "Second"));
  std::cout << "IPTV model: discovery, live-only hiding, outage retention, "
               "namespaced playback, lazy episodes, isolated private caches, "
               "cancellation and 60k catalog passed\n";
}
