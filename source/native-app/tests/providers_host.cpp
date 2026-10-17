#include "providers.h"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
int main(int argc, char **argv) {
  assert(argc >= 2);
  assert(providers::initialize(argv[1]));
  std::string error;
  const std::string script = R"JS(
 const cheerio=require('cheerio');const CryptoJS=require('crypto-js');
 module.exports.getStreams=async(id,type,season,episode)=>{
  const $=cheerio.load('<div class="title">Test &amp; text</div>');
  await new Promise(resolve=>setTimeout(resolve,5));
  const u=new URL('/video','https://example.com/root');u.searchParams.set('id',id);
  if(CryptoJS.SHA256('abc').toString()!=='ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')throw Error('crypto');
  const data=CryptoJS.AES.encrypt('hello','secret').toString();
  if(CryptoJS.AES.decrypt(data,'secret').toString(CryptoJS.enc.Utf8)!=='hello')throw Error('aes');
  if(atob(btoa('abc'))!=='abc' || typeof process!=='undefined')throw Error('globals');
  return [{url:u.href,name:$('.title').text(),title:[type,season,episode,getSetting('test','fallback')].join(' '),behaviorHints:{proxyHeaders:{request:{Referer:'https://example.com/'}}}}];
 };
 )JS";
  auto v = providers::execute(script, json::array({"10378", "tv", 1, 2}),
                              {{"test", "setting"}}, error);
  if (!error.empty()) {
    std::cerr << error << '\n';
    return 1;
  }
  assert(v.size() == 1);
  assert(v[0]["url"] == "https://example.com/video?id=10378");
  assert(v[0]["name"] == "Test & text");
  auto streams = parse_streams({{"streams", v}}, "fixture");
  assert(streams.size() == 1);
  assert(streams[0].request_headers.size() == 1);
  auto result =
      providers::execute("module.exports.getStreams=async()=>{while(true){}}",
                         json::array(), {}, error, nullptr, 500);
  assert(!error.empty());
  result = providers::execute(
      "module.exports.getStreams=async()=>new Promise(()=>{})", json::array(),
      {}, error, nullptr, 500);
  assert(error.find("timed out") != std::string::npos);
  std::atomic<bool> cancel = false;
  std::thread t([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    cancel = true;
  });
  result =
      providers::execute("module.exports.getStreams=async()=>{while(true){}}",
                         json::array(), {}, error, &cancel);
  t.join();
  assert(!error.empty());
  result = providers::execute("require('fs');module.exports.getStreams=()=>[]",
                              json::array(), {}, error);
  assert(error.find("Unsupported provider module: fs") != std::string::npos);
  int server = socket(AF_INET, SOCK_STREAM, 0);
  assert(server >= 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(bind(server, reinterpret_cast<sockaddr *>(&address),
              sizeof(address)) == 0);
  assert(listen(server, 1) == 0);
  socklen_t len = sizeof(address);
  assert(getsockname(server, reinterpret_cast<sockaddr *>(&address), &len) ==
         0);
  auto blocked = "module.exports.getStreams=async()=>{try {await "
                 "fetch('http://127.0.0.1:" +
                 std::to_string(ntohs(address.sin_port)) +
                 "/');throw "
                 "Error('allowed')}catch(e){if(String(e).includes('allowed'))"
                 "throw e;return []}}";
  result = providers::execute(blocked, json::array(), {}, error);
  assert(error.empty() && result.empty());
  pollfd incoming{server, POLLIN, 0};
  assert(poll(&incoming, 1, 0) == 0);
  close(server);
  result = providers::execute("module.exports.getStreams=()=>({wrong:true})",
                              json::array(), {}, error);
  assert(!error.empty());
  result = providers::execute(
      "module.exports.getStreams=()=>{setTimeout(()=>{},40000);return []}",
      json::array(), {}, error);
  assert(error.empty());
  result = providers::execute(
      "module.exports.getStreams=()=>{try{Atomics.wait(new Int32Array(new "
      "SharedArrayBuffer(4)),0,0,10);throw Error('blocking "
      "allowed')}catch(e){if(String(e).includes('blocking allowed'))throw "
      "e;}return []}",
      json::array(), {}, error);
  assert(error.empty());
  Addon a;
  assert(parse_addon(
      {{"name", "Fixture"},
       {"scrapers", json::array({{{"id", "sample"},
                                  {"name", "Sample"},
                                  {"filename", "providers/sample.js"},
                                  {"supportedTypes", {"movie", "tv"}}}})}},
      "https://example.com/manifest.json", a));
  assert(a.provider_repository && !a.supports("stream", "movie", "tt1254207"));
  a.providers[0].enabled = true;
  assert(a.supports("stream", "series", "tt1254207:1:2"));
  assert(!a.supports("catalog", "movie", ""));
  assert(parse_meta({{"moviedb_id", 10378}}).tmdb_id == "10378");
  assert(parse_meta({{"tmdbId", "10378"}}).tmdb_id == "10378");
  assert(parse_meta({{"tmdb_id", "bad"}}).tmdb_id.empty());
  auto episode_id = providers::identify("tt1234567:2:3", Meta{});
  assert(episode_id.imdb == "tt1234567" && episode_id.season == 2 &&
         episode_id.episode == 3);
  episode_id = providers::identify("tmdb:10378:1:2", Meta{});
  assert(episode_id.tmdb == "10378" && episode_id.season == 1 &&
         episode_id.episode == 2);
  Meta custom;
  custom.imdb_id = "tt1234567";
  Video video;
  video.id = "custom-episode";
  video.season = 2;
  video.episode = 5;
  custom.videos = {video};
  episode_id = providers::identify("custom-episode", custom);
  assert(episode_id.imdb == custom.imdb_id && episode_id.season == 2 &&
         episode_id.episode == 5);
  auto jobs = providers::stream_sources(a, "series", "tt1254207:1:2");
  assert(jobs.size() == 1 && jobs[0].providers.size() == 1 &&
         jobs[0].manifest.is_null());
  // Load real-world scripts without executing their remote requests, when
  // supplied.
  for (int i = 2; i < argc; ++i) {
    std::ifstream f(argv[i]);
    std::string code((std::istreambuf_iterator<char>(f)), {});
    assert(!code.empty());
    code += "\nmodule.exports={getStreams:async()=>[]};";
    providers::execute(code, json::array(), {}, error);
    if (!error.empty()) {
      std::cerr << "Compatibility load failed: " << argv[i] << ": " << error
                << '\n';
      return 1;
    }
  }
  std::cout << "Provider runtime, parsing, crypto, timers, cancellation, "
               "isolation and manifest tests passed\n";
}
