// Exercise production Session Library filtering and keyboard callbacks, offline.
#include "library_artwork_boundary.hpp"
#include <cassert>
#include <iostream>
#include "http.h"
int network_calls=0;
std::string HttpResponse::describe() const {return "offline fixture";}
HttpResponse http_get(const std::string&,long,const std::atomic<bool>*,const std::vector<std::string>&) {
    ++network_calls;return {503,"","offline fixture","",0};
}
int main() {
    sx::Session s;
    s.library_={
        {"a",{{"name","The Dark Knight"},{"type","movie"},{"state",{{"timeOffset",12000},{"duration",90000},{"lastWatchedMs",50},{"timesWatched",2}}}}},
        {"b",{{"name","Dark Matter"},{"type","series"},{"state",{{"lastWatchedMs",100},{"timesWatched",4}}}}},
        {"c",{{"name","Amélie"},{"type","movie"}}},
        {"d",{{"name","日本の映画"},{"type","movie"}}},
        {"gone",{{"name","Dark removed"},{"type","movie"},{"removed",true}}},
        {"temp",{{"name","Dark progress"},{"type","movie"},{"temp",true}}}
    };
    auto search=[&](const std::string& q){s.library_search();assert(s.keyboard_title.find("Library")!=std::string::npos);auto done=s.typed;done(q);};
    s.refresh_library();assert(s.lib_items_.size()==4 && s.library.filters.size()==3);
    // Triangle routes locally on Library, while other pages and existing global results keep global search.
    s.begin_search();assert(s.keyboard_title.find("Library")!=std::string::npos);s.typed("Dark");
    s.page_=sx::Page::board;s.begin_search();s.typed("global");assert(s.global_query=="global" && s.lib_query_=="Dark");
    s.page_=sx::Page::library;s.search_open=true;s.begin_search();s.typed("global edit");assert(s.global_query=="global edit");s.search_open=false;
    s.library_clear_search();
    // Existing type/sort indexes retain their behavior; new chips activate keyboard and clear.
    s.filter_activate(sx::Page::library,0);assert(s.choices==std::vector<std::string>({"All types","Movie","Series"}));s.pick(1);assert(s.lib_type_=="movie");
    s.filter_activate(sx::Page::library,1);s.pick(1);assert(s.lib_sort_==1);
    s.filter_activate(sx::Page::library,2);s.typed("Dark");assert(s.lib_items_.size()==1);
    s.filter_activate(sx::Page::library,3);assert(s.lib_query_.empty() && s.lib_items_.size()==3);
    s.lib_type_.clear();s.refresh_library();
    const auto original=s.library_;
    search("  dArK   ");
    assert(s.lib_query_=="dArK" && s.lib_items_.size()==2 && s.library.count=="2 of 4 titles");
    assert(s.library.filters.size()==4 && s.library.filters[3].label=="Clear");
    assert((s.changed_&sx::Session::kLibraryNew)!=0);
    s.lib_type_="movie";s.refresh_library();
    assert(s.lib_items_.size()==1 && s.lib_items_[0].id=="a" && s.lib_items_[0].offset==12);
    s.lib_type_.clear();s.lib_sort_=0;s.refresh_library();assert(s.lib_items_[0].id=="b");
    s.lib_sort_=2;s.refresh_library();assert(s.lib_items_[0].id=="b");
    search("knight DARK");assert(s.lib_items_.size()==1 && s.lib_items_[0].id=="a");
    search("AMELIE");assert(s.lib_items_.size()==1 && s.lib_items_[0].id=="c");
    search("日本");assert(s.lib_items_.size()==1 && s.lib_items_[0].id=="d");
    search("no such title");assert(s.lib_items_.empty() && s.library.status.find("No Library titles match")!=std::string::npos);
    s.library_clear_search();assert(s.lib_items_.size()==4 && s.library.filters.size()==3);
    search("Dark");s.library_search(); // Cancelled keyboard never invokes done.
    assert(s.lib_query_=="Dark" && s.lib_items_.size()==2);
    auto stale=s.typed;s.reset_library_search();s.library_={{"other",{{"name","Other source title"},{"type","movie"}}}};
    s.refresh_library();stale("Dark");assert(s.lib_query_.empty() && s.lib_items_.size()==1 && s.lib_items_[0].id=="other");
    s.library_search();stale=s.typed;++s.profile_generation_;s.reset_library_search();
    s.library_={{"profile-b",{{"name","New profile title"},{"type","series"}}}};s.refresh_library();
    stale("Other source");assert(s.lib_query_.empty() && s.lib_items_.size()==1 && s.lib_items_[0].id=="profile-b");
    s.library_search();stale=s.typed;s.library_search();s.typed("New");stale("no match");assert(s.lib_query_=="New");
    search(" \t ");assert(s.lib_query_.empty() && s.lib_items_.size()==1);
    assert(network_calls==0 && g_tasks.pending.empty());
    // Search never mutates membership or watch progress, including guest history.
    s.library_=original;search("Dark");assert(s.library_==original);
    s.account=false;s.progress_.clear();sx::Progress p;p.name="Guest movie";p.type="movie";p.time=15;s.progress_["guest"]=p;
    search("guest");assert(s.lib_items_.size()==1 && s.lib_items_[0].id=="guest" && s.lib_items_[0].offset==15);
    auto bounded=sx::library_search::query(std::string(255,'a')+"éx");
    assert(bounded.size()==255 && bounded.back()=='a');
    std::cout<<"PASS: local Library search, type/sort/progress, accents/UTF-8, clear/cancel, source/profile isolation and zero search requests\n";
}
