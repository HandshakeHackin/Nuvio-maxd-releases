#include "trailers.h"
#include <cassert>
#include <cstdio>
#include <iostream>
using namespace trailers;
json player(const std::string& url="https://rr1.googlevideo.com/videoplayback?expire=1") {
 return json{{"playabilityStatus",{{"status","OK"}}},{"streamingData",{{"formats",json::array({json{{"url",url},{"mimeType","video/mp4; codecs=\"avc1.42001E, mp4a.40.2\""},{"height",720}}})}}}};
}
HttpResponse answer(const json& data){HttpResponse r;r.status=200;r.body=data.dump();return r;}
int main(int argc,char** argv) {
 if(argc==3 && std::string(argv[1])=="--live") {
  http_init("");auto result=resolve({"Trailer",argv[2],""});
  std::cout<<"Public YouTube resolution: "<<(result.url.empty()?"unavailable":"playable source returned")<<"\n";
  if(result.url.empty())std::cout<<result.error<<"\n";
  return result.url.empty()?2:0;
 }
 const std::string id="zSWdZVtXT7E";
 assert(youtube_id(id)==id && youtube_id("https://youtu.be/"+id+"?si=abc")==id);
 assert(youtube_id("https://www.youtube.com/watch?feature=share&v="+id)==id);
 assert(youtube_id("https://www.youtube.com/embed/"+id)==id);
 assert(youtube_id("https://youtube.com.evil/watch?v="+id).empty());
 assert(youtube_id("https://youtube.com@evil/watch?v="+id).empty());
 assert(!https_url("file:///tmp/a.mp4")&&!https_url("https://a/\r\nX: y")&&!https_url("https://user:pass@a/video.mp4"));
 assert(!records_history(true,"movie")&&!records_history(true,"series")&&!records_history(false,"tv")&&records_history(false,"movie"));
 auto meta=parse_meta(json{{"type","movie"},{"id","tt0816692"},{"trailers",json::array({json{{"source",id},{"type","Trailer"}}})},{"trailerStreams",json::array({json{{"url","https://cdn.example/trailer.mp4"},{"name","Direct"}}})}});
 assert(meta.trailers.size()==2);
 const json videos{{"results",json::array({json{{"key",id},{"site","YouTube"},{"type","Trailer"},{"official",true},{"iso_639_1","en"},{"name","Official"}},json{{"key","aaaaaaaaaaa"},{"site","YouTube"},{"type","Clip"}}})}};
 auto found=candidates(meta,videos);assert(found.size()==2&&found[0].name=="Official");
 assert(candidates(Meta{},json::array()).empty());
 auto response=player();assert(!player_source(response).url.empty());
 assert(embedded_player("<script>var ytInitialPlayerResponse = "+response.dump()+";</script>")==response);
 assert(embedded_player("ytInitialPlayerResponse = {\"x\":\"}\\\"{\"}").is_object());
 assert(embedded_player("ytInitialPlayerResponse = {").is_null());
 auto blocked=response;blocked["playabilityStatus"]["status"]="LOGIN_REQUIRED";assert(player_source(blocked).url.empty());
 assert(player_source(player("https://googlevideo.com.evil/x")).url.empty());
 auto oversized=response;oversized["streamingData"]["formats"][0]["height"]=1e100;assert(player_source(oversized).url.empty());
 auto cipher=response;cipher["streamingData"]["formats"][0].erase("url");cipher["streamingData"]["formats"][0]["signatureCipher"]="secret";assert(player_source(cipher).url.empty());
 json hls{{"playabilityStatus",{{"status","OK"}}},{"streamingData",{{"hlsManifestUrl","https://manifest.googlevideo.com/api/manifest/hls_playlist/a"}}}};
 assert(!player_source(hls).url.empty());
 int calls=0;
 Request fallback=[&](const std::string& url,const std::string* body) {
  ++calls;
  if(!body){assert(url=="https://www.youtube.com/watch?v="+id);return HttpResponse{};}
  assert(url=="https://www.youtube.com/youtubei/v1/player"&&json::parse(*body)["videoId"]==id);return answer(response);
 };
 assert(!resolve({"Trailer",id,{}},nullptr,fallback).url.empty()&&calls==2);
 calls=0;std::atomic<bool> changed{false};
 Request obsolete=[&](const std::string&,const std::string*) {++calls;changed=true;return answer(response);};
 assert(resolve({"Trailer",id,{}},&changed,obsolete).error=="cancelled"&&calls==1);
 calls=0;std::atomic<bool> cancel{true};assert(resolve({"Trailer",id,{}},&cancel,fallback).error=="cancelled"&&calls==0);
 assert(!resolve({"Direct",{},"https://cdn.example/trailer.mp4"},nullptr,fallback).url.empty()&&calls==0);
 Request lookup=[&](const std::string& url,const std::string* body) {
  assert(!body);++calls;
  if(calls==1){assert(url.find("find/tt0816692?external_source=imdb_id&api_key=dummy")!=std::string::npos);return answer(json{{"movie_results",json::array({json{{"id",157336}}})}});}
  assert(url=="https://api.themoviedb.org/3/movie/157336/videos?api_key=dummy");return answer(videos);
 };
 assert(tmdb_videos(meta,"dummy",nullptr,lookup)==videos&&calls==2);
 Request ambiguous=[&](const std::string&,const std::string*) {return answer(json{{"movie_results",json::array({json{{"id",1}},json{{"id",2}}})}});};
 assert(tmdb_videos(meta,"dummy",nullptr,ambiguous).empty());
 calls=0;assert(tmdb_videos(meta,"",nullptr,lookup).empty()&&calls==0);
 assert(tmdb_videos(meta,"dummy",&cancel,lookup).empty()&&calls==0);
 meta.id="tt1&injected=true";meta.imdb_id="";assert(tmdb_videos(meta,"dummy",nullptr,lookup).empty()&&calls==0);
 puts("Trailer checks passed: identities, metadata, ranking, deduplication, safe URLs, public player responses, TMDB routing, fallback, cancellation and history isolation");
}
