#include <cassert>
#include "people_session_boundary.hpp"
#include "providers.h"
#include "i18n.hpp"
#include <iostream>
#include <filesystem>
namespace providers {
int requests=0;std::string language;people::Member selected;
people::Cast title_people(const Meta&,const std::atomic<bool>*,const std::string& lang){++requests;language=lang;return {{{"1","Settings","Role","https://image.tmdb.org/t/p/w185/a.jpg",false}},"",{{"ES","12"},{"US","PG-13"}}};}
people::Person person_details(const people::Member& m,const std::atomic<bool>*,const std::string&){++requests;selected=m;people::Person p;p.id=m.id;p.name=m.name;p.movies={{"19","movie","Library"}};return p;}
people::Resolved person_title(const people::Credit& c,const std::atomic<bool>*){++requests;return {"tt8765",c.type,""};}
}
int main(){
 sx::i18n::set_language("de");sx::Session s;s.d_item_.id="tt1234";s.d_item_.type="movie";
 s.cast_load();assert(s.detail.people_loading);s.cast_load();assert(g_tasks.pending.size()==1);g_tasks.one();assert(s.detail.people.size()==1&&!s.detail.people_loading&&providers::language=="de");assert(s.detail.people[0].name=="Settings");
 assert(s.detail.title.certification=="US PG-13");
 s.cast_load();s.prefs_.age_rating_country="ES";g_tasks.one();assert(s.detail.title.certification=="ES 12");
 s.cast_load();s.detail.title.certification.clear();++s.profile_generation_;g_tasks.one();assert(s.detail.title.certification.empty());s.detail.people_loading=false;
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
 char pattern[]="/tmp/nuvio-rating-profile-XXXXXX";auto* path=mkdtemp(pattern);assert(path);
 const std::string base=path;std::filesystem::create_directory(base+"/1");std::filesystem::create_directory(base+"/2");
 s.profile_dir_=base+"/1";s.prefs_.age_rating_country="US";s.profile_ready_=false;s.choose={};s.age_rating_country_menu();assert(!s.choose);s.profile_ready_=true;
 assert(save_json(s.profile_dir_+"/profile-settings.json",json{{"keep","other preferences"}}));
 s.age_rating_country_menu();assert(s.choices.size()==age_ratings::countries.size());auto stale=s.choose;++s.profile_generation_;auto writes=s.saves;stale(int(age_ratings::country_index("ES")));assert(s.saves==writes);
 s.age_rating_country_menu();s.choose(-1);assert(s.saves==writes);s.choose(32);assert(s.saves==writes);
 s.choose(int(age_ratings::country_index("ES")));json stored;assert(load_json(s.profile_dir_+"/profile-settings.json",stored));assert(stored["age_rating_country"]=="ES"&&stored["keep"]=="other preferences"&&s.detail.title.certification=="ES 12");
 s.fail_save=true;s.choose(0);assert(s.prefs_.age_rating_country=="ES"&&s.errors==1);s.fail_save=false;
 auto other_profile_callback=s.choose;s.profile_dir_=base+"/2";++s.profile_generation_;json empty;s.prefs_.age_rating_country=age_ratings::load_preference(empty);assert(s.prefs_.age_rating_country=="US");
 writes=s.saves;other_profile_callback(0);assert(s.saves==writes&&!std::filesystem::exists(s.profile_dir_+"/profile-settings.json"));
 s.age_rating_country_menu();s.choose(int(age_ratings::country_index("GB")));assert(s.detail.title.certification=="US PG-13");
 assert(load_json(base+"/1/profile-settings.json",stored)&&age_ratings::load_preference(stored)=="ES");
 std::filesystem::remove_all(base);
 std::cout<<"Age-rating Session: current preference on completion, stale title/profile rejection, durable per-profile country, rollback and cached fallback passed\n";
 sx::i18n::set_language("en");std::cout<<"People Session: cancelled/stale/profile completions, literal metadata, addon support, resolved titles and safe back snapshots passed\n";
}
