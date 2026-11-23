#include "../ui/i18n.hpp"
// Secret/general text uses the console dialog. Search has its own local editor.

#include "session.hpp"
#include "search_suggestions.hpp"

#include <algorithm>

namespace sx
{

namespace
{

constexpr std::size_t kMaxRowCards = 50;

std::string row_title(const Catalog &c)
{
    const std::string type = capitalize(c.type);
    return c.name.empty() ? type : c.name + " - " + type;
}

} // namespace

void Session::ask_text(const std::function<void(const std::string &)> &done, const std::string& title, bool secret)
{
    text_done_ = done;
    if (!ime_.open(title,secret))
    {
        text_done_ = nullptr;
        say(sx::i18n::tr("The keyboard could not be opened."), 4.0, true);
    }
}

void Session::poll_typing()
{
    if (!ime_.active())
        return;
    std::string text;
    const Ime::State s = ime_.poll(&text);
    if (s == Ime::State::done)
    {
        const auto fn = text_done_;
        text_done_ = nullptr;
        if (fn)
            fn(text);
    }
    else if (s == Ime::State::cancelled)
    {
        text_done_ = nullptr;
        if(search_keyboard_open)search_keyboard_changed();
    }
}

void Session::begin_search()
{
    if (page_ == Page::library && !search_open) { library_search(); return; }
    ask_search([this](const std::string &q) { start_search(q); },i18n::tr("Search movies, series and more"),search_query,true);
}

void Session::open_search_page() {
    search_open=true;
    if(search.rows.empty()&&search_pending_==0)search.status=i18n::tr("Press Triangle to search your configured catalogs.");
    changed_|=kSearch|kSearchNew;
}

void Session::ask_search(const std::function<void(const std::string&)>& done,const std::string& title,const std::string& initial,bool suggestions) {
    if(watching||ime_.active()||pin_open)return;
    search_keyboard_cancel();
    search_entry={};search_entry.title=title;search_entry.text=search_keyboard::bounded(initial);
    search_entry.language=search_keyboard::language_index(search_keyboard_language_);
    search_entry.suggestions_enabled=suggestions;
    const auto profile=profile_generation_;
    search_keyboard_done_=[this,profile,done](const std::string& text){if(profile==profile_generation_)done(text);};
    search_keyboard_open=true;search_keyboard_changed();
}

void Session::search_keyboard_cancel() {
    *suggest_cancel_=true;++suggest_generation_;
    search_keyboard_open=false;search_keyboard_done_=nullptr;search_entry={};
    suggest_urls_.clear();suggest_rows_.clear();suggest_prepared_=false;
}

void Session::search_keyboard_changed() {
    *suggest_cancel_=true;suggest_cancel_=std::make_shared<std::atomic<bool>>(false);++suggest_generation_;
    suggest_urls_.clear();suggest_rows_.clear();suggest_next_=0;suggest_completed_=suggest_errors_=0;suggest_prepared_=false;
    search_entry.suggestions.clear();search_entry.suggestion=-1;
    suggest_after_=now_seconds()+.5;suggest_deadline_=suggest_after_+12;
    search_entry.status=!search_entry.suggestions_enabled?i18n::tr("Search this section"):search_keyboard::decode(trim(search_entry.text)).size()<2?i18n::tr("Type at least two characters"):i18n::tr("Searching...");
}

void Session::search_keyboard_input(const hui::InputFrame& input) {
    if(!search_keyboard_open||ime_.active())return;
    const auto action=search_entry.input(input);
    if(action==search_keyboard::Event::changed)search_keyboard_changed();
    else if(action==search_keyboard::Event::language){
        search_keyboard_language_=search_keyboard::languages[search_entry.language].code;
        save_profile_settings();
    }else if(action==search_keyboard::Event::cancel)search_keyboard_cancel();
    else if(action==search_keyboard::Event::submit){
        const auto fn=search_keyboard_done_;const auto text=search_entry.text;
        search_keyboard_cancel();if(fn)fn(text);
    }else if(action==search_keyboard::Event::system){
        // Pause autocomplete while the system IME owns the controller. Cancelling
        // it keeps the draft. Its callback cannot outlive this profile/editor.
        *suggest_cancel_=true;++suggest_generation_;
        const auto profile=profile_generation_,generation=suggest_generation_;
        text_done_=[this,profile,generation](const std::string& text){
            if(profile!=profile_generation_||generation!=suggest_generation_||!search_keyboard_open)return;
            search_entry.text=search_keyboard::bounded(text);search_keyboard_changed();
        };
        if(!ime_.open(search_entry.title,false,search_entry.text)){
            text_done_=nullptr;say(i18n::tr("The keyboard could not be opened."),4,true);search_keyboard_changed();
        }
    }
}

void Session::search_suggestions_tick() {
    if(!search_keyboard_open||ime_.active()||!search_entry.suggestions_enabled||search_keyboard::decode(trim(search_entry.text)).size()<2||now_seconds()<suggest_after_)return;
    if(now_seconds()>=suggest_deadline_){
        *suggest_cancel_=true;
        if(suggest_completed_<int(suggest_rows_.size()))search_entry.status=i18n::tr("Some catalogs could not be reached. You can still search.");
        return;
    }
    if(!suggest_prepared_){
        suggest_prepared_=true;const auto q=trim(search_entry.text);std::set<std::string> seen;
        for(const auto& a:addons_)for(const auto& c:a->catalogs){
            if(!c.has_extra("search")||c.requires_other_than("search")||suggest_urls_.size()==search_suggestions::max_catalogs)continue;
            const auto url=a->resource_url("catalog",c.type,c.id,"search="+url_encode(q));
            if(seen.insert(url).second)suggest_urls_.push_back(url);
        }
        suggest_rows_.resize(suggest_urls_.size());
        if(suggest_urls_.empty())search_entry.status=addons_loading_?i18n::tr("Addons are still loading..."):i18n::tr("None of your addons can search.");
    }
    while(suggest_inflight_<2&&suggest_next_<suggest_urls_.size()){
        const auto index=suggest_next_++;const auto url=suggest_urls_[index];
        const auto cancel=suggest_cancel_;const auto gen=suggest_generation_,profile=profile_generation_;++suggest_inflight_;
        // Always release the global slot, including stale profile/query replies.
        g_tasks.run<search_suggestions::Result>([url,cancel]{return search_suggestions::fetch(url,cancel.get());},[this,index,gen,profile](search_suggestions::Result& result){
            --suggest_inflight_;
            if(!search_keyboard_open||*suggest_cancel_||gen!=suggest_generation_||profile!=profile_generation_||index>=suggest_rows_.size())return;
            ++suggest_completed_;if(result.failed)++suggest_errors_;
            suggest_rows_[index]=std::move(result.items);
            // Do not reorder a list while the user is choosing a suggestion.
            if(search_entry.suggestion<0)search_entry.suggestions=search_suggestions::merge(suggest_rows_);
            search_entry.status=suggest_completed_<int(suggest_rows_.size())?i18n::tr("Searching..."):suggest_errors_?i18n::tr("Some catalogs could not be reached. You can still search."):search_entry.suggestions.empty()?i18n::tr("No title suggestions. You can still search."):i18n::tr("From your configured catalogs");
        });
    }
}

void Session::close_search()
{
    search_keyboard_cancel();
    ++search_gen_;
    search_pending_=0;
    search_open = false;
    search.rows.clear();
    search.status.clear();
    search_.clear();
    search_map_.clear();
    changed_ |= kSearch | kSearchNew;
}

void Session::start_search(const std::string &text)
{
    const std::string q = trim(text);
    if (q.empty())
        return;
    if(prefs_.builtin_torrents && torrent_try_search_input(q))return;
    const int gen = ++search_gen_;
    search_query = q;
    search_open = true;
    search_.clear();
    search_map_.clear();
    search.rows.clear();
    for (const auto &a : addons_)
        for (const auto &c : a->catalogs)
        {
            if (!c.has_extra("search") || c.requires_other_than("search"))
                continue;
            BoardRow row;
            row.key = "search|" + a->transport_url + "|" + c.type + "|" + c.id;
            row.title = row_title(c);
            row.addon_url = a->transport_url;
            row.catalog = c;
            search_.push_back(row);
        }
    changed_ |= kSearch | kSearchNew;
    if (search_.empty())
    {
        search.status = addons_loading_ ? sx::i18n::tr("Addons are still loading...") : sx::i18n::tr("None of your addons can search.");
        return;
    }
    search.status = sx::i18n::tr("Searching...");
    search_pending_ = static_cast<int>(search_.size());
    for (std::size_t i = 0; i < search_.size(); ++i)
    {
        const ::Addon *a = find_addon(search_[i].addon_url);
        const std::string url =
            a ? a->resource_url("catalog", search_[i].catalog.type, search_[i].catalog.id, "search=" + url_encode(q)) : "";
        struct Res
        {
            std::vector<Item> items;
            std::string error;
        };
        bg<Res>(
            [url]() {
                Res r;
                json j;
                if (!url.empty() && fetch_json(url, j, r.error))
                {
                    const json &metas = jobj(j, "metas");
                    if (metas.is_array())
                        for (const auto &m : metas)
                        {
                            Item it = item_from_meta_json(m);
                            if (!it.id.empty() && !it.name.empty())
                                r.items.push_back(it);
                        }
                }
                return r;
            },
            [this, gen, i](Res &r) {
                if (gen != search_gen_ || i >= search_.size())
                    return;
                search_[i].loaded = true;
                search_[i].items = std::move(r.items);
                if (search_[i].items.size() > kMaxRowCards)
                    search_[i].items.resize(kMaxRowCards);
                --search_pending_;
                refresh_search();
            });
    }
}

void Session::refresh_search()
{
    search.rows.clear();
    search_map_.clear();
    std::size_t total = 0;
    for (std::size_t i = 0; i < search_.size(); ++i)
    {
        const BoardRow &br = search_[i];
        if (!br.loaded || br.items.empty())
            continue;
        Row row;
        row.key = br.key;
        row.title = br.title;
        for (const Item &it : br.items)
            row.items.push_back(make_title(it, false));
        total += br.items.size();
        search.rows.push_back(std::move(row));
        search_map_.push_back(static_cast<int>(i));
    }
    // "No results" is the screen's own message once the search is over.
    search.status = search_pending_ > 0 && total == 0 ? sx::i18n::tr("Searching...") : "";
    changed_ |= kSearch;
    art_dirty_ = true;
}

} // namespace sx
