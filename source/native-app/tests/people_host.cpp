#include "people.h"
#include <cassert>
#include <iostream>
#include <deque>
HttpResponse http_provider_request(const std::string&,const std::string*,const std::string&,const std::vector<std::string>&,const std::atomic<bool>*){std::abort();}
struct Fixture {
 std::deque<json> bodies;std::vector<std::string> paths;std::atomic<bool> cancel=false;bool stop=false;
 people::Request request(){return [this](const std::string& url,const std::atomic<bool>*){
  assert(url.starts_with("https://api.themoviedb.org/3/")&&url.ends_with("api_key="+std::string(32,'a')));paths.push_back(url.substr(32,url.find("api_key=")-32));assert(!bodies.empty());auto j=bodies.front();bodies.pop_front();if(stop)cancel=true;return HttpResponse{200,j.dump(),"","application/json",0};};}
};
const std::string key(32,'a');
int main(){
 Meta m;m.id="tt12345";m.type="movie";Fixture f;f.bodies={json::parse(R"({"movie_results": [{"id": 12}]})"),json::parse(R"({"id": 12, "credits": {"crew": [{"id": 1, "name": "Director", "job": "Director", "profile_path": "/one.jpg"}], "cast": [{"id": 1, "name": "Duplicate", "character": "Also acts"}, {"id": 2, "name": "Actor", "character": "Role", "profile_path": "/two.jpg"}]}})")};
 auto cast=people::fetch_cast(m,key,"de",&f.cancel,f.request());assert(cast.error.empty()&&cast.members.size()==2&&cast.members[0].crew&&cast.members[1].role=="Role");assert(cast.members[0].character=="Also acts");assert(f.paths.size()==2&&f.paths[1].find("language=de")!=std::string::npos);
 m.tmdb_id="42";m.type="series";f.bodies={json::parse(R"({"id":42, "created_by": [{"id": 3, "name": "Creator"}], "aggregate_credits": {"crew": [{"id":5,"name":"TV Director","jobs":[{"job":"Director"}]}], "cast": [{"id": 4, "name": "TV Actor", "roles": [{"character": "Detective"}]}]}})")};
 cast=people::fetch_cast(m,key,"en",nullptr,f.request());assert(cast.members.size()==3&&cast.members[0].crew&&cast.members[2].role=="Detective");
 auto credits=json::array();for(int n=1;n<=600;++n)credits.push_back({{"id",n},{"media_type",n%2?"movie":"tv"},{"title","Film"},{"name","Show"},{"release_date","2025-01-02"},{"first_air_date","2024-01-01"}});
 credits.push_back({{"id",1},{"media_type","movie"},{"title","Duplicate"}});credits.push_back({{"id",99},{"media_type","person"},{"name","Wrong type"}});
 f.bodies={{{"id",4},{"name","Actor"},{"biography",""},{"birthday","1970-01-02"},{"place_of_birth","Place"},{"profile_path","/photo.jpg"},{"combined_credits",{{"cast",credits}}}},{{"id",4},{"biography","English fallback"}}};
 auto p=people::fetch_person({"4","Actor","","",false},key,"de",nullptr,f.request());assert(p.error.empty()&&p.biography=="English fallback"&&p.movies.size()==256&&p.series.size()==256&&p.birthday=="1970-01-02");
 f.bodies={json::parse(R"({"id": 3, "name": "Director", "combined_credits": {"crew": [{"id": 19, "media_type": "movie", "title": "Directed", "job": "Director"}], "cast": [{"id": 20, "media_type": "movie", "title": "Acted"}]}})")};
 p=people::fetch_person({"3","Director","","",true},key,"en",nullptr,f.request());assert(p.movies.size()==1&&p.movies[0].id=="19");
 f.bodies={{{"id",4},{"name","Wrong person"}}};p=people::fetch_person({"3","Person"},key,"en",nullptr,f.request());assert(!p.error.empty()&&p.name=="Person");
 for(auto id:{"../1","0","-1","1234567890123","1?x=1"}){auto before=f.paths.size();p=people::fetch_person({id,"X"},key,"en",nullptr,f.request());assert(!p.error.empty()&&f.paths.size()==before);}
 assert(people::image("/valid.jpg").starts_with("https://image.tmdb.org/t/p/w185/"));for(auto path:{"https://private.test/a.jpg","/../private.jpg","//private.jpg","/x%2f.jpg","/x?key=secret.jpg","/x.svg"})assert(people::image(path).empty());
 f.bodies={{{"id",19},{"imdb_id","tt8765"}}};auto title=people::resolve({"19","movie","Film"},key,nullptr,f.request());assert(title.id=="tt8765"&&title.type=="movie");
 f.bodies={{{"id",20},{"imdb_id",nullptr}}};title=people::resolve({"20","series","Show"},key,nullptr,f.request());assert(title.id=="tmdb:20"&&title.type=="series");
 f.cancel=true;auto before=f.paths.size();title=people::resolve({"20","series","Show"},key,&f.cancel,f.request());assert(!title.error.empty()&&f.paths.size()==before);f.cancel=false;
 f.stop=true;f.bodies={{{"id",3},{"name","Too late"}}};p=people::fetch_person({"3","Original"},key,"de",&f.cancel,f.request());assert(!p.error.empty()&&p.name=="Original"&&f.bodies.empty());f.cancel=false;f.stop=false;
 before=f.paths.size();p=people::fetch_person({"3","Original"},"", "en",nullptr,f.request());assert(!p.error.empty()&&f.paths.size()==before);
 auto oversized=[](const std::string&,const std::atomic<bool>*){return HttpResponse{200,std::string(people::max_body+1,'x'),"","",0};};p=people::fetch_person({"3","Original"},key,"en",nullptr,oversized);assert(!p.error.empty());
 auto malformed=[](const std::string&,const std::atomic<bool>*){return HttpResponse{200,"{bad", "SECRET URL", "",0};};p=people::fetch_person({"3","Original"},key,"en",nullptr,malformed);assert(p.error.find("SECRET")==std::string::npos&&!p.error.empty());
 json many=json::array();for(int i=1;i<=200;++i)many.push_back({{"id",i},{"name","Actor"}});m.type="movie";f.bodies={{{"id",42},{"credits",{{"cast",many}}}}};cast=people::fetch_cast(m,key,"en",nullptr,f.request());assert(cast.members.size()==people::max_members);
 f.bodies={{{"id",999},{"credits",{{"cast",many}}}}};cast=people::fetch_cast(m,key,"en",nullptr,f.request());assert(cast.members.empty()&&!cast.error.empty());
 f.bodies={{{"id",999},{"imdb_id","tt9876"}}};title=people::resolve({"19","movie","Film"},key,nullptr,f.request());assert(title.id.empty()&&!title.error.empty());
 std::cout<<"People metadata: movie/TV credits, role deduplication, identity mapping, English fallback, bounded replies/lists, image validation and cancellation passed\n";
}
