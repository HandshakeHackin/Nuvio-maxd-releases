// SPDX-License-Identifier: GPL-3.0-or-later
#include "people.h"
#include <algorithm>
#include <chrono>
#include <set>
namespace people {
namespace {
std::string text(const json& j,const char* key,size_t limit=240){auto s=jstr(j,key);if(s.size()>limit){s.resize(limit);while(!s.empty()&&(static_cast<unsigned char>(s.back())&0xc0)==0x80)s.pop_back();if(!s.empty()&&static_cast<unsigned char>(s.back())>=0xc0)s.pop_back();}return s;}
std::string id(const json& j){auto n=jobj(j,"id");if(n.is_number_unsigned()){auto s=std::to_string(n.get<uint64_t>());return valid_id(s)?s:"";}if(n.is_number_integer()){auto s=std::to_string(n.get<int64_t>());return valid_id(s)?s:"";}return {};}
bool imdb(const std::string& s){return s.size()>=3&&s.size()<=20&&s.starts_with("tt")&&s.find_first_not_of("0123456789",2)==std::string::npos;}
std::string lang(const std::string& s){return s=="de"?"de":"en";}
struct Fetch {
 const std::string& key;const std::atomic<bool>* cancel;const Request& request;
 std::chrono::steady_clock::time_point end=std::chrono::steady_clock::now()+std::chrono::seconds(25);
 unsigned calls=0;bool failed=false;
 bool stopped()const{return (cancel&&*cancel)||std::chrono::steady_clock::now()>=end;}
 json get(const std::string& path){
  if(stopped()||key.size()!=32||key.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos||++calls>4){failed=true;return {};}
  auto url="https://api.themoviedb.org/3/"+path+(path.find('?')==std::string::npos?"?":"&")+"api_key="+key;
  auto r=request?request(url,cancel):http_provider_request(url,nullptr,"",{},cancel);
  if(!r.ok()||r.body.size()>max_body||stopped()){failed=true;return {};}
  auto j=json::parse(r.body,nullptr,false);if(!j.is_object()){failed=true;return {};}return j;
 }
};
std::string media_id(const Meta& m,Fetch& f){if(valid_id(m.tmdb_id))return m.tmdb_id;auto i=m.imdb_id.empty()?m.id:m.imdb_id;if(!imdb(i))return {};auto j=f.get("find/"+i+"?external_source=imdb_id");auto rows=jobj(j,m.type=="movie"?"movie_results":"tv_results");return rows.is_array()&&rows.size()==1?id(rows[0]):"";}
void member(std::vector<Member>& out,std::set<std::string>& seen,const json& row,const std::string& role,bool crew){
 if(!row.is_object())return;auto i=id(row),name=text(row,"name");if(i.empty()||name.empty())return;
 if(!seen.insert(i).second){if(!crew&&!role.empty())for(auto& person:out)if(person.id==i&&person.crew)person.character=role;return;}
 if(out.size()>=max_members)return;
 out.push_back({i,name,role,image(text(row,"profile_path",256)),crew});
}
void credits(Person& p,const json& rows){
 if(!rows.is_array())return;std::set<std::pair<std::string,std::string>> seen;
 // Inspect a bounded prefix, independently of the number accepted.
 for(size_t n=0;n<rows.size()&&n<4096;++n){const auto& r=rows[n];auto type=jstr(r,"media_type");if(type!="movie"&&type!="tv")continue;
  auto i=id(r);if(i.empty()||!seen.insert({type,i}).second)continue;auto& out=type=="movie"?p.movies:p.series;if(out.size()>=max_titles)continue;
  auto name=text(r,type=="movie"?"title":"name");if(name.empty())name=text(r,type=="movie"?"original_title":"original_name");if(name.empty())continue;
  auto date=text(r,type=="movie"?"release_date":"first_air_date",10);auto year=date.size()>=4?date.substr(0,4):"";
  out.push_back({i,type=="movie"?"movie":"series",name,image(text(r,"poster_path",256),false),text(r,"overview",4000),year,text(r,p.crew?"job":"character")});
 }
 const auto order=[](const Credit& a,const Credit& b){return a.year==b.year?a.name<b.name:a.year>b.year;};
 std::stable_sort(p.movies.begin(),p.movies.end(),order);std::stable_sort(p.series.begin(),p.series.end(),order);
}
}
bool valid_id(const std::string& s){return !s.empty()&&s.size()<=12&&s[0]!='0'&&s.find_first_not_of("0123456789")==std::string::npos;}
std::string image(const std::string& s,bool portrait){
 if(s.empty()||s.size()>256||s[0]!='/'||s.find("..")!=std::string::npos||s.find_first_not_of("/abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")!=std::string::npos||s.find('/',1)!=std::string::npos)return {};
 if(!s.ends_with(".jpg")&&!s.ends_with(".png")&&!s.ends_with(".webp"))return {};
 return std::string("https://image.tmdb.org/t/p/")+(portrait?"w185":"w342")+s;
}
Cast fetch_cast(const Meta& m,const std::string& key,const std::string& language,const std::atomic<bool>* cancel,const Request& request){
 Cast out;if(m.type!="movie"&&m.type!="series"){out.error="Cast photos are unavailable for this title.";return out;}Fetch f{key,cancel,request};auto media=media_id(m,f);if(media.empty()){out.error="Cast photos are unavailable for this title.";return out;}
 auto kind=m.type=="movie"?"movie":"tv";auto j=f.get(std::string(kind)+"/"+media+"?append_to_response="+(m.type=="movie"?"credits":"aggregate_credits")+"&language="+lang(language));
 if(id(j)!=media){out.error="Cast photos are unavailable for this title.";return out;}
 auto c=jobj(j,m.type=="movie"?"credits":"aggregate_credits");std::set<std::string> seen;
 auto crew=jobj(c,"crew");if(crew.is_array())for(size_t n=0;n<crew.size()&&n<512;++n){
  bool director=jstr(crew[n],"job")=="Director";auto jobs=jobj(crew[n],"jobs");
  if(jobs.is_array())for(size_t k=0;k<jobs.size()&&k<32;++k)director|=jstr(jobs[k],"job")=="Director";
  if(director)member(out.members,seen,crew[n],"Director",true);
 }
 auto creators=jobj(j,"created_by");if(creators.is_array())for(size_t n=0;n<creators.size()&&n<40;++n)member(out.members,seen,creators[n],"Creator",true);
 auto cast=jobj(c,"cast");if(cast.is_array())for(size_t n=0;n<cast.size()&&n<512;++n){auto role=text(cast[n],"character");auto roles=jobj(cast[n],"roles");if(role.empty()&&roles.is_array()&&!roles.empty())role=text(roles[0],"character");member(out.members,seen,cast[n],role,false);}
 if(out.members.empty())out.error=f.stopped()?"Cast loading cancelled.":"Cast photos are unavailable for this title.";return out;
}
Person fetch_person(const Member& m,const std::string& key,const std::string& language,const std::atomic<bool>* cancel,const Request& request){
 Person p;p.id=m.id;p.name=m.name;p.photo=m.photo;p.crew=m.crew;Fetch f{key,cancel,request};
 if(!valid_id(m.id)){p.error="Invalid person ID.";return p;}
 auto j=f.get("person/"+m.id+"?append_to_response=combined_credits&language="+lang(language));
 if(id(j)!=m.id){p.error=f.stopped()?"Person loading cancelled.":"Could not load this person's details. Try again.";return p;}
 p.name=text(j,"name");p.biography=text(j,"biography",12000);p.birthday=text(j,"birthday",10);p.deathday=text(j,"deathday",10);p.birthplace=text(j,"place_of_birth",240);p.known_for=text(j,"known_for_department",80);p.photo=image(text(j,"profile_path",256));
 if(p.name.empty())p.name=m.name;
 if(p.biography.empty()&&lang(language)!="en"&&!f.stopped()){auto en=f.get("person/"+m.id+"?language=en");if(id(en)==m.id)p.biography=text(en,"biography",12000);}
 const auto& c=jobj(j,"combined_credits");auto rows=jobj(c,m.crew?"crew":"cast");if(!rows.is_array()||rows.empty())rows=jobj(c,m.crew?"cast":"crew");credits(p,rows);
 return p;
}
Resolved resolve(const Credit& c,const std::string& key,const std::atomic<bool>* cancel,const Request& request){
 Resolved out;out.type=c.type;if(!valid_id(c.id)||(c.type!="movie"&&c.type!="series")){out.error="Invalid title ID.";return out;}
 Fetch f{key,cancel,request};auto j=f.get(std::string(c.type=="movie"?"movie":"tv")+"/"+c.id+"/external_ids");auto external=jstr(j,"imdb_id");
 if(id(j)!=c.id){out.error="Could not resolve this title. Try again.";return out;}
 if(imdb(external))out.id=external;else if(!f.failed)out.id="tmdb:"+c.id;else out.error=f.stopped()?"Title loading cancelled.":"Could not resolve this title. Try again.";return out;
}
}
