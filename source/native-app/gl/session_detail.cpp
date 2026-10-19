// A title's page: its details, the seasons and episodes of a series, and the
// streams the addons offer for a film or an episode.

#include "session.hpp"
#include "providers.h"
#include "stream_display.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace sx
{

namespace
{

using stream_display::view_of;
using stream_display::resolution_of;
using stream_display::seeds_of;

int resolution_rank(const std::string &r)
{
    const char *order[] = {"4K", "1440p", "1080p", "720p", "SD", "Other"};
    for (int i = 0; i < 6; ++i) if (r == order[i]) return i;
    return 5;
}

// "2010-09-14T00:00:00.000Z" -> "Sep 14, 2010"
std::string date_label(const std::string &released)
{
    if (released.size() < 10 || released[4] != '-' || released[7] != '-')
        return released;
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const int m = std::atoi(released.substr(5, 2).c_str());
    if (m < 1 || m > 12)
        return released.substr(0, 10);
    return std::string(months[m - 1]) + " " + std::to_string(std::atoi(released.substr(8, 2).c_str())) + ", " +
           released.substr(0, 4);
}

std::string episode_title(const ::Video &v)
{
    return v.title.empty() ? "Episode " + std::to_string(v.episode) : v.title;
}

} // namespace

Stream stream_view(const ::Stream &st)
{
    return view_of(st);
}

bool Session::watched(const std::string &video_id) const
{
    if (watched_.count(video_id)) return true;
    for (const auto& video : d_meta_.videos) if (video.id == video_id)
        return watched_.count(d_item_.id + ":" + std::to_string(video.season) + ":" + std::to_string(video.episode)) > 0;
    return false;
}

void Session::detail_toggle_watched(int index)
{
    if (index < 0 || index >= static_cast<int>(d_season_videos_.size()))
        return;
    const std::string &id = d_season_videos_[static_cast<std::size_t>(index)]->id;
    const auto& video = *d_season_videos_[static_cast<size_t>(index)];
    const auto canonical = d_item_.id + ":" + std::to_string(video.season) + ":" + std::to_string(video.episode);
    const bool mark = !watched(id);
    if (!mark)
    {
        watched_.erase(id); watched_.erase(canonical);
        say("Marked as not watched");
    }
    else
    {
        watched_.insert(canonical);
        say("Marked as watched");
    }
    save_progress();
    sync_watched_mark(id, d_item_.name, mark, d_item_.id, video.season, video.episode);
    detail.episodes[static_cast<std::size_t>(index)].watched = watched(id);
    changed_ |= kDetail;
}

// ---------------------------------------------------------------------------

void Session::open_title(const Title &t, const std::string &streams_video)
{
    detail_preparation_cancel();
    *d_cancel_ = true;
    *d_stream_cancel_ = true;
    d_cancel_ = std::make_shared<std::atomic<bool>>(false);
    const Item *found = nullptr;
    const auto look = [&](const std::vector<Item> &items) {
        if (found)
            return;
        for (const Item &it : items)
            if (it.id == t.id && (t.type.empty() || it.type == t.type))
            {
                found = &it;
                return;
            }
    };
    for (const BoardRow &br : board_)
        look(br.items);
    for (const BoardRow &br : search_)
        look(br.items);
    look(disc_items_);
    look(lib_items_);
    Item it;
    if (found)
    {
        it = *found;
    }
    else
    {
        it.id = t.id;
        it.type = t.type;
        it.name = t.name;
    }

    d_item_ = it;
    d_meta_ = ::Meta();
    d_meta_loaded_ = false;
    d_season_videos_.clear();
    d_seasons_.clear();
    d_streams_.clear();
    d_found_.clear();
    d_video_id_.clear();
    ++d_gen_;
    ++d_meta_gen_;
    d_open_ = true;
    want_streams_video_ = streams_video;
    resume_play_ = false;

    detail = DetailContent();
    detail_fill_title();

    detail.series = it.type == "series";
    detail.episodes_status = "Loading episodes...";
    detail.streams_status = "";
    changed_ |= kDetail;
    art_dirty_ = true;

    // The details come from the first addon that has them.
    std::vector<std::string> urls;
    for (const auto &a : addons_)
        if (a->supports("meta", it.type, it.id))
            urls.push_back(a->resource_url("meta", it.type, it.id));
    const int gen = d_meta_gen_;
    const auto cancel = d_cancel_;
    struct Res
    {
        bool ok = false;
        ::Meta meta;
        std::string error;
    };
    bg<Res>(
        [urls, cancel]()
        {
            Res r;
            for (const auto &u : urls)
            {
                json j;
                std::string err;
                if (!fetch_json(u, j, err, 10, cancel.get()))
                {
                    r.error = err;
                    continue;
                }
                const json &m = jobj(j, "meta");
                if (!m.is_object())
                    continue;
                r.meta = parse_meta(m);
                r.ok = true;
                break;
            }
            if (urls.empty())
                r.error = "no addon has details for this";
            return r;
        },
        [this, gen](Res &r)
        {
            if (gen != d_meta_gen_ || !d_open_)
                return;
            if (!r.ok)
            {
                detail.episodes_status = "Could not load the details: " + r.error;
                // Without details it can still be played as one video.
                detail.series = false;
                detail_load_streams(d_item_.id, "Streams");
                changed_ |= kDetail;
                if (resume_play_)
                    start_remembered_stream();
                return;
            }
            d_meta_ = r.meta;
            if (d_meta_.id.empty())
                d_meta_.id = d_item_.id;
            if (d_meta_.type.empty())
                d_meta_.type = d_item_.type;
            d_meta_loaded_ = true;
            detail_apply_meta();
            if (resume_play_)
                start_remembered_stream();
        },
        true);

    if (!detail.series)
        detail_load_streams(it.id, "Streams"); // a film: its streams right away
}

// Continue Watching, "Play now": the details are in, so the stream that was playing is played again.
void Session::start_remembered_stream()
{
    resume_play_ = false;
    ::Stream s;
    // A series' stream belongs to one episode; a film has one video, whatever it is called.
    const bool series = d_item_.type == "series";
    if (saved_stream(d_item_.id, series ? resume_video_ : std::string(), &s))
    {
        const auto p = progress_.find(d_item_.id);
        d_video_id_ = series || p == progress_.end() || p->second.stream_video_id.empty() ? resume_video_
                                                                                          : p->second.stream_video_id;
        play_stream(s, false);
        return;
    }
    // Nothing usable remembered after all: leave the launch screen, the page shows the streams.
    resuming_ = false;
    resumed_from_board_ = false;
    watch_stop(false);
}
void Session::close_detail()
{
    *d_cancel_ = true;
    *d_stream_cancel_ = true;
    detail_preparation_cancel();
    ++d_gen_;
    ++d_meta_gen_;
    d_open_ = false;
}

void Session::detail_fill_title()
{
    Title &t = detail.title;
    const ::Meta &m = d_meta_;
    t.id = d_item_.id;
    t.type = d_item_.type;
    t.name = d_meta_loaded_ && !m.name.empty() ? m.name : d_item_.name;
    t.year = d_meta_loaded_ ? m.release_info : d_item_.release_info;
    t.runtime = d_meta_loaded_ ? m.runtime : d_item_.runtime;
    t.genres = join(d_meta_loaded_ ? m.genres : d_item_.genres, ", ");
    t.imdb = d_meta_loaded_ ? m.imdb_rating : d_item_.imdb_rating;
    t.synopsis = d_meta_loaded_ ? m.description : d_item_.description;
    if (d_meta_loaded_)
    {
        std::vector<std::string> cast = m.cast;
        if (cast.size() > 6)
            cast.resize(6);
        detail.cast = join(cast, ", ");
        detail.directors = join(m.directors, ", ");
    }
}

void Session::detail_apply_meta()
{
    const ::Meta &m = d_meta_;
    if (d_item_.poster.empty())
        d_item_.poster = m.poster;
    if (d_item_.background.empty())
        d_item_.background = m.background;
    detail_fill_title();

    // Remote progress may identify an episode by season/episode rather than
    // this addon's video ID. Resolve it before Continue Watching opens streams.
    const auto remote = library_.find(d_item_.id);
    if (remote != library_.end())
    {
        json& state = remote->second["state"];
        const int season = int(jnum(state, "nuvioSeason", -1));
        const int episode = int(jnum(state, "nuvioEpisode", -1));
        const auto previous = jstr(state, "video_id");
        bool known = false;
        for (const auto& video : m.videos) known = known || video.id == previous;
        if (!known && season >= 0 && episode > 0)
            for (const auto& video : m.videos) if (video.season == season && video.episode == episode)
            {
                state["video_id"] = video.id;
                if (resume_video_ == previous) resume_video_ = video.id;
                if (want_streams_video_ == previous) want_streams_video_ = video.id;
                break;
            }
    }

    // Series-like when there are several videos (series, channels, anime).
    bool series = m.type != "movie" && m.videos.size() > 1;
    if (m.type == "series" && !m.videos.empty())
        series = true;
    if (series)
    {
        detail.series = true;
        d_seasons_.clear();
        for (const auto &v : m.videos)
            if (std::find(d_seasons_.begin(), d_seasons_.end(), v.season) == d_seasons_.end())
                d_seasons_.push_back(v.season);
        // Specials (season 0) last.
        std::stable_sort(d_seasons_.begin(), d_seasons_.end(), [](int a, int b) {
            if (a == 0)
                return false;
            if (b == 0)
                return true;
            return a < b;
        });
        detail.seasons.clear();
        for (int s : d_seasons_)
            detail.seasons.push_back(s == 0 ? "Specials" : "Season " + std::to_string(s));
        // Start on the season being watched, else the first.
        int season_idx = 0;
        std::string resume_video;
        for (const Item &cw : continue_watching())
            if (cw.id == d_item_.id)
                resume_video = cw.video_id;
        if (resume_video.empty() && progress_.count(d_item_.id))
            resume_video = progress_[d_item_.id].video_id;
        int want_episode = -1;
        for (const auto &v : m.videos)
            if (v.id == resume_video)
            {
                for (std::size_t i = 0; i < d_seasons_.size(); ++i)
                    if (d_seasons_[i] == v.season)
                        season_idx = static_cast<int>(i);
                want_episode = v.episode;
            }
        detail_set_season(season_idx);
        if (want_episode >= 0)
            for (std::size_t i = 0; i < d_season_videos_.size(); ++i)
                if (d_season_videos_[i]->episode == want_episode)
                {
                    detail.episode_hint = static_cast<int>(i);
                    ++detail.hint_serial;
                }
        // Asked for from Continue Watching: that episode's streams, without the list of episodes.
        if (!want_streams_video_.empty() && !resume_play_)
        {
            for (const auto &v : m.videos)
                if (v.id == want_streams_video_)
                {
                    std::string heading = "S" + std::to_string(v.season) + " E" + std::to_string(v.episode);
                    if (!v.title.empty())
                        heading += "  \xC2\xB7  " + v.title;
                    detail_load_streams(v.id, heading);
                    ++detail.streams_serial;
                    break;
                }
        }
        want_streams_video_.clear();
    }
    else
    {
        const bool was_series = detail.series;
        detail.series = false;
        detail.episodes.clear();
        const std::string vid = m.videos.size() == 1 ? m.videos[0].id : m.id;
        if (was_series || vid != d_video_id_)
            detail_load_streams(vid, "Streams");
    }
    changed_ |= kDetail;
    art_dirty_ = true;
}

void Session::detail_set_season(int index)
{
    if (d_seasons_.empty())
        return;
    index = std::max(0, std::min(index, static_cast<int>(d_seasons_.size()) - 1));
    d_season_idx_ = index;
    detail.season = index;
    const int season = d_seasons_[static_cast<std::size_t>(index)];
    d_season_videos_.clear();
    for (const auto &v : d_meta_.videos)
        if (v.season == season)
            d_season_videos_.push_back(&v);
    // What was left half-watched, shown on its episode.
    std::string resume_video;
    float resume_progress = -1.0f;
    for (const Item &cw : continue_watching())
        if (cw.id == d_item_.id && cw.duration > 0)
        {
            resume_video = cw.video_id;
            resume_progress = static_cast<float>(std::min(1.0, cw.offset / cw.duration));
        }
    detail.episodes.clear();
    int first_unwatched = -1;
    for (std::size_t i = 0; i < d_season_videos_.size(); ++i)
    {
        const ::Video *v = d_season_videos_[i];
        Episode e;
        e.number = v->episode;
        e.title = episode_title(*v);
        e.info = date_label(v->released);
        e.synopsis = v->overview;
        e.watched = watched(v->id);
        if (v->id == resume_video)
            e.progress = resume_progress;
        detail.episodes.push_back(e);
        if (!e.watched && first_unwatched < 0)
            first_unwatched = static_cast<int>(i);
    }
    detail.episode_hint = first_unwatched >= 0 ? first_unwatched : 0;
    ++detail.hint_serial;
    detail.episodes_status = detail.episodes.empty() ? "No episodes in this season." : "";
    changed_ |= kDetail;
    art_dirty_ = true;
}

void Session::detail_change_season(int delta)
{
    if (d_seasons_.empty())
        return;
    const int next = d_season_idx_ + delta;
    if (next < 0 || next >= static_cast<int>(d_seasons_.size()))
        return;
    detail_set_season(next);
}

void Session::detail_pick_episode(int index)
{
    if (index < 0 || index >= static_cast<int>(d_season_videos_.size()))
        return;
    const ::Video *v = d_season_videos_[static_cast<std::size_t>(index)];
    std::string heading = "S" + std::to_string(v->season) + " E" + std::to_string(v->episode);
    if (!v->title.empty())
        heading += "  \xC2\xB7  " + v->title;
    detail_load_streams(v->id, heading);
}

void Session::detail_load_streams(const std::string &video_id, const std::string &heading,
                                  const std::vector<::Stream>* prefetched)
{
    detail_preparation_cancel();
    *d_stream_cancel_ = true;
    d_stream_cancel_ = std::make_shared<std::atomic<bool>>(false);
    const int gen = ++d_gen_;
    d_video_id_ = video_id;
    d_stream_heading_ = heading;
    d_found_.clear();
    d_streams_.clear();
    detail.streams.clear();
    detail.stream_addons.clear();
    detail.resume_label.clear();
    double resume = 0;
    for (const Item &cw : continue_watching())
        if (cw.id == d_item_.id && !cw.up_next && (cw.video_id.empty() || cw.video_id == video_id || !detail.series) &&
            cw.offset > 5 && (cw.duration <= 0 || cw.offset < cw.duration*0.95))
            resume = cw.offset;
    if (resume > 0)
    {
        const int seconds = static_cast<int>(resume);
        char label[64];
        std::snprintf(label, sizeof(label), "Resume from %d:%02d", seconds / 60, seconds % 60);
        detail.resume_label = label;
    }
    detail.stream_heading = heading;
    d_answered_ = 0;
    d_stream_errors_=false;
    d_cached_sources_.clear();

    std::string type = d_meta_loaded_ ? d_meta_.type : d_item_.type;
    if (type.empty())
        type = d_item_.type;
    std::vector<::Addon> sources;
    for (const auto &a : addons_)
        for (auto source : providers::stream_sources(*a, type, video_id))
        {
            detail.stream_addons.push_back(source.name);
            sources.push_back(std::move(source));
        }
    d_pending_ = static_cast<int>(sources.size());
    json source_identity=json::array();
    for(const auto& a:sources) {
        json scripts=json::array();
        for(const auto& p:a.providers)scripts.push_back(json::array({p.id,p.version,p.filename,p.settings}));
        source_identity.push_back(json::array({a.transport_url,a.version,scripts}));
    }
    d_stream_cache_key_=json::array({profile_dir_,type,video_id,source_identity}).dump();
    detail.streams_loading = d_pending_ > 0;
    if (prefetched)
    {
        d_found_ = *prefetched;
        d_answered_ = d_pending_;
        detail_refresh_streams();
        ++detail.streams_serial;
        return;
    }
    if (sources.empty())
    {
        autoplay_pending_ = false;
        detail.streams_status = addons_loading_ ? "Addons are still loading." :
                                                  "No enabled addons or plugins provide streams for this title. "
                                                  "Manage addons and enable providers on the Addons page.";
        changed_ |= kDetail;
        return;
    }
    detail.streams_status = "Loading streams... (0 of " + std::to_string(sources.size()) + " sources answered)";
    d_found_=stream_result_cache_.get(d_stream_cache_key_,now_seconds());
    for(const auto& s:d_found_)d_cached_sources_.insert(s.addon);
    if(!d_found_.empty())detail_refresh_streams();
    for (const auto &s : sources)
    {
        const std::string name = s.name;
        const auto cancel = d_stream_cancel_;
        const auto meta = d_meta_loaded_ ? d_meta_ : ::Meta{};
        struct Res
        {
            std::vector<::Stream> streams;
            std::string error;
        };
        auto work = [s, type, video_id, meta, cancel]()
        {
            try {
                auto found = providers::fetch(s, type, video_id, meta, cancel.get());
                return Res{std::move(found.streams), std::move(found.error)};
            } catch (...) {return Res{{},"Provider search failed; try reloading this source"};}
        };
        auto done = [this, gen, name](Res &r)
        {
            if (gen != d_gen_)
                return;
            ++d_answered_;
            // Replace a cached source's results with its fresh answer, including
            // empty/error responses. Never leave old links after a refresh.
            if(d_cached_sources_.erase(name))
                d_found_.erase(std::remove_if(d_found_.begin(),d_found_.end(),[&](const auto& s){return s.addon==name;}),d_found_.end());
            if (!r.error.empty())
            {
                d_stream_errors_=true;
                dlog("streams from %s: %s", name.c_str(), r.error.c_str());
            }
            dlog("streams: source %s returned %zu playable candidates", name.c_str(), r.streams.size());
            for (auto &st : r.streams)
                d_found_.push_back(st);
            detail_refresh_streams();
            if(d_answered_==d_pending_)
                stream_result_cache_.put(d_stream_cache_key_,d_stream_errors_ ? std::vector<::Stream>{} : d_found_,now_seconds());
            if (!r.error.empty() && d_streams_.empty())
                say(name + ": " + r.error, 6.0, true);
        };
        if (s.provider_repository)
            provider_tasks_.run<Res>(work, done);
        else
            bg<Res>(work, done);
    }
    changed_ |= kDetail;
}

void Session::detail_reload_streams()
{
    if (d_open_ && !d_video_id_.empty()) detail_load_streams(d_video_id_, d_stream_heading_);
}

void Session::detail_refresh_streams()
{
    detail.streams_loading = d_answered_ < d_pending_;
    // Direct/debrid streams first. Raw torrents appear only when enabled.
    std::vector<std::size_t> order;
    std::size_t hidden_torrents = 0;
    for (std::size_t i = 0; i < d_found_.size(); ++i)
    {
        auto& stream = d_found_[i];
        stream.debrid_resolvable = services_.can_resolve(stream);
        stream.debrid_service = stream.debrid_resolvable ? (stream.client_resolve.is_object()
            ? debrid::provider_id(jstr(stream.client_resolve, "service")) : services_.active_provider()) : "";
        if (stream.allowed(prefs_.builtin_torrents) || stream.debrid_resolvable) order.push_back(i);
        else if (stream.torrent() && !prefs_.builtin_torrents) ++hidden_torrents;
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (d_found_[a].direct_http() != d_found_[b].direct_http())
            return d_found_[a].direct_http();
        const int ra = resolution_rank(resolution_of(d_found_[a])), rb = resolution_rank(resolution_of(d_found_[b]));
        if (ra != rb)
            return ra < rb;
        return seeds_of(d_found_[a].description) > seeds_of(d_found_[b].description);
    });
    d_streams_.clear();
    detail.streams.clear();
    for (std::size_t i : order)
    {
        d_streams_.push_back(d_found_[i]);
        detail.streams.push_back(view_of(d_found_[i]));
    }
    detail_preparation_labels();
    // Wait for every addon before matching the previous release and quality.
    if (autoplay_pending_ && d_answered_ >= d_pending_)
    {
        const int pick = episode_playback::select_stream(d_streams_, autoplay_previous_);
        autoplay_pending_ = false;
        if (pick >= 0)
        {
            // play_stream remembers this selection; do not pass a reference into
            // d_streams_ across UI updates.
            const ::Stream selected = d_streams_[static_cast<std::size_t>(pick)];
            play_stream(selected, false);
        }
        else
            say("Choose a stream for the next episode. No matching direct/debrid quality was found.", 7.0);
    }
    if (d_answered_ < d_pending_)
        detail.streams_status = "Loading streams... (" + std::to_string(d_answered_) + " of " + std::to_string(d_pending_) +
                                " sources answered)";
    else if (detail.streams.empty() && hidden_torrents)
        detail.streams_status = std::to_string(hidden_torrents) +
            " torrent results hidden. Enable Peer-to-peer torrents in Settings, or connect a compatible debrid service.";
    else if (detail.streams.empty())
        detail.streams_status = prefs_.builtin_torrents ? "No streams found. Check your stream addons in Nuvio." :
            "No playable streams returned. Check your addon setup or connect TorBox / Premiumize in Settings > Connected Services.";
    else
        detail.streams_status.clear();
    changed_ |= kDetail;
}

} // namespace sx
