#include "continue_watching.hpp"
#include <algorithm>
namespace sx {
std::vector<Item> make_continue_watching(const std::map<std::string,json>& library_,
    const std::map<std::string,Progress>& progress_, bool account_ready,
    const std::function<void(Item&)>& apply_meta)
{
    std::vector<Item> out;
    if (account_ready)
    {
        for (const auto &kv : library_)
        {
            const json &li = kv.second;
            const json &st = jobj(li, "state");
            const double offset = jnum(st, "timeOffset") / 1000.0;
            const bool removed = jbool(li, "removed"), temp = jbool(li, "temp");
            if (offset <= 0 || (removed && !temp))
                continue;
            Item it;
            it.id = kv.first;
            it.type = jstr(li, "type");
            it.name = jstr(li, "name");
            it.poster = jstr(li, "poster");
            it.background = jstr(li, "background");
            it.logo = jstr(li, "logo");
            it.video_id = jstr(st, "video_id");
            it.offset = offset;
            it.duration = jnum(st, "duration") / 1000.0;
            it.last_watched = int64_t(jnum(st, "lastWatchedMs", double(iso8601_to_ms(jstr(st, "lastWatched")))));
            apply_meta(it);
            out.push_back(it);
        }
    }
    else
    {
        for (const auto &kv : progress_)
        {
            if (kv.second.time <= 0)
                continue;
            Item it;
            it.id = kv.first;
            it.type = kv.second.type;
            it.name = kv.second.name;
            it.poster = kv.second.poster;
            it.video_id = kv.second.video_id;
            it.offset = kv.second.time;
            it.duration = kv.second.duration;
            it.last_watched = kv.second.updated;
            apply_meta(it);
            out.push_back(it);
        }
    }
    // The episode after one that was just finished: nothing of it is watched yet, so there is no
    // timeOffset to find, only what was remembered here.
    for (const auto &kv : progress_)
    {
        if (kv.second.up_next.empty())
            continue;
        bool listed = false;
        for (const Item &o : out)
            listed = listed || o.id == kv.first;
        if (listed)
            continue;
        Item it;
        it.id = kv.first;
        it.type = kv.second.type;
        it.name = kv.second.name;
        it.poster = kv.second.poster;
        const auto li = library_.find(kv.first);
        if (li != library_.end())
        {
            it.poster = jstr(li->second, "poster");
            it.background = jstr(li->second, "background");
            it.logo = jstr(li->second, "logo");
        }
        it.video_id = kv.second.up_next;
        it.up_next = true;
        it.last_watched = kv.second.updated;
        apply_meta(it);
        out.push_back(it);
    }
    std::sort(out.begin(), out.end(), [](const Item &a, const Item &b) { return a.last_watched > b.last_watched; });
    return out;
}

}
