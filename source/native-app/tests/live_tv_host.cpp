#include "http.h"
#include "live_tv.h"
#include <cassert>
#include <iostream>
const char *kUserAgent = "Maxd-LiveTV-Test";
void http_setup_handle(void *) {}
const std::string &http_ca_bundle() {
  static std::string s;
  return s;
}
int main(int argc, char **argv) {
  using namespace live_tv;
  assert(http_url("https://example.test:8443/list.m3u?token=abc"));
  assert(!http_url("file:///etc/passwd"));
  assert(!http_url("http://"));
  assert(!http_url("https://a/\r\nSecret: x"));
  auto r = parse(
      "\xef\xbb\xbf#EXTM3U\r\n#EXT-X-SESSION-DATA:DATA-ID=\"com.example."
      "playlist\",VALUE=\"1\"\r\n#EXTINF:-1 tvg-id=\"news\" "
      "group-title=\"News, "
      "world\",News HD\r\n#EXTVLCOPT:http-user-agent=TV "
      "client\r\n#EXTVLCOPT:http-referrer=https://example.test/\r\n../news/"
      "live.m3u8\r\n#EXTINF:-1,Duplicate\r\n../news/"
      "live.m3u8\n#EXTINF:-1,Bad\nfile:///tmp/x\n",
      "https://example.test/lists/main.m3u");
  assert(r.error.empty() && r.channels.size() == 1);
  assert(r.channels[0].url == "https://example.test/news/live.m3u8");
  assert(r.channels[0].group == "News, world" && r.channels[0].id == "news");
  assert(r.channels[0].headers.size() == 2 &&
         r.channels[0].headers[0] == "User-Agent: TV client");
  assert(!parse("<html>error</html>", "https://example.test/").error.empty());
  assert(
      !parse("#EXTM3U\n#EXT-X-TARGETDURATION:10\n#EXTINF:10,Segment\nfile.ts",
             "https://example.test/")
           .error.empty());
  assert(!parse("#EXTM3U\n#EXT-X-SESSION-DATA:DATA-ID=\"example\"\n#EXT-X-"
                "STREAM-INF:BANDWIDTH=1000000\nvariant.m3u8",
                "https://example.test/")
              .error.empty());
  assert(!parse(std::string(max_bytes + 1, 'a'), "https://example.test/")
              .error.empty());
  r = parse_xtream(
      R"([{"stream_id":15,"name":"News","category_id":"1","direct_source":"http://wrong.test/"},{"stream_id":"../bad","name":"Bad"},{"stream_id":15,"name":"Duplicate"}])",
      R"([{"category_id":1,"category_name":"News channels"}])",
      "https://example.test:8443", "a/b", "p?ss");
  assert(r.error.empty() && r.channels.size() == 1 &&
         r.channels[0].group == "News channels");
  assert(r.channels[0].url ==
         "https://example.test:8443/live/a%2Fb/p%3Fss/15.ts");
  assert(!parse_xtream("{}", "[]", "https://example.test", "u", "p")
              .error.empty());
  assert(!parse_xtream("[]", "[]", "https://example.test?leak=1", "u", "p")
              .error.empty());
  // Do not silently accept partial lists after syntax, nesting or capacity failures.
  assert(!parse_xtream("[{'bad':1}]", "[]", "https://example.test", "u", "p").error.empty());
  assert(!parse_xtream("[[[{}]]]", "[]", "https://example.test", "u", "p").error.empty());
  assert(!parse_xtream(std::string(80, '[')+std::string(80, ']'), "[]", "https://example.test", "u", "p").error.empty());
  r=parse_xtream(R"([{"stream_id":1,"name":"A","ignored":{"stream_id":2,"name":"Wrong"}},{"stream_id":2,"name":"B"},null,7])", "[]", "https://example.test", "u", "p");
  assert(r.error.empty() && r.channels.size()==2 && r.channels[0].name=="A");
  std::string full="[";
  for(size_t i=0;i<=max_channels;++i) {
    if(i)full+=",";
    full+="{\"stream_id\":"+std::to_string(i)+",\"name\":\"Channel\"}";
  }
  r=parse_xtream(full.substr(0,full.rfind(",{"))+"]", "[]", "https://example.test", "u", "p");
  assert(r.error.empty() && r.channels.size()==max_channels);
  r=parse_xtream(full+"]", "[]", "https://example.test", "u", "p");
  assert(!r.error.empty() && r.channels.empty());
  std::atomic<bool> cancelled{true};
  assert(!parse_xtream(full+"]", "[]", "https://example.test", "u", "p", "ts", &cancelled).error.empty());
  assert(!parse("#EXTM3U\n", "https://example.test", &cancelled).error.empty());
  if (argc > 1) {
    std::string base = argv[1];
    std::atomic<bool> cancel{false};
    r = fetch(base + "/redirect", &cancel);
    assert(r.error.empty() && r.channels.size() == 1);
    assert(r.channels[0].url == base + "/channels/news.m3u8");
    r = fetch(base + "/oversized", &cancel);
    assert(r.error.find("128 MB")!=std::string::npos);
    r = fetch(base + "/large.m3u", &cancel);
    assert(r.error.empty() && r.channels.size()==60000 && r.channels.back().name=="Channel 60000");
    r = fetch_xtream(base+"/large", "user &", "secret/?", &cancel);
    assert(r.error.empty() && r.channels.size()==60000 && r.channels.back().group=="News");
    assert(r.channels.back().url==base+"/large/live/user%20%26/secret%2F%3F/60000.ts");
    r = fetch_xtream(base, "user &", "secret/?", &cancel);
    assert(r.error.empty() && r.channels.size() == 1);
    assert(r.channels[0].url.ends_with(".m3u8"));
    r = fetch_xtream(base, "wrong", "do-not-echo", &cancel);
    assert(!r.error.empty() &&
           r.error.find("do-not-echo") == std::string::npos);
    cancel = true;
    r = fetch(base + "/playlist", &cancel);
    assert(!r.error.empty());
  }
  std::cout << "Live TV parser, Xtream API, headers, bounds, redirects, "
               "credentials and cancellation passed\n";
}
