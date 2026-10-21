#include "trailers.h"
#include <algorithm>
#include <cctype>
namespace trailers {
namespace {
bool id_valid(const std::string& s) {
 return s.size()==11 && std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-';});
}
std::string host(const std::string& u) {
 if(u.rfind("https://",0)!=0 || std::any_of(u.begin(),u.end(),[](unsigned char c){return c<=32||c==127;}))return {};
 const auto authority=u.substr(8,u.find_first_of("/?#",8)-8);
 if(authority.empty()||authority.find('@')!=std::string::npos||authority.find('\\')!=std::string::npos)return {};
 return lower(authority);
}
bool youtube_media(const std::string& u) {
 const auto h=host(u);
 return h=="googlevideo.com" || (h.size()>16 && h.ends_with(".googlevideo.com")) || h=="manifest.googlevideo.com";
}
}
bool https_url(const std::string& url) {return !host(url).empty();}
std::string youtube_id(const std::string& value) {
 if(id_valid(value))return value;
 const auto h=host(value);
 std::string id;
 if(h=="youtu.be")id=value.substr(17,value.find_first_of("?#",17)-17);
 else if(h=="www.youtube.com"||h=="youtube.com"||h=="m.youtube.com"||h=="www.youtube-nocookie.com") {
  const auto path=value.find('/',8);
  if(path==std::string::npos)return {};
  if(value.compare(path,7,"/watch?")==0) {
   const auto query=value.substr(path+7);size_t at=0;
   while(at<query.size()) {auto end=query.find('&',at);auto part=query.substr(at,end-at);if(part.rfind("v=",0)==0){id=part.substr(2);break;}if(end==std::string::npos)break;at=end+1;}
  } else for(const auto& prefix:{"/embed/","/shorts/"})if(value.compare(path,std::char_traits<char>::length(prefix),prefix)==0)id=value.substr(path+std::char_traits<char>::length(prefix));
  id=id.substr(0,id.find_first_of("?&#"));
 }
 return id_valid(id)?id:"";
}
json tmdb_videos(const Meta& meta,const std::string& key,const std::atomic<bool>* cancel,const Request& injected) {
  if(key.empty() || (meta.type!="movie" && meta.type!="series"))return json::object();
  const auto get=[&](const std::string& path) {
    if(cancel && *cancel)return json::object();
    const auto url="https://api.themoviedb.org/3/"+path+"api_key="+key;
    const auto response=injected?injected(url,nullptr):http_provider_request(url,nullptr,"",{},cancel);
    return response.ok()?json::parse(response.body,nullptr,false):json::object();
  };
  auto id=meta.tmdb_id;const auto kind=meta.type=="movie"?"movie":"tv";
  if(id.empty()) {
    auto imdb=meta.imdb_id.empty()?meta.id:meta.imdb_id;
    if(imdb.size()<3 || imdb.size()>20 || imdb.rfind("tt",0)!=0 || imdb.find_first_not_of("0123456789",2)!=std::string::npos)return json::object();
    auto found=get("find/"+imdb+"?external_source=imdb_id&");
    const auto& rows=jobj(found,meta.type=="movie"?"movie_results":"tv_results");
    if(!rows.is_array() || rows.size()!=1)return json::object();
    const auto& value=jobj(rows[0],"id");
    if(value.is_number_integer())id=std::to_string(value.get<long long>());
  }
  if(id.empty() || id.size()>12 || id.find_first_not_of("0123456789")!=std::string::npos)return json::object();
  return get(std::string(kind)+"/"+id+"/videos?");
}
std::vector<Candidate> candidates(const Meta& meta,const json& tmdb) {
 struct Ranked {Candidate c;int rank;};std::vector<Ranked> ranked;
 const auto add=[&](const json& row,bool from_tmdb) {
  if(!row.is_object())return;
  const auto type=lower(jstr(row,"type","trailer"));
  if(type!="trailer"&&type!="teaser")return;
  Candidate c;c.name=jstr(row,"name",jstr(row,"title",type=="teaser"?"Teaser":"Trailer"));
  if(c.name.size()>160)c.name.resize(160);
  if(from_tmdb) {if(lower(jstr(row,"site"))!="youtube")return;c.youtube_id=youtube_id(jstr(row,"key"));}
  else {
   const auto source=jstr(row,"ytId",jstr(row,"source",jstr(row,"url")));
   c.youtube_id=youtube_id(source);
   if(c.youtube_id.empty()&&https_url(source)) {
    const auto path=lower(source.substr(0,source.find_first_of("?#")));
    if(path.ends_with(".mp4")||path.ends_with(".m3u8")||path.ends_with(".webm"))c.url=source;
   }
  }
  if(c.youtube_id.empty()&&c.url.empty())return;
  int rank=(type=="trailer"?100:0)+(jbool(row,"official")?20:0)+(jstr(row,"iso_639_1")=="en"?10:0);
  ranked.push_back({c,rank});
 };
 const auto& results=jobj(tmdb,"results");if(results.is_array())for(const auto& r:results)add(r,true);
 if(meta.trailers.is_array())for(const auto& r:meta.trailers)add(r,false);
 std::stable_sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){return a.rank>b.rank;});
 std::vector<Candidate> out;
 for(const auto& r:ranked) {
  if(out.size()==12)break;
  if(std::none_of(out.begin(),out.end(),[&](const auto& c){return c.youtube_id==r.c.youtube_id&&c.url==r.c.url;}))out.push_back(r.c);
 }
 return out;
}
json embedded_player(const std::string& html) {
 // Parse one balanced JSON value; never execute scripts from a watch page.
 for(const auto* marker:{"ytInitialPlayerResponse =","ytInitialPlayerResponse="}) {
  auto at=html.find(marker);if(at==std::string::npos)continue;
  at=html.find('{',at+std::char_traits<char>::length(marker));if(at==std::string::npos)continue;
  bool quoted=false,escaped=false;int depth=0;
  for(size_t i=at;i<html.size()&&i-at<8*1024*1024;++i) {
   const char c=html[i];
   if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}
   if(c=='"')quoted=true;else if(c=='{')++depth;else if(c=='}'&&--depth==0)return json::parse(html.substr(at,i-at+1),nullptr,false);
  }
 }
 return {};
}
Playback player_source(const json& response) {
 Playback result;
 if(jstr(jobj(response,"playabilityStatus"),"status")!="OK")return {{},"YouTube did not offer public playback for this trailer.",{}};
 const auto& streaming=jobj(response,"streamingData");
 // A multiplexed MP4 keeps picture and sound together on all three players.
 int best=-1;
 const auto& formats=jobj(streaming,"formats");
 if(formats.is_array())for(const auto& f:formats) {
  const auto url=jstr(f,"url");const auto mime=lower(jstr(f,"mimeType"));
  if(!youtube_media(url)||mime.rfind("video/mp4",0)!=0||mime.find("avc1")==std::string::npos||mime.find("mp4a")==std::string::npos)continue;
  const double height=jnum(f,"height",0);if(!(height>0&&height<=1080)||height<=best)continue;
  best=int(height);result.url=url;
 }
 if(result.url.empty()) {
  auto hls=jstr(streaming,"hlsManifestUrl");if(youtube_media(hls))result.url=hls;
 }
 if(result.url.empty())result.error="This trailer needs a YouTube playback method that is not available here. Try another trailer.";
 return result;
}
Playback resolve(const Candidate& candidate,const std::atomic<bool>* cancel,const Request& injected) {
 const auto stopped=[&]{return cancel&&cancel->load();};
 if(stopped())return {{},"cancelled",{}};
 if(!candidate.url.empty())return https_url(candidate.url)?Playback{candidate.url,{}, {}}:Playback{{},"Invalid trailer URL",{}};
 if(!id_valid(candidate.youtube_id))return {{},"Invalid trailer identity",{}};
 const auto request=[&](const std::string& url,const std::string* body) {
  if(stopped())return HttpResponse{};
  if(injected)return injected(url,body);
  if(body)return http_post_json(url,*body,10,cancel,{"User-Agent: com.google.ios.youtube/20.10.1 (iPhone16,2; U; CPU iOS 17_4 like Mac OS X)","X-YouTube-Client-Name: 5","X-YouTube-Client-Version: 20.10.1"});
  return http_get_https(url,10,cancel,{"User-Agent: Mozilla/5.0"});
 };
 const auto page=request("https://www.youtube.com/watch?v="+candidate.youtube_id,nullptr);
 auto result=page.ok()?player_source(embedded_player(page.body)):Playback{};
 if(!result.url.empty()||stopped()) {
  if(stopped())return {{},"cancelled",{}};
  result.headers={"User-Agent: Mozilla/5.0","Referer: https://www.youtube.com/"};return result;
 }
 // Public iOS player responses can provide muxed video or an HLS manifest.
 // No login cookies, remote resolver service, script evaluation or cipher bypass.
 const json body{{"videoId",candidate.youtube_id},{"context",{{"client",{{"clientName","IOS"},{"clientVersion","20.10.1"},{"hl","en"}}}}}};
 const auto encoded=body.dump();const auto response=request("https://www.youtube.com/youtubei/v1/player",&encoded);
 result=response.ok()?player_source(json::parse(response.body,nullptr,false)):Playback{{},"The trailer service is unavailable. Try again later.",{}};
 if(stopped())return {{},"cancelled",{}};
 if(!result.url.empty())result.headers={"User-Agent: com.google.ios.youtube/20.10.1 (iPhone16,2; U; CPU iOS 17_4 like Mac OS X)","Referer: https://www.youtube.com/"};
 return result;
}
} // namespace trailers
