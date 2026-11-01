// Platform/task boundaries only; Session metadata and Library methods are production code.
#pragma once
#include "../gl/model.hpp"
#include "../gl/library_search.hpp"
#include "../gl/series_updates.hpp"
#include "../ui/content.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <map>
#include <set>
#include <deque>
struct ArtworkTasks {
    std::deque<std::function<void()>> pending;
    template<class T> void run(std::function<T()> work,std::function<void(T&)> done,bool) {
        pending.push_back([work,done]{auto result=work();done(result);});
    }
    void one() { auto f=std::move(pending.front());pending.pop_front();f(); }
};
inline ArtworkTasks g_tasks;
namespace sx {
struct Session {
    struct Cursor { int library=0; };
    enum {kLibrary=4, kLibraryNew=128};
    std::map<std::string,Item> meta_cache_;
    std::set<std::string> meta_asked_;
    std::size_t meta_inflight_=0;
    std::vector<std::unique_ptr<::Addon>> addons_;
    std::map<std::string,json> library_;
    std::map<std::string,Progress> progress_;
    std::vector<Item> lib_items_;
    LibraryContent library;
    std::string lib_type_, lib_query_;
    unsigned lib_search_generation_=0;
    int lib_sort_=1;
    unsigned profile_generation_=0,changed_=0;
    bool library_loaded_=true,art_dirty_=false;
    Page page_=Page::library;
    std::vector<std::string> fetched;
    bool account=true;
    bool signed_in() const {return account;}
    bool search_open=false;
    std::string global_query;
    void begin_search();
    void start_search(const std::string& q) {global_query=q;}
    void filter_activate(Page,int);
    std::vector<std::string> choices;
    std::function<void(int)> pick;
    void open_dropdown(const std::string&,const std::vector<std::string>& options,int,const std::function<void(int)>& done) {choices=options;pick=done;}
    std::function<void(const std::string&)> typed;
    std::string keyboard_title;
    void ask_text(const std::function<void(const std::string&)>& done,const std::string& title="") {typed=done;keyboard_title=title;}
    void reset_library_search();
    void library_search();
    void library_clear_search();
    sx::SeriesUpdates series_updates_;
    std::set<std::string> watched_, series_updates_pending_;
    std::shared_ptr<std::atomic<bool>> series_updates_cancel_;
    unsigned series_updates_generation_=0;
    int series_updates_inflight_=0;
    bool series_history_ready_=false;
    double series_updates_tick_=0;
    void series_updates_reset();
    void want_series_updates(const Item&);
    void refresh_board() {}
    Title make_title(const Item&,bool) const;
    void apply_meta(Item&) const;
    void want_meta(const Item&);
    void refresh_library();
    void refresh_art(const Cursor&);
    template<class T> void bg(std::function<T()> work,std::function<void(T&)> done,bool urgent=false);
};
}
