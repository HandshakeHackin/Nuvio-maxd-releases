// Exercise the production request/download/execute path with an in-process
// curl transfer fixture. No external sites or console are needed.
#include "http.h"
#include <curl/curl.h>
#include <cassert>
#include <cstdarg>
#include <iostream>
#include <map>
#include <fstream>
#include <unistd.h>

struct FixtureTransfer {
  curl_write_callback write = nullptr, header = nullptr;
  void *body = nullptr, *headers = nullptr;
  curl_opensocket_callback open = nullptr;
  void *open_data = nullptr;
  std::string url;
  bool setup = false;
};
static std::map<CURL *, FixtureTransfer> transfers;
static long reply_status = 200;
static CURLcode reply_error = CURLE_OK;
static bool redirect = false;
static int ipv4_setups = 0;
extern "C" CURLcode __real_curl_easy_setopt(CURL *, CURLoption, ...);
extern "C" CURLcode __wrap_curl_easy_setopt(CURL *c, CURLoption opt, ...) {
  // This sees the real shared http.cpp setup; the isolated transfer options
  // below are captured separately. A regression to an empty host setup fails.
  assert(opt == CURLOPT_IPRESOLVE);
  va_list args; va_start(args, opt); const long value = va_arg(args, long); va_end(args);
  assert(value == CURL_IPRESOLVE_V4); ++ipv4_setups;
  return __real_curl_easy_setopt(c, opt, value);
}
static std::string script;
static int metadata_status=200;
static int metadata_calls=0;
static CURLcode fixture_setopt(CURL *c, CURLoption opt, ...);
static CURLcode fixture_perform(CURL *c);
static CURLcode fixture_getinfo(CURL *c, CURLINFO info, ...);
static void fixture_setup(void *c) {
  http_setup_handle(c);
  transfers[static_cast<CURL *>(c)].setup = true;
}
#undef curl_easy_setopt
#undef curl_easy_getinfo
#define curl_easy_setopt fixture_setopt
#define curl_easy_perform fixture_perform
#define curl_easy_getinfo fixture_getinfo
#define http_setup_handle fixture_setup
#include "../src/providers.cpp"
#undef http_setup_handle
#undef curl_easy_getinfo
#undef curl_easy_perform
#undef curl_easy_setopt

static CURLcode fixture_setopt(CURL *c, CURLoption opt, ...) {
  va_list args; va_start(args, opt);
  auto &t = transfers[c];
  switch (opt) {
  case CURLOPT_URL: t.url = va_arg(args, const char *); break;
  case CURLOPT_WRITEFUNCTION: t.write = va_arg(args, curl_write_callback); break;
  case CURLOPT_HEADERFUNCTION: t.header = va_arg(args, curl_write_callback); break;
  case CURLOPT_WRITEDATA: t.body = va_arg(args, void *); break;
  case CURLOPT_HEADERDATA: t.headers = va_arg(args, void *); break;
  case CURLOPT_OPENSOCKETFUNCTION: t.open = va_arg(args, curl_opensocket_callback); break;
  case CURLOPT_OPENSOCKETDATA: t.open_data = va_arg(args, void *); break;
  default: break; // Fixture doesn't use the remaining curl transfer options.
  }
  va_end(args); return CURLE_OK;
}
static CURLcode fixture_getinfo(CURL *, CURLINFO info, ...) {
  assert(info == CURLINFO_RESPONSE_CODE);
  va_list args; va_start(args, info); *va_arg(args, long *) = reply_status;
  va_end(args); return CURLE_OK;
}
static CURLcode fixture_perform(CURL *c) {
  auto &t=transfers.at(c);assert(t.setup && t.open && t.body);
  std::string body;reply_status=200;
  if(t.url.find("fixture.example/providers/")!=std::string::npos)body=script;
  else if(t.url.find("https://api.themoviedb.org/")==0) {
    ++metadata_calls;assert(t.url.find("api_key=11111111111111111111111111111111")!=std::string::npos);
    reply_status=metadata_status;
    body=R"({"title":"Fixture","original_title":"Fixture","release_date":"2000-01-01","external_ids":{"imdb_id":"tt123"},"translations":{"translations":[]},"alternative_titles":{"titles":[]}})";
  } else if(t.url.find("moflix-stream.xyz/search/")!=std::string::npos)
    body=R"(<script>window.bootstrapData = {"loaders":{"searchPage":{"results":[{"model_type":"title","is_series":false,"id":1,"name":"Fixture","year":2000,"tmdb_id":1}]}}};</script>)";
  else if(t.url.find("moflix-stream.xyz/api/")!=std::string::npos)
    body=R"({"title":{"videos":[{"src":"https://cdn.fixture.invalid/video.m3u8","category":"full","name":"Free","type":"stream","language":"de","quality":"1080p"}]}})";
  else if(t.url.find("huhu.to/mediaurl-source.json")!=std::string::npos)
    body=R"([{"type":"url","url":"https://embed.fixture.invalid/video","languages":["de"],"tag":"1080p"}])";
  else if(t.url.find("filmpalast.to/search/title/")!=std::string::npos)
    body=R"(<div id="content"><div class="glowliste"><h2><a title="Fixture" href="https://filmpalast.to/fixture">Fixture</a></h2></div></div>)";
  else if(t.url=="https://filmpalast.to/fixture")
    body=R"(<div id="content"><ul class="currentStreamLinks"><span class="hostName">Fixture</span><a class="iconPlay" data-player-url="https://embed.fixture.invalid/video">Play</a></ul></div>)";
  else if(t.url.find("meinecloud.click/movie/")!=std::string::npos)
    body=R"(<script>type: 'movie', id: "fixture", token: "fixture"</script>)";
  else if(t.url=="https://meinecloud.click/api/embed-links")
    body=R"({"sources":[{"url":"https://embed.fixture.invalid/video"}]})";
  else if(t.url=="https://embed.fixture.invalid/video")
    body=R"(<script>player = {sources: [{file:"https://cdn.fixture.invalid/video.m3u8", label:"1080p"}]};</script>)";
  else {std::cerr<<"Unexpected fixture request\n";assert(false);}
  t.write(body.data(),1,body.size(),t.body);return CURLE_OK;
}
int main(int argc,char** argv) {
  assert(argc==6 && providers::initialize(argv[1]));
  const auto path="/tmp/nuvio-provider-apps-"+std::to_string(getpid())+".json";
  assert(save_json(path,{{"tmdb",{{"api_key","11111111111111111111111111111111"}}}}));
  assert(providers::configure(path));remove(path.c_str());
  for(int i=2;i<argc;++i) {
    std::ifstream f(argv[i]);script.assign(std::istreambuf_iterator<char>(f),{});assert(!script.empty());
    providers::clear_script_cache();
    Addon a;a.provider_repository=true;a.name="Fixture";a.transport_url="https://fixture.example/manifest.json";
    ProviderScript p;p.id="sample";p.name="Sample";p.filename="providers/sample.js";p.enabled=true;a.providers={p};
    auto r=providers::streams(a,"movie","tmdb:1",Meta{},nullptr);
    if(!r.error.empty())std::cerr<<"Provider returned a diagnostic: "<<r.error<<"\n";
    assert(r.error.empty() && r.streams.size()==1);
    assert(r.streams[0].url=="https://cdn.fixture.invalid/video.m3u8");
    assert(!r.streams[0].request_headers.empty());
    metadata_status=401;r=providers::streams(a,"movie","tmdb:1",Meta{},nullptr);
    assert(r.streams.empty() && r.error.find("metadata authorization was rejected")!=std::string::npos);
    metadata_status=200;
    std::cout<<"German provider fixture "<<(i-1)<<": stream and playback headers returned; metadata rejection reported\n";
  }
  assert(metadata_calls==8);
}
