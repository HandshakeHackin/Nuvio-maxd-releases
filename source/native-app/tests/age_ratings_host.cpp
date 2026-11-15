#include "people.h"
#include <cassert>
#include <iostream>
HttpResponse http_provider_request(const std::string&,const std::string*,const std::string&,const std::vector<std::string>&,const std::atomic<bool>*){std::abort();}
using json=nlohmann::json;
int main() {
 using namespace age_ratings;
 auto movie=json::parse(R"({"id":42,"release_dates":{"results":[
  {"iso_3166_1":"ES","release_dates":[{"type":1,"certification":"18"},{"type":4,"certification":"16"},{"type":3,"certification":" 12 "},{"type":3,"certification":"7"}]},
  {"iso_3166_1":"US","release_dates":[{"type":3,"certification":"PG-13"}]},
  {"iso_3166_1":"GB","release_dates":[{"type":3,"certification":""}]}]}})");
 auto ratings=parse(movie,true);assert(ratings.size()==2&&badge(ratings,"ES")=="ES 12");
 assert(badge(ratings,"GB")=="US PG-13"&&badge(ratings,"US")=="US PG-13");
 auto tv=json::parse(R"({"id":42,"content_ratings":{"results":[{"iso_3166_1":"ES","rating":"16"},{"iso_3166_1":"US","rating":"TV-MA"},{"iso_3166_1":"KR","rating":"15세이상관람가"}]}})");
 ratings=parse(tv,false);assert(badge(ratings,"ES")=="ES 16"&&badge(ratings,"KR")=="KR 15세이상관람가");
 assert(badge(ratings,"GB")=="US TV-MA");
 assert(parse(movie,false).empty()&&parse(tv,true).empty());
 assert(badge({{"ES","12"}},"GB").empty()&&badge({},"US").empty());
 assert(badge({{"US","NR"}},"ES")=="US NR");
 for(const auto& value:{json(),json::array(),json("bad"),json{{"content_ratings",false}},json{{"content_ratings",{{"results",false}}}}})assert(parse(value,false).empty());
 for(const auto& entry:{json(false),json{{"iso_3166_1","USA"},{"rating","18"}},json{{"iso_3166_1","US"},{"rating",18}},json{{"iso_3166_1","US"},{"rating",""}},json{{"iso_3166_1","US"},{"rating","https://private.invalid"}}})
  assert(parse(json{{"content_ratings",{{"results",json::array({entry})}}}},false).empty());
 for(const auto& value:{std::string(""),std::string("  "),std::string(49,'x'),std::string("PG\n13"),std::string("PG\0secret",9),std::string("\xc0\xaf"),std::string("\xed\xa0\x80"),std::string("\xe2\x80\xaePG"),std::string("N/A"),std::string("<18>")})assert(label(value).empty());
 assert(label("普遍級")=="普遍級"&&label("R15+")=="R15+"&&label("TV-Y7-FV")=="TV-Y7-FV");
 auto bad_type=movie;for(auto& release:bad_type["release_dates"]["results"][0]["release_dates"])release["type"]=true;
 assert(badge(parse(bad_type,true),"ES")=="US PG-13");
 auto bounded=tv;bounded["content_ratings"]["results"]=json::array();
 for(size_t i=0;i<max_countries;++i)bounded["content_ratings"]["results"].push_back({{"iso_3166_1","XX"},{"rating",""}});
 bounded["content_ratings"]["results"].push_back({{"iso_3166_1","US"},{"rating","TV-MA"}});assert(parse(bounded,false).empty());
 bounded=movie;auto& releases=bounded["release_dates"]["results"][0]["release_dates"];releases=json::array();
 for(size_t i=0;i<max_releases;++i)releases.push_back({{"type",3},{"certification",""}});
 releases.push_back({{"type",3},{"certification","18"}});assert(badge(parse(bounded,true),"ES")=="US PG-13");
 assert(preference("es")=="ES"&&preference("XX")=="US");
 for(const auto& value:{json(),json{{"age_rating_country",42}},json{{"age_rating_country","ES?private=1"}}})assert(load_preference(value)=="US");
 json a={{"other_setting",true}},b;save_preference(a,"ES");assert(load_preference(a)=="ES"&&load_preference(b)=="US"&&a["other_setting"]==true);
 int requests=0;Meta m;m.id="tmdb:42";m.tmdb_id="42";m.type="movie";
 auto request=[&](const std::string& url,const std::atomic<bool>*){++requests;assert(url.find(m.type=="movie"?"append_to_response=credits,release_dates":"append_to_response=aggregate_credits,content_ratings")!=std::string::npos);return HttpResponse{200,(m.type=="movie"?movie:tv).dump(),"","application/json",0};};
 auto cast=people::fetch_cast(m,std::string(32,'a'),"en",nullptr,request);assert(requests==1&&cast.members.empty()&&badge(cast.certifications,"ES")=="ES 12");
 m.type="series";cast=people::fetch_cast(m,std::string(32,'a'),"es",nullptr,request);assert(requests==2&&badge(cast.certifications,"ES")=="ES 16");
 std::atomic<bool> cancel=true;cast=people::fetch_cast(m,std::string(32,'a'),"es",&cancel,request);assert(requests==2&&cast.certifications.empty());
 auto wrong=[&](const std::string&,const std::atomic<bool>*){auto j=tv;j["id"]=99;return HttpResponse{200,j.dump(),"","application/json",0};};
 cast=people::fetch_cast(m,std::string(32,'a'),"es",nullptr,wrong);assert(cast.certifications.empty());
 std::cout<<"Age ratings: movie/TV parsing, theatrical priority, explicit country/US fallback, absent/malformed data, UTF-8, bounds, no extra requests, identity and cancellation passed\n";
}
