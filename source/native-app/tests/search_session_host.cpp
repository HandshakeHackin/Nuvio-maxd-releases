#include "search_session_boundary.hpp"
#include <cassert>
#include <iostream>
SearchTasks g_tasks;
static int requests=0,cancelled=0;
namespace feature_http {
Response perform(const Query& q,const std::atomic<bool>* cancel){
 ++requests;Response r;
 if(cancel&&*cancel){++cancelled;r.error="Cancelled";return r;}
 assert(q.timeout_ms==4000&&q.limit==512u<<10);
 assert(q.url.starts_with("https://configured.invalid/private-path/"));
 assert(q.url.ends_with("?config=fixture"));
 const auto query=q.url.find("search=beta")!=std::string::npos?"Beta":q.url.find("search=gamma")!=std::string::npos?"Gamma":"Alpha";
 r.status=200;r.body=json{{"metas",json::array({json{{"id",query},{"type","movie"},{"name",query}}})}}.dump();return r;
}
}
static hui::InputFrame press(hui::Action action){hui::InputFrame f;f.pressed=hui::action_bit(action);return f;}
static void ready(sx::Session& s){s.suggest_after_=now_seconds()-1;s.search_suggestions_tick();}
int main(){
 sx::Session s;
 auto addon=std::make_unique<Addon>();
 json manifest={{"id","fixture"},{"name","Fixture"},{"version","1.0"},{"resources",json::array({"catalog"})},{"types",json::array({"movie"})},{"catalogs",json::array()}};
 for(int n=0;n<12;++n)manifest["catalogs"].push_back({{"type","movie"},{"id",std::to_string(n)},{"extra",json::array({json{{"name","search"}}})}});
 assert(parse_addon(manifest,"https://configured.invalid/private-path/manifest.json?config=fixture",*addon));s.addons_.push_back(std::move(addon));
 std::string result;
 s.ask_search([&](const std::string& text){result=text;},"Search","alpha",true);
 s.search_suggestions_tick();assert(g_tasks.work.empty()); // debounce
 ready(s);assert(g_tasks.work.size()==2&&s.suggest_inflight_==2&&s.suggest_urls_.size()==8);
 s.search_entry.text="beta";s.search_keyboard_changed();ready(s);assert(g_tasks.work.size()==2); // no overlapping generations beyond 2 slots
 g_tasks.finish();assert(s.search_entry.suggestions.empty());ready(s);assert(g_tasks.work.size()==2&&s.suggest_inflight_==2);
 g_tasks.finish();assert(s.search_entry.suggestions.empty()); // second stale query reply
 g_tasks.finish();assert(s.search_entry.suggestions.size()==1&&s.search_entry.suggestions[0].name=="Beta");
 ready(s);assert(g_tasks.work.size()==2);
 s.search_keyboard_cancel();++s.profile_generation_;
 s.ask_search([&](const std::string& text){result=text;},"Search","gamma",true);ready(s);assert(g_tasks.work.size()==2);
 g_tasks.finish();g_tasks.finish();assert(s.search_entry.suggestions.empty());ready(s);g_tasks.finish();assert(s.search_entry.suggestions[0].name=="Gamma");
 s.search_entry.suggestion=0;s.search_keyboard_input(press(hui::Action::confirm));assert(result=="Gamma"&&!s.search_keyboard_open);
 while(!g_tasks.work.empty())g_tasks.finish();assert(s.suggest_inflight_==0);
 s.ask_search([&](const std::string& text){result=text;},"Local","",false);ready(s);assert(g_tasks.work.empty());
 s.search_keyboard_input(press(hui::Action::page_prev));assert(s.saves==1&&s.search_keyboard_language_=="zh-Hans");
 s.search_entry.text="中文 😀";s.search_keyboard_input(press(hui::Action::menu));assert(s.ime_.opened&&!s.ime_.secret&&s.ime_.initial=="中文 😀");
 auto old_callback=s.text_done_;s.search_keyboard_cancel();++s.profile_generation_;old_callback("other profile");assert(!s.search_keyboard_open&&s.search_entry.text.empty());
 s.ime_.opened=false;s.ask_search([&](const std::string& text){result=text;},"Local","",false);s.search_keyboard_input(press(hui::Action::north));assert(result.empty()&&!s.search_keyboard_open); // empty clears local filters
 s.ask_search([&](const std::string&){assert(false);},"Search","alpha",true);ready(s);s.suggest_deadline_=now_seconds()-1;s.search_suggestions_tick();assert(*s.suggest_cancel_);while(!g_tasks.work.empty())g_tasks.finish();assert(s.search_entry.suggestions.empty());
 assert(requests>0&&cancelled==0); // cancelled jobs never reach the transport
 std::cout<<"Production search session: debounce, configured endpoints, 8-source/2-request caps, stale query/profile replies, slot recovery, system-IME draft, local empty filter and deadline cancellation passed\n";
}
