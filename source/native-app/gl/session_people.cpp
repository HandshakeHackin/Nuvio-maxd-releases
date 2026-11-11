// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "providers.h"
#include "i18n.hpp"
namespace sx {
void Session::cast_load(){
 if(detail.people_loading&&!*cast_cancel_)return;
 *cast_cancel_=true;cast_cancel_=std::make_shared<std::atomic<bool>>(false);
 const auto cancel=cast_cancel_;const auto generation=++cast_generation_;const auto meta_generation=d_meta_gen_;
 Meta meta=d_meta_;if(meta.id.empty())meta.id=d_item_.id;if(meta.type.empty())meta.type=d_item_.type;
 if(meta.imdb_id.empty()&&meta.id.starts_with("tt"))meta.imdb_id=meta.id;
 if(meta.tmdb_id.empty()&&meta.id.starts_with("tmdb:"))meta.tmdb_id=meta.id.substr(5);
 detail.people_loading=true;detail.people_status=i18n::tr("Loading cast & crew...");changed_|=kDetail;
 const std::string language=i18n::code();
 const bool had_tmdb=!meta.tmdb_id.empty();
 bg<people::Cast>([meta,cancel,language]{return providers::title_people(meta,cancel.get(),language);},[this,cancel,generation,meta_generation,had_tmdb](auto& result){
  if(*cancel||generation!=cast_generation_||meta_generation!=d_meta_gen_||!d_open_)return;
  detail.people.clear();for(const auto& m:result.members)detail.people.push_back({m.id,m.name,m.role,m.photo,0,m.crew,m.character});
  detail.people_loading=false;detail.people_status=i18n::tr(result.error);changed_|=kDetail;art_dirty_=true;
  if(detail.people.empty()&&!had_tmdb&&d_meta_loaded_&&people::valid_id(d_meta_.tmdb_id))cast_load();
 },false);
}
bool Session::person_show(int index){
 if(index<0||index>=int(detail.people.size())||watching||resuming_)return false;
 const auto& c=detail.people[size_t(index)];selected_person_={c.id,c.name,c.role,c.photo_url,c.crew};person_open=true;resume_play_=false;person_retry();return true;
}
void Session::person_retry(){
 if(!person_open||!people::valid_id(selected_person_.id))return;
 if(person.loading&&!*person_cancel_)return;
 *person_cancel_=true;person_cancel_=std::make_shared<std::atomic<bool>>(false);person_target_.reset();
 const auto cancel=person_cancel_;const auto generation=++person_generation_;const auto member=selected_person_;const std::string language=i18n::code();
 person={};person.loading=true;person.data.id=member.id;person.data.name=member.name;person.data.photo=member.photo;person.data.crew=member.crew;person.status=i18n::tr("Loading person details...");changed_|=kPerson;art_dirty_=true;
 bg<people::Person>([member,cancel,language]{return providers::person_details(member,cancel.get(),language);},[this,cancel,generation](auto& result){
  if(*cancel||generation!=person_generation_||!person_open)return;
  person.data=std::move(result);person.loading=false;person.status=i18n::tr(person.data.error);changed_|=kPerson;art_dirty_=true;
 },false);
}
void Session::person_leave(){*person_cancel_=true;++person_generation_;person_target_.reset();person_open=false;person.resolving=false;}
void Session::person_restore(PersonContent content){
 person_leave();person=std::move(content);person.photo=0;person.movies.assign(person.data.movies.size(),0);person.series.assign(person.data.series.size(),0);
 person.resolving=false;person_open=true;selected_person_={person.data.id,person.data.name,"",person.data.photo,person.data.crew};
 changed_|=kPerson;art_dirty_=true;if(person.loading)person_retry();
}
void Session::person_pick(const people::Credit& credit){
 if(!person_open||person.loading||person.resolving||watching||resuming_)return;
 const auto cancel=person_cancel_;const auto generation=person_generation_;
 person.resolving=true;person.status=i18n::tr("Opening title...");changed_|=kPerson;
 bg<people::Resolved>([credit,cancel]{return providers::person_title(credit,cancel.get());},[this,credit,cancel,generation](auto& result){
  if(*cancel||generation!=person_generation_||!person_open)return;
  person.resolving=false;
  if(!result.error.empty()){person.status=i18n::tr(result.error);changed_|=kPerson;return;}
  const auto supported=[&](const std::string& id){for(const auto& a:addons_)if(a->supports("meta",result.type,id))return true;return false;};
  auto id=result.id;if(!supported(id)&&supported("tmdb:"+credit.id))id="tmdb:"+credit.id;
  if(!supported(id)){person.status=i18n::tr("No configured addon has details for this title.");changed_|=kPerson;return;}
  Item item;item.id=id;item.type=result.type;item.name=credit.name;item.poster=credit.poster;item.description=credit.overview;item.release_info=credit.year;
  person_target_=std::move(item);person.status.clear();changed_|=kPerson;
 },false);
}
void Session::open_title_item(const Item& item){detail_seed_=item;Title title;title.id=item.id;title.type=item.type;title.name=item.name;open_title(title);}
}
