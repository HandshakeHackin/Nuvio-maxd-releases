#include <cassert>
#include "people_session_boundary.hpp"
#include "providers.h"
#include "i18n.hpp"
#include <iostream>
namespace providers {
int requests=0;std::string language;people::Member selected;
people::Cast title_people(const Meta&,const std::atomic<bool>*,const std::string& lang){++requests;language=lang;return {{{"1","Settings","Role","https://image.tmdb.org/t/p/w185/a.jpg",false}},""};}
people::Person person_details(const people::Member& m,const std::atomic<bool>*,const std::string&){++requests;selected=m;people::Person p;p.id=m.id;p.name=m.name;p.movies={{"19","movie","Library"}};return p;}
people::Resolved person_title(const people::Credit& c,const std::atomic<bool>*){++requests;return {"tt8765",c.type,""};}
}
int main(){
 sx::i18n::set_language("de");sx::Session s;s.d_item_.id="tt1234";s.d_item_.type="movie";
 s.cast_load();assert(s.detail.people_loading);s.cast_load();assert(g_tasks.pending.size()==1);g_tasks.one();assert(s.detail.people.size()==1&&!s.detail.people_loading&&providers::language=="de");assert(s.detail.people[0].name=="Settings");
 s.cast_load();++s.d_meta_gen_;s.detail.people.clear();g_tasks.one();assert(s.detail.people.empty());
 // A new title cancels its predecessor and replaces the detail loading state.
 *s.cast_cancel_=true;s.detail.people_loading=false;
 s.cast_load();g_tasks.one();s.person_show(0);assert(s.person_open&&s.person.loading);s.person_leave();g_tasks.one();assert(!s.person_open&&s.person.data.movies.empty());
 s.person_show(0);++s.profile_generation_;g_tasks.one();assert(s.person.loading);s.person_leave();
 s.resuming_=true;assert(!s.person_show(0));s.resuming_=false;
 s.person_show(0);s.person_retry();assert(g_tasks.pending.size()==1);g_tasks.one();assert(!s.person.loading&&s.person.data.name=="Settings");assert(s.person.data.movies[0].name=="Library");
 auto credit=s.person.data.movies[0];s.person_pick(credit);assert(s.person.resolving);g_tasks.one();assert(!s.person.resolving&&!s.person_target_&&!s.person.status.empty());
 auto addon=std::make_unique<Addon>();addon->types={"movie","series"};AddonResource resource;resource.name="meta";addon->resources.push_back(resource);s.addons_.push_back(std::move(addon));
 s.person_pick(credit);g_tasks.one();assert(s.person_target_&&s.person_target_->id=="tt8765"&&s.person_target_->name=="Library");
 s.person_target_.reset();s.person_pick(credit);s.person_leave();g_tasks.one();assert(!s.person_target_);
 s.person_open=true;s.person_pick(credit);++s.profile_generation_;g_tasks.one();assert(!s.person_target_);s.person_leave();
 sx::PersonContent cached;cached.data.name="Person";cached.data.id="1";cached.data.movies={credit};cached.photo=123;cached.movies={456};s.person_restore(cached);assert(s.person_open&&s.person.photo==0&&s.person.movies==std::vector<unsigned>{0});
 sx::Item title;title.id="tt8765";title.type="movie";title.name="Library";s.open_title_item(title);assert(s.d_item_.id==title.id&&s.d_item_.name==title.name);
 sx::i18n::set_language("en");std::cout<<"People Session: cancelled/stale/profile completions, literal metadata, addon support, resolved titles and safe back snapshots passed\n";
}
