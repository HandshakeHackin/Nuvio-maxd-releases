// Platform/task boundaries only; Session metadata and Library methods are production code.
#pragma once
#include "../gl/model.hpp"
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
    enum {kLibrary=4};
    std::map<std::string,Item> meta_cache_;
    std::set<std::string> meta_asked_;
    std::size_t meta_inflight_=0;
    std::vector<std::unique_ptr<::Addon>> addons_;
    std::map<std::string,json> library_;
    std::map<std::string,Progress> progress_;
    std::vector<Item> lib_items_;
    LibraryContent library;
    std::string lib_type_;
    int lib_sort_=1;
    unsigned profile_generation_=0,changed_=0;
    bool library_loaded_=true,art_dirty_=false;
    Page page_=Page::library;
    std::vector<std::string> fetched;
    bool signed_in() const {return true;}
    void refresh_board() {}
    Title make_title(const Item& it,bool) const {Title t;t.id=it.id;t.name=it.name;return t;}
    void apply_meta(Item&) const;
    void want_meta(const Item&);
    void refresh_library();
    void refresh_art(const Cursor&);
    template<class T> void bg(std::function<T()> work,std::function<void(T&)> done,bool urgent=false);
};
}
