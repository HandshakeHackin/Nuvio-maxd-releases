// Exercise the production request/download/execute path with an in-process
// curl transfer fixture. No external sites or console are needed.
#include "http.h"
#include <curl/curl.h>
#include <cassert>
#include <cstdarg>
#include <iostream>
#include <map>

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
static const std::string script = "module.exports.getStreams=async()=>[{url:'https://example.com/video.mp4',name:'fixture'}]";
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
  auto &t = transfers.at(c);
  assert(t.setup && t.open && t.open_data && t.body && t.headers);
  assert(t.url == "https://example.com/repo/providers/sample.js?secret=hidden");
  if (reply_error != CURLE_OK) return reply_error;
  if (redirect) {
    std::string h = "Location: http://example.com/insecure.js\r\n";
    t.header(h.data(), 1, h.size(), t.headers);
  }
  t.write(const_cast<char *>(script.data()), 1, script.size(), t.body);
  return CURLE_OK;
}
int main(int argc, char **argv) {
  assert(argc == 2 && providers::initialize(argv[1]));
  Addon a;
  assert(parse_addon({{"name", "Fixture"}, {"scrapers", json::array({
    {{"id", "sample"}, {"name", "Sample"}, {"filename", "providers/sample.js?secret=hidden"}}
  })}}, "https://example.com/repo/manifest.json", a));
  a.providers[0].enabled = true;
  const auto run = [&] { return providers::streams(a, "movie", "tmdb:10378", Meta{}, nullptr); };
  auto r = run();
  assert(ipv4_setups == 1);
  assert(r.error.empty() && r.streams.size() == 1);
  assert(r.streams[0].url == "https://example.com/video.mp4");
  r=run();assert(ipv4_setups==1); // repeated searches reuse the downloaded script
  providers::clear_script_cache();
  reply_status = 404; r = run();
  assert(r.streams.empty() && r.error.find("Sample:") != std::string::npos && r.error.find("HTTP 404") != std::string::npos);
  reply_status = 0; reply_error = CURLE_OPERATION_TIMEDOUT; r = run();
  assert(r.error.find("curl 28") != std::string::npos && r.error.find("secret") == std::string::npos);
  reply_status = 302; reply_error = CURLE_OK; redirect = true; r = run();
  assert(r.error.find("Insecure provider redirect refused") != std::string::npos);
  assert(providers::resolve_url(a.transport_url, "../script.js") == "https://example.com/script.js");
  // Same callback used by script downloads: private addresses remain blocked.
  providers::Execution execution; execution.deadline = providers::Clock::now() + std::chrono::seconds(5);
  providers::Transfer transfer{&execution};
  curl_sockaddr addr{}; addr.family = AF_INET; addr.socktype = SOCK_STREAM;
  auto *ip = reinterpret_cast<sockaddr_in *>(&addr.addr);
  ip->sin_family = AF_INET; ip->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(providers::open_socket(&transfer, CURLSOCKTYPE_IPCXN, &addr) == CURL_SOCKET_BAD && transfer.blocked_address);
  assert(providers::transport_error(CURLE_COULDNT_CONNECT, transfer).find("public address") != std::string::npos);
  std::cout << "Provider download pipeline: script execution, relative URLs, HTTP errors, timeout, redirect and address protection passed\n";
}
