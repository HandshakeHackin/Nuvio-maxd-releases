// Playing: starting a stream (the app's own torrent engine, a streaming server or a
// direct link), the player's state for the screens, tracks, subtitles, and what
// was watched.

#include "session.hpp"
#include "track_restore.hpp"
#include "trailers.h"
#include "diagnostics.h"
#include "stream_json.hpp"
#include "system_status.hpp"
#include "hdr_output.hpp"
#include "stream_display.hpp"
#include "playback_badges.hpp"

#include "http.h"
#include "torrent/engine.h"
#include "torrent/torrent_stream.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>

namespace sx
{

namespace
{
std::string badge_metadata(const ::Stream& stream) {
    return stream_display::plain(stream.name.substr(0,2048)+" "+stream.description.substr(0,4096)+" "+stream.filename.substr(0,2048));
}

constexpr double kInfoSeconds = 5.0;
constexpr double kSaveEvery = 30.0;

std::string random_id()
{
    static const char *hex = "0123456789abcdef";
    std::string s;
    std::uint64_t x = static_cast<std::uint64_t>(now_epoch_ms()) * 6364136223846793005ULL + static_cast<std::uint64_t>(std::rand());
    for (int i = 0; i < 16; ++i)
    {
        s += hex[x & 15];
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
    }
    return s;
}

// A cue's markup (<em>, <strong>, <br/>, entities) as the plain lines the player draws.
std::string plain_cue(const std::string &rml)
{
    std::string out;
    for (std::size_t i = 0; i < rml.size();)
    {
        if (rml[i] == '<')
        {
            const std::size_t end = rml.find('>', i);
            if (end == std::string::npos)
                break;
            if (rml.compare(i, 3, "<br") == 0)
                out += '\n';
            i = end + 1;
        }
        else if (rml[i] == '&')
        {
            const std::size_t end = rml.find(';', i);
            const std::string entity = end == std::string::npos ? "" : rml.substr(i, end - i + 1);
            if (entity == "&amp;")
                out += '&';
            else if (entity == "&lt;")
                out += '<';
            else if (entity == "&gt;")
                out += '>';
            else if (entity == "&quot;")
                out += '"';
            else if (entity == "&apos;" || entity == "&#39;")
                out += '\'';
            else
            {
                out += rml[i];
                ++i;
                continue;
            }
            i = end + 1;
        }
        else
        {
            out += rml[i++];
        }
    }
    return out;
}

} // namespace

std::string Session::server() const
{
    std::string s = trim(prefs_.server_url);
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    if (!s.empty() && !starts_with(s, "http://") && !starts_with(s, "https://"))
        s = "http://" + s;
    return s;
}

std::vector<std::string> Session::pref_langs(const std::string &list) const
{
    std::vector<std::string> out;
    for (const auto &part : split(list, ','))
    {
        const std::string p = trim(part);
        if (p.empty() || p == "-")
            continue;
        out.push_back(language_to_iso639_2(p));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Starting a stream

void Session::detail_play(int index, bool with_server)
{
    if (index < 0 || index >= static_cast<int>(d_streams_.size()))
        return;
    autoplay_pending_ = false;
    play_stream(d_streams_[static_cast<std::size_t>(index)], with_server);
}

void Session::play_stream(const ::Stream &st, bool transcode, bool trailer)
{
    const bool was_resuming = resuming_;
    const auto refuse = [&](const char *message) {
        if (was_resuming)
        {
            resuming_ = false;
            resumed_from_board_ = false;
            watch_stop(false);
        }
        say(message, 6.0, true);
    };
    const std::string srv = server();
    const bool resolving = services_.can_resolve(st);
    if (!st.allowed(prefs_.builtin_torrents) && !resolving)
    {
        refuse("Choose a direct/debrid stream. Peer-to-peer torrents are off in Settings.");
        return;
    }
    const bool torrent = st.torrent() && !resolving;
    // Torrents play with the app's own engine, unless Settings says to use the
    // server; transcoding is always the server's.
    const bool builtin = torrent && !transcode && (prefs_.builtin_torrents || srv.empty());
    if (torrent && !builtin && srv.empty())
    {
        refuse("Torrent streams need your Nuvio streaming server: set it in Settings.");
        return;
    }
    if (transcode && srv.empty())
    {
        refuse("Transcoding needs your Nuvio streaming server: set it in Settings.");
        return;
    }
    if (watching && !resuming_)
        return;
    resuming_ = false;
    w_trailer_=trailer;

    // What is being played, for progress and the player's titles.
    w_item_ = d_item_;
    if (d_meta_loaded_)
    {
        w_item_.name = d_meta_.name.empty() ? d_item_.name : d_meta_.name;
        w_item_.type = d_meta_.type.empty() ? d_item_.type : d_meta_.type;
        if (!d_meta_.poster.empty())
            w_item_.poster = d_meta_.poster;
        if (!d_meta_.background.empty())
            w_item_.background = d_meta_.background;
    }
    w_video_id_ = w_trailer_?std::string():d_video_id_;
    w_stream_ = st;
    // Live channels and trailers never persist stream URLs in title history.
    if (trailers::records_history(w_trailer_,w_item_.type))
    {
        Progress &p = progress_[w_item_.id];
        p.type = w_item_.type;
        p.name = w_item_.name;
        p.poster = w_item_.poster;
        p.stream_json = stream_to_json(st).dump();
        p.stream_video_id = w_video_id_;
        save_progress();
    }

    std::string title = w_item_.name, subtitle;
    int season = -1, episode = -1;
    if (detail.series && !w_trailer_)
    {
        for (const auto &v : d_meta_.videos)
            if (v.id == w_video_id_)
            {
                season = v.season;
                episode = v.episode;
                subtitle = "S" + std::to_string(v.season) + " E" + std::to_string(v.episode) +
                           (v.title.empty() ? "" : "  \xC2\xB7  " + v.title);
            }
    }
    else
    {
        subtitle = detail.title.year;
    }

    // Adopt the selected request before cancelling the other browsing candidates.
    w_season_ = season; w_episode_number_ = episode;
    if (resolving) prepared_->promote(st, season, episode);
    detail_preparation_cancel();

    // Resume where it was left.
    double start = 0;
    if(!w_trailer_) for (const Item &cw : continue_watching())
    {
        if (cw.id != w_item_.id)
            continue;
        const bool same_video = cw.video_id.empty() || cw.video_id == w_video_id_ || !detail.series;
        if (same_video && cw.offset > 5 && (cw.duration <= 0 || cw.offset < cw.duration * 0.95))
            start = cw.offset;
    }

    if(iptv::library_type(w_item_.type))start=iptv_resume(w_item_.id);
    if(w_trailer_)subtitle="Trailer";
    const int gen = ++w_gen_;
    launch = LaunchState();
    launch.title = title;
    launch.subtitle = subtitle;
    launch.source = st.addon;
    {
        const Stream v = stream_view(st);
        launch.quality = v.resolution;
        if (!v.tags.empty())
            launch.quality += "  \xC2\xB7  " + v.tags;
        if (!v.size.empty())
            launch.quality += "  \xC2\xB7  " + v.size;
    }
    launch.stages = {"Finding sources", "Connecting to peers", "Filling the buffer", "Starting"};
    if(w_item_.type == "tv") launch.stages = {"Choosing channel", "Connecting", "Buffering live video", "Starting"};
    if(iptv::library_type(w_item_.type))launch.stages={"Opening IPTV title","Connecting to provider","Buffering video","Starting"};
    if (resolving) launch.stages = {"Checking service cache", "Resolving original file", "Filling the buffer", "Starting"};
    launch.stage = 0;
    launch.progress = -1.0f;
    launch.torrent = torrent;
    launch.hash = st.info_hash;
    launch_open = true;
    cue(hui::audio::Cue::launch);
    watching = true;
    nuvio_diagnostics::record({{"playing",false},{"phase","ready"}});
    playing = PlayerState();
    playing.title = title;
    playing.subtitle = subtitle;
    playing.badges = playback_badges::build(badge_metadata(st));
    playing.controls = true;
    w_episode_ = episode_playback::Flow();
    watch_reset_anime();
    w_eof_recorded_ = false;
    nuvio_diagnostics::record({{"outputFailed",false},{"playerFailed",false}});
    w_next_.reset();
    w_next_streams_.clear();
    w_next_requested_ = false;
    w_next_pending_ = w_next_answered_ = 0;
    w_next_pick_ = -1;
    w_episode_tick_ = now_seconds();
    if (w_episode_cancel_) *w_episode_cancel_ = true;
    w_episode_cancel_ = std::make_shared<std::atomic<bool>>(false);
    if(!w_trailer_ && w_item_.type != "tv" && !iptv::library_type(w_item_.type)) watch_load_segments(season, episode);
    if (detail.series && !w_trailer_)
        if (const auto* next = episode_playback::next_video(d_meta_.videos, w_video_id_, now_epoch_ms())) w_next_ = *next;
    art_dirty_ = true;

    struct Res
    {
        std::string url, error;
        std::vector<std::string> headers;
        int file_idx = -1;
    };
    if (builtin)
        torrent_stats_start(TorrentStream::make_url(st.info_hash, -1), st.info_hash);
    else if (torrent)
        torrent_stats_start(srv + "/" + st.info_hash + "/stats.json", st.info_hash);
    else
        t_visible_ = false;
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    launch_cancel_ = cancel;
    const ::Stream s = st;
    const auto prepared = prepared_;
    const auto services_epoch = prepared->epoch();
    const int services_gen = services_generation_;
    bg<Res>(
        [s, srv, torrent, builtin, transcode, season, episode, cancel, prepared, services_epoch, resolving]() {
            Res r;
            std::string media = s.url;
            if (!s.url.empty())
                r.headers = s.request_headers;
            if (resolving) {
                const auto resolved = prepared->resolve(s, season, episode, services_epoch, cancel);
                if (!resolved.ok) { r.error = resolved.error; return r; }
                media = resolved.stream.url;
                r.headers = resolved.stream.request_headers;
            }
            if (builtin)
            {
                // The app's own engine: start it, wait for the file list (the peers
                // send it when the addon gave only the hash), pick the file and play
                // it from the engine.
                bt::Engine &eng = bt::Engine::get();
                eng.start(s.info_hash, s.sources);
                std::vector<bt::FileInfo> files;
                std::string err;
                if (!eng.wait_metadata(s.info_hash, files, cancel.get(), 900, &err))
                {
                    if (err == "cancelled")
                        r.error = "cancelled";
                    else if (err == "timed out")
                        r.error = "No peers sent this torrent's file list in 15 minutes. Try another stream.";
                    else
                        r.error = "The torrent couldn't start (" + err + ").";
                    return r;
                }
                int idx = s.file_idx;
                if (idx < 0 || static_cast<std::size_t>(idx) >= files.size())
                    idx = bt::Engine::guess_file(files, season, episode);
                if (idx < 0)
                {
                    r.error = "This torrent has no video file.";
                    return r;
                }
                dlog("torrent %s: playing file %d, %s", s.info_hash.c_str(), idx, files[static_cast<std::size_t>(idx)].path.c_str());
                eng.select_file(s.info_hash, idx);
                r.url = TorrentStream::make_url(s.info_hash, idx);
                r.file_idx = idx;
                return r;
            }
            if (torrent)
            {
                // As Nuvio's own player does: with no trackers from the addon and a
                // known file, play the file URL straight away and let the server find
                // peers its own way. Only otherwise ask it to /create the torrent first.
                int idx = s.file_idx;
                std::vector<std::string> sources;
                for (const auto &x : s.sources)
                {
                    const std::string src = starts_with(x, "tracker:") || starts_with(x, "dht:") ? x : "tracker:" + x;
                    if (std::find(sources.begin(), sources.end(), src) == sources.end())
                        sources.push_back(src);
                }
                if (!sources.empty() || idx < 0)
                {
                    json body;
                    body["torrent"] = {{"infoHash", s.info_hash}};
                    if (!sources.empty())
                    {
                        std::vector<std::string> all = {"dht:" + s.info_hash};
                        for (const auto &x : sources)
                            if (x != all[0])
                                all.push_back(x);
                        sources = all;
                        body["peerSearch"] = {{"sources", sources}, {"min", 40}, {"max", 200}};
                    }
                    if (idx < 0)
                    {
                        json guess = json::object();
                        if (season >= 0)
                            guess["season"] = season;
                        if (episode >= 0)
                            guess["episode"] = episode;
                        body["guessFileIdx"] = guess;
                    }
                    else
                    {
                        body["guessFileIdx"] = false;
                    }
                    // With few seeders the server may need minutes to get the torrent's
                    // metadata: wait 15 minutes (Circle cancels). A proxy in front of the
                    // server answers 502/503/504 when it tires of waiting: that means
                    // "not yet", so ask again.
                    const double deadline = now_seconds() + 900;
                    HttpResponse h;
                    for (;;)
                    {
                        const long left = static_cast<long>(deadline - now_seconds());
                        h = http_post_json(srv + "/" + s.info_hash + "/create", body.dump(), std::max(left, 1L), cancel.get());
                        const bool not_yet = h.error.empty() && h.status >= 502 && h.status <= 504;
                        if (!not_yet || cancel->load() || now_seconds() + 3 >= deadline)
                            break;
                        dlog("torrent: server answered HTTP %ld to /create, asking again", h.status);
                        for (int i = 0; i < 20 && !cancel->load(); ++i)
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    if (cancel->load())
                        h.error = "cancelled";
                    if (!h.error.empty() || !h.ok())
                    {
                        if (h.error == "cancelled")
                            r.error = "cancelled";
                        else if (h.error.find("timed out") != std::string::npos)
                            r.error = "Your streaming server found no peers for this torrent in 15 minutes. Try another stream.";
                        else
                            r.error = "Your streaming server: " + h.describe();
                        return r;
                    }
                    if (idx < 0)
                    {
                        const json j = json::parse(h.body, nullptr, false);
                        idx = static_cast<int>(jnum(j, "guessedFileIdx", 0));
                    }
                }
                media = srv + "/" + s.info_hash + "/" + std::to_string(idx);
                std::string query;
                for (const auto &x : sources)
                    query += (query.empty() ? "?tr=" : "&tr=") + url_encode(x);
                media += query;
                r.file_idx = idx;
                r.headers.clear();
            }
            if (transcode)
            {
                r.url = srv + "/hlsv2/" + random_id() + "/master.m3u8?mediaURL=" + url_encode(media) +
                        "&videoCodecs=h264&audioCodecs=aac&audioCodecs=mp3&maxAudioChannels=2";
                r.headers.clear();
            }
            else
            {
                r.url = media;
            }
            return r;
        },
        [this, gen, start, title, subtitle, srv, s, builtin, resolving, services_gen](Res &r) {
            if (gen != w_gen_ || !launch_open)
                return;
            if (resolving && services_gen != services_generation_) {
                watch_stop(false); say("Connected Services changed. Select the stream again.", 7.0); return;
            }
            if (!r.error.empty())
            {
                watch_stop(false);
                if (r.error != "cancelled")
                    say(r.error, 8.0, true);
                return;
            }
            if (builtin)
                t_stats_url_ = r.url;
            else if (r.file_idx >= 0)
                t_stats_url_ = srv + "/" + s.info_hash + "/" + std::to_string(r.file_idx) + "/stats.json";
            // Several connections and the long read-ahead for big files: direct links
            // and the streaming server's torrent streams. Not for transcoding, which is
            // a playlist of pieces.
            const bool par = !w_trailer_ && w_item_.type != "tv" && !iptv::library_type(w_item_.type) && r.url.find("/hlsv2/") == std::string::npos;
            watch_start(r.url, r.headers, start, title, subtitle, par);
        });
}

void Session::watch_start(const std::string &url, const std::vector<std::string> &headers, double start,
                          const std::string &title, const std::string &subtitle, bool direct)
{
    Player::Options o;
    o.url = url;
    o.parallel = direct;
    o.live_tv = w_item_.type == "tv";
    o.read_ahead_seconds = w_trailer_ || w_item_.type == "tv" ? 2 : prefs_.read_ahead_seconds;
    o.evo = prefs_.evo_player;
    o.vlc = prefs_.vlc_player;
    o.surround = prefs_.surround_audio;
    o.audio_mode=prefs_.audio_gain_percent==100?prefs_.audio_output_mode:"pcm";
    o.gain_percent=prefs_.audio_gain_percent;o.audio_delay=prefs_.audio_delay_ms/1000.0;
    o.headers = headers;
    o.start = start;
    o.audio_langs = pref_langs(prefs_.audio_langs);
    nuvio_diagnostics::record({{"engine",o.vlc?"vlc":o.evo?"evo":"native"},{"evo",o.evo},{"nativeHdr",prefs_.native_hdr},{"phase","playback"},{"playing",false}});
    player_.open(o);

    playing.title = title;
    playing.subtitle = subtitle;
    playing.position = start;
    playing.duration = 0;
    playing.paused = false;
    playing.buffering = -1;
    playing.subtitle_style = prefs_.subtitle_style;
    playing.sub_size = prefs_.sub_size == "sub-s" ? 0 : prefs_.sub_size == "sub-l" ? 2 : 1;
    w_started_ = false;
    w_heavy_warned_ = false;
    w_menu_built_ = false;
    w_player_revision_=0;w_chapters_at_=0;
    playing.chapters.clear();playing.chapter_labels.clear();playing.chapter_active=-1;
    w_auto_sub_done_ = false;
    w_subs_.clear();
    w_sub_active_ = -1;
    w_sub_delay_ = prefs_.subtitle_delay_ms/1000.0;
    subtitle_shown_.clear();
    w_last_save_ = now_seconds();
    w_last_saved_pos_ = start;
    w_info_until_ = now_seconds() + kInfoSeconds;
    launch.stage = 1;
    if(!w_trailer_ && w_item_.type != "tv" && !iptv::library_type(w_item_.type)) watch_load_addon_subtitles();
}

// ---------------------------------------------------------------------------
// The torrent's statistics (peers, speed, how much of the file is there)

void Session::torrent_stats_start(const std::string &url, const std::string &info_hash)
{
    ++t_gen_;
    t_stats_url_ = url;
    t_peers_ = 0;
    t_known_ = -1;
    t_speed_ = 0;
    t_progress_ = -1;
    t_has_meta_ = false;
    t_visible_ = true;
    t_next_poll_ = 0;
    t_polling_ = false;
    launch.hash = info_hash;
}

void Session::torrent_stats_poll()
{
    if (t_polling_ || now_seconds() < t_next_poll_)
        return;
    t_polling_ = true;
    const int gen = t_gen_;
    const std::string url = t_stats_url_;
    struct Stats
    {
        bool ok = false;
        double peers = 0, speed = 0, progress = -1;
        int known = -1; // the app's own engine only
        bool has_meta = false;
    };
    bg<Stats>(
        [url]() {
            Stats s;
            if (TorrentStream::is_url(url))
            { // the app's own engine
                const bt::Stats es = bt::Engine::get().stats(url.substr(10, 40));
                s.ok = es.found;
                s.peers = es.peers;
                s.speed = es.download_rate;
                s.progress = es.file_progress;
                s.known = es.known_peers;
                s.has_meta = es.has_metadata;
                return s;
            }
            const HttpResponse h = http_get(url, 5);
            if (!h.ok())
                return s;
            const json j = json::parse(h.body, nullptr, false);
            if (!j.is_object())
                return s;
            s.ok = true;
            s.peers = jnum(j, "peers");
            s.speed = jnum(j, "downloadSpeed");
            if (j.contains("streamProgress"))
                s.progress = jnum(j, "streamProgress");
            return s;
        },
        [this, gen](Stats &s) {
            t_polling_ = false;
            t_next_poll_ = now_seconds() + 1;
            if (gen != t_gen_ || !s.ok)
                return;
            t_peers_ = static_cast<int>(s.peers);
            t_speed_ = s.speed;
            if (s.progress >= 0)
                t_progress_ = s.progress;
            t_known_ = s.known;
            t_has_meta_ = s.has_meta;
        });
}

// ---------------------------------------------------------------------------
// While it plays

void Session::watch_show_info()
{
    w_info_until_ = now_seconds() + kInfoSeconds;
}

void Session::watch_update()
{
    if(multiview_active){multi_update();return;}
    const double now = now_seconds();
    const Player::State st = player_.state();
    player_.log_stats();
    // The PlayStation's menu is over the app (the PS button), or the app is in the background: pause.
    // It stays paused until Cross.
    {
        const SystemStatus sys = read_system_status();
        if (sys.valid && (sys.overlaid || sys.background) && w_started_ && !player_.paused())
        {
            player_.set_paused(true);
            watch_save_progress(false);
            dlog("playback paused: the system menu is over the app");
        }
    }

    if (st == Player::State::Failed)
    {
        const std::string err = player_.error();
        nuvio_diagnostics::record(player_.diagnostic_snapshot());
        watch_stop(false);
        nuvio_diagnostics::record({{"playerFailed",true}});
        say(iptv::library_type(w_item_.type)
                ? iptv::failure_message(err)
                : "Playback failed: " + err +
                      (w_stream_.torrent() ? ". The torrent may have no seeders; try another stream." : ""),
            iptv::library_type(w_item_.type) ? 14.0 : 8.0, true);
        return;
    }
    if (st == Player::State::Ended)
    {
        if(!w_eof_recorded_) { watch_save_progress(true, true); w_eof_recorded_=true; }
        const bool next = !w_trailer_ && prefs_.autoplay_next && detail.series && w_next_.has_value() && !w_episode_.cancelled;
        if(next) {
            playing.position=playing.duration=player_.duration(); playing.controls=true;
            playing.paused=player_.paused(); playing.buffering=-1;
            watch_episode_update(now,player_.duration(),player_.duration(),!player_.paused() && !playing.menu,true);
        } else watch_stop(true);
        return;
    }

    int pct = 0;
    const bool buffering = player_.buffering(&pct);
    if (t_visible_ && (launch_open || buffering))
        torrent_stats_poll();

    if (launch_open)
    {
        // Say what is going on. Before the file is requested the torrent engine
        // knows how far it has got; then the player opens it and fills its buffer.
        if (st == Player::State::Idle)
        {
            launch.stage = t_known_ <= 0 ? 0 : (t_has_meta_ ? 2 : 1);
        }
        else if (st == Player::State::Opening)
        {
            launch.stage = 1;
        }
        else if (buffering)
        {
            launch.stage = 2;
            launch.progress = static_cast<float>(pct) / 100.0f;
        }
        else
        {
            launch.stage = 3;
            launch_open = false;
            w_started_ = true;
        }
    }
    launch.peers = std::to_string(t_peers_);
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f MB/s", t_speed_ / (1024.0 * 1024.0));
        launch.speed = buf;
        if (t_progress_ >= 0)
        {
            std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(t_progress_ * 100.0 + 0.5));
            launch.done = buf;
        }
    }

    if(st==Player::State::Playing && (!w_menu_built_ || w_player_revision_!=player_.revision()))
        watch_refresh_tracks();

    const double pos = player_.position();
    const double dur = player_.duration();
    if(st==Player::State::Playing && now>=w_chapters_at_) {
        w_chapters_at_=now+1;
        auto chapters=player_.chapters();
        if(chapters.size()<2)chapters.clear();
        if(chapters.size()!=playing.chapters.size() || !std::equal(chapters.begin(),chapters.end(),playing.chapters.begin(),
            [](const auto& a,const auto& b){return a.start==b.start && a.title==b.title;})) {
            playing.chapters=std::move(chapters);
            w_menu_dirty_=true;
        }
    }
    playing.chapter_active=playback::current_chapter(playing.chapters,pos);

    if (watch_episode_update(now, pos, dur, w_started_ && !buffering && !player_.paused() && !playing.menu))
        return;

    // The subtitle on screen.
    std::string sub;
    int rich_id=-1;
    if (w_sub_active_ >= 0 && w_sub_active_ < static_cast<int>(w_subs_.size()))
    {
        WatchSub &ws = w_subs_[static_cast<std::size_t>(w_sub_active_)];
        rich_id=ws.embedded>=0?ws.embedded:ws.rich_id;
        const double t = pos + player_.video_sync_offset() - w_sub_delay_;
        if (ws.embedded >= 0)
            sub = player_.embedded_subtitle(ws.embedded, t);
        else if (!ws.cues.empty())
            sub = cues_at(ws.cues, t);
    }
    int embedded_stream=-1;
    if(w_sub_active_>=0 && w_sub_active_<int(w_subs_.size())) embedded_stream=w_subs_[size_t(w_sub_active_)].embedded;
    if(st==Player::State::Playing)player_.select_embedded_subtitle(embedded_stream,w_sub_delay_);
    const auto& style=prefs_.subtitle_style;
    auto overlay=player_.subtitle_renderer().render(rich_id,pos+player_.video_sync_offset()-w_sub_delay_,video_w,video_h,video_sar,
        playing.controls?125.0f:0.0f,style.font,style.colour,style.effect,style.background,
        prefs_.sub_size=="sub-s"?0:prefs_.sub_size=="sub-l"?2:1);
    if(!overlay.supported && rich_id < -1 && w_sub_active_>=0 && w_sub_active_<int(w_subs_.size())) {
        auto& selected=w_subs_[size_t(w_sub_active_)];
        if(!selected.rich_text.empty() && player_.subtitle_renderer().external(rich_id,selected.rich_text,selected.lang))
            overlay=player_.subtitle_renderer().render(rich_id,pos+player_.video_sync_offset()-w_sub_delay_,video_w,video_h,video_sar,
                playing.controls?125.0f:0.0f,style.font,style.colour,style.effect,style.background,
                prefs_.sub_size=="sub-s"?0:prefs_.sub_size=="sub-l"?2:1);
    }
    if(overlay.supported) sub.clear();
    if(overlay.changed) {
        playing.subtitle_texture=0;
        if(!overlay.rgba.empty()) {
            if(subtitle_texture_ && (overlay.w!=subtitle_width_ || overlay.h!=subtitle_height_)) {
                glDeleteTextures(1,&subtitle_texture_); subtitle_texture_=0;
            }
            if(!subtitle_texture_) subtitle_texture_=renderer_->batch().create_texture(overlay.w,overlay.h,overlay.rgba.data());
            else renderer_->batch().update_texture(subtitle_texture_,overlay.w,overlay.h,overlay.rgba.data());
            subtitle_width_=overlay.w; subtitle_height_=overlay.h;
            playing.subtitle_texture=subtitle_texture_;
            playing.subtitle_rect={float(overlay.x),float(overlay.y),float(overlay.w),float(overlay.h)};
        }
    }
    if (sub != subtitle_shown_)
    {
        subtitle_shown_ = sub;
        playing.line = plain_cue(sub);
    }

    playing.subtitle_style = prefs_.subtitle_style;
    playing.sub_size = prefs_.sub_size == "sub-s" ? 0 : prefs_.sub_size == "sub-l" ? 2 : 1;
    playing.position = pos;
    playing.duration = dur;
    playing.paused = player_.paused();
    playing.buffering = (buffering && w_started_) ? pct : -1;
    {
        char buf[96];
        if (t_visible_)
        {
            std::snprintf(buf, sizeof(buf), "%.1f MB/s  \xC2\xB7  %d peers", t_speed_ / (1024.0 * 1024.0), t_peers_);
            playing.net = buf;
        }
        else
        {
            playing.net.clear();
        }
    }
    std::string audio_label;
    for (const auto &t : player_.audio_tracks())
        if (t.stream == player_.audio_stream())
            audio_label = t.label;
    const auto next_audio=audio_label.empty()?"-":audio_label;
    if(playing.audio!=next_audio) {
        playing.audio=next_audio;
        if(!audio_label.empty())playing.badges.clear(); // Refresh selected-track facts on the next picture.
        if(video_w<=0)playing.badges=playback_badges::build(badge_metadata(w_stream_),0,0,false,false,false,audio_label,player_.codec_summary());
    }
    playing.subs = (w_sub_active_ >= 0 && w_sub_active_ < static_cast<int>(w_subs_.size()))
                       ? w_subs_[static_cast<std::size_t>(w_sub_active_)].label
                       : "Off";
    // The decoder, frame rate and buffer: on the controls, and in the log every 10 seconds.
    if (now - w_stats_at_ > 0.5)
    {
        w_stats_at_ = now;
        playing.stats = player_.stats();
    }
    if (now - w_stats_logged_ > 10.0 && st == Player::State::Playing)
    {
        w_stats_logged_ = now;
        dlog("player: %s", playing.stats.c_str());
        auto context=player_.diagnostic_snapshot();
        context.update(json{{"positionSeconds",pos},{"durationSeconds",dur},{"playing",true},{"phase","playback"},{"nativeHdr",video_hdr},{"evo",prefs_.evo_player}});
        nuvio_diagnostics::record(context);
    }
    playing.controls = now < w_info_until_ || playing.paused || playing.menu;
    if (playing.menu && w_menu_dirty_)
        watch_build_menu();

    if (w_started_ && now - w_last_save_ > kSaveEvery)
    {
        w_last_save_ = now;
        watch_save_progress(false);
    }
    if (player_.too_heavy() && !w_heavy_warned_)
    {
        w_heavy_warned_ = true;
        say("This video is too heavy to decode smoothly. Pick a 1080p stream, or press Square on a stream to transcode it "
            "on the server.",
            10.0);
    }
}

void Session::poll_presentation_timing() {
    if (!watching || launch_open || !video_texture || player_.paused() || player_.buffering() ||
        player_.state()!=Player::State::Playing || player_.picture_serial()!=player_.video_serial()) {
        presentation::reset(); timing_picture_={}; player_.set_video_lead(0);
        return;
    }
    player_.set_video_lead(presentation::poll());
}

void Session::arm_presentation_timing() {
    if (watching && !launch_open && video_texture && !player_.paused() && !player_.buffering() &&
        player_.state()==Player::State::Playing && player_.picture_serial()==player_.video_serial())
        presentation::arm(timing_picture_,output_hz_);
    else presentation::disarm();
}

void Session::upload_video()
{
    if(multiview_active)return;
    const int64_t selected_us = int64_t(now_seconds()*1000000);
    const Player::Picture *pic = player_.present();
    if (!pic)
        return;
    std::string error;
    const bool hdr = prefs_.native_hdr && pic->colors == YuvColors::Hdr10;
    if (hdr && (!pic->hdr10_compatible || pic->format == VideoFormat::Rgba)) {
        fail_video_output("This source has no supported HDR10 base picture. Choose an HDR10 or SDR stream.");
        return;
    }
    if (!video_surface_.upload(*pic, &error, hdr))
    {
        dlog("player: presentation failed: %s", error.c_str());
        watch_stop(false);
        say(error + ". Try another stream.", 10.0);
        return;
    }
    if (!video_texture || pic->w != video_w || pic->h != video_h)
    {
        dlog("player: presenting %dx%d at native resolution via GPU (%s%s); sample aspect %.4f",
             pic->w, pic->h, pic->format == VideoFormat::Rgba ? "RGBA" :
             pic->format == VideoFormat::Nv12 ? "8-bit YUV" : "10-bit YUV",
             pic->format == VideoFormat::Nv12_10 ? (pic->shift ? ", high bits" : ", low bits") : "",
             pic->sar);
        if (!hdr && pic->colors == YuvColors::Hdr10)
            dlog("player: SDR conversion uses luminance tone mapping, BT.1886 display response and gamut fitting; reference peak %.0f nits",
                 pic->hdr_peak_nits);
    }
    if(!video_texture || pic->w!=video_w || pic->h!=video_h) nuvio_diagnostics::record({{"sourceWidth",pic->w},{"sourceHeight",pic->h},{"sourceTenBit",pic->format==VideoFormat::Nv12_10}});
    timing_picture_ = {++timing_picture_id_, selected_us,
        player_.picture_pts()>=0 ? int64_t(player_.picture_pts()*1000000) : INT64_MIN,
        player_.picture_fps(),uint64_t(player_.picture_serial())};
    video_texture = video_surface_.texture();
    video_hdr = hdr;
    video_w = pic->w; video_h = pic->h; video_sar = pic->sar;
    const std::string quality = "Picture " + std::to_string(pic->w) + "x" + std::to_string(pic->h) +
        (pic->format == VideoFormat::Nv12_10 ? " 10-bit" : " 8-bit") +
        (pic->colors == YuvColors::Hdr10 ? (hdr ? " HDR10" : " HDR10 → SDR") : " SDR") +
        " · Render " + std::to_string(output_w_) + "x" + std::to_string(output_h_) +
        (hdr ? (HdrOutput::confirmed() ? " HDR10 10-bit (output confirmed) · " : " HDR10 10-bit (checking output) · ")
             : " SDR 8-bit · ") + (output_hz_ ? std::to_string(output_hz_) + " Hz" : "refresh unverified") +
        " · Audio " + player_.audio_output();
    if(playing.badges.empty() || playing.quality!=quality)
        playing.badges=playback_badges::build(badge_metadata(w_stream_),pic->w,pic->h,true,
            pic->colors==YuvColors::Hdr10,pic->format==VideoFormat::Nv12_10,
            playing.audio=="-"?std::string():playing.audio,player_.codec_summary());
    if (playing.quality != quality) {
        playing.quality = quality;
        dlog("player: quality %s; PS5 configured %ux%u at startup; HDMI resolution requires TV signal readback",
             quality.c_str(), configured_w_, configured_h_);
    }
}

void Session::watch_stop(bool ended)
{
    if(multiview_active){multi_stop();return;}
    resume_play_ = false;
    resuming_ = false;
    if (w_started_ && !ended)
        watch_save_progress(true);
    if(w_started_ && ended && iptv::library_type(w_item_.type))iptv_progress(true);
    if (ended && trailers::records_history(w_trailer_,w_item_.type) && !w_video_id_.empty())
    {
        watched_.insert(w_video_id_);
        save_progress();
    }
    ++w_gen_;
    watch_reset_anime();
    prepared_->cancel(w_stream_, w_season_, w_episode_number_);
    next_preparation_cancel();
    if (w_episode_cancel_) *w_episode_cancel_ = true;
    if (launch_cancel_)
        *launch_cancel_ = true;
    player_.close();
    watching = false;
    launch_open = false;
    t_visible_ = false;
    nuvio_diagnostics::record({{"playing",false},{"phase","ready"}});
    playing = PlayerState();
    subtitle_shown_.clear();
    if(subtitle_texture_) glDeleteTextures(1,&subtitle_texture_);
    subtitle_texture_=0; subtitle_width_=subtitle_height_=0;
    video_surface_.reset();
    presentation::reset(); timing_picture_={};
    video_texture = 0;
    video_hdr = false;
    video_w = video_h = 0;
    video_sar = 1.0;
    // Back on the title's page: what was watched shows there.
    if (d_open_)
        for (std::size_t i = 0; i < detail.episodes.size() && i < d_season_videos_.size(); ++i)
            detail.episodes[i].watched = watched(d_season_videos_[i]->id);
    if(iptv::library_type(w_item_.type))iptv_build();
    changed_ |= kDetail;
    refresh_board();
    refresh_library();
}

void Session::fail_video_output(const std::string& error) {
    dlog("player: output failed: %s",error.c_str());
    dlog("player: HDR evidence %s",HdrOutput::status().c_str());
    nuvio_diagnostics::record(player_.diagnostic_snapshot());
    watch_stop(false);
    nuvio_diagnostics::record({{"phase","output-rejected"},{"outputFailed",true},{"hdrResult",unsigned(HdrOutput::result())}});
    say(error,14.0,true);
}

void Session::watch_save_progress(bool final, bool force_finished)
{
    if(iptv::library_type(w_item_.type)){iptv_progress(force_finished);return;}
    const double pos = player_.position(), dur = player_.duration();
    if (!trailers::records_history(w_trailer_,w_item_.type) || dur <= 0 || w_item_.id.empty())
        return; // live channels and trailers
    const bool finished = w_episode_.record_completion(pos, dur, force_finished);
    if (!final && pos < 1)
        return;

    Progress &p = progress_[w_item_.id];
    p.type = w_item_.type;
    p.name = w_item_.name;
    p.poster = w_item_.poster;
    p.video_id = w_video_id_;
    p.time = finished ? 0 : pos;
    // Finished: Continue Watching goes on to the next episode (and shows its streams); else it is the one being watched.
    p.up_next = finished ? next_episode_id() : std::string();
    p.duration = dur;
    p.updated = now_epoch_ms();
    if (finished)
        watched_.insert(w_video_id_);
    save_progress();

    const double delta = w_last_saved_pos_ >= 0 ? std::max(0.0, std::min(pos - w_last_saved_pos_, kSaveEvery * 2)) : 0;
    w_last_saved_pos_ = pos;

    if (!signed_in())
        return;
    const std::string now = iso8601_now();
    json item;
    const auto it = library_.find(w_item_.id);
    if (it != library_.end())
    {
        item = it->second;
    }
    else
    {
        item = json{{"_id", w_item_.id},
                    {"name", w_item_.name},
                    {"type", w_item_.type},
                    {"poster", w_item_.poster},
                    {"posterShape", "poster"},
                    {"removed", true},
                    {"temp", true},
                    {"_ctime", now},
                    {"state", json::object()},
                    {"behaviorHints", {{"defaultVideoId", nullptr}, {"featuredVideoId", nullptr}, {"hasScheduledVideos", false}}}};
    }
    json &s = item["state"];
    if (!s.is_object())
        s = json::object();
    s["lastWatched"] = now;
    s["lastWatchedMs"] = now_epoch_ms();
    s["nuvioCompleted"] = finished;
    s["trackingFinal"] = final;
    s.erase("nuvioRemoveProgress");
    s["nuvioSeason"] = nullptr;
    s["nuvioEpisode"] = nullptr;
    if (w_item_.type != "movie")
        for (const auto& video : d_meta_.videos) if (video.id == w_video_id_)
        {
            s["nuvioSeason"] = video.season;
            s["nuvioEpisode"] = video.episode;
            break;
        }
    s["timeOffset"] = static_cast<std::int64_t>(finished ? 0 : pos * 1000);
    s["duration"] = static_cast<std::int64_t>(dur * 1000);
    s["video_id"] = w_video_id_;
    s["timeWatched"] = static_cast<std::int64_t>(jnum(s, "timeWatched") + delta * 1000);
    s["overallTimeWatched"] = static_cast<std::int64_t>(jnum(s, "overallTimeWatched") + delta * 1000);
    if (!s.contains("timesWatched"))
        s["timesWatched"] = 0;
    if (!s.contains("flaggedWatched"))
        s["flaggedWatched"] = 0;
    if (!s.contains("noNotif"))
        s["noNotif"] = false;
    if (!s.contains("watched"))
        s["watched"] = nullptr;
    if (finished && final)
    {
        if (jstr(tracking_status_, "progressPreferred", "nuvio") == "nuvio")
            sync_watched_mark(w_video_id_, w_item_.name, true, w_item_.id, w_season_, w_episode_number_);
        s["timesWatched"] = static_cast<int>(jnum(s, "timesWatched")) + 1;
        if (w_item_.type == "movie")
            s["flaggedWatched"] = 1;
    }
    item["_mtime"] = now;
    library_[w_item_.id] = item;
    library_loaded_ = true;

    const std::string key = prefs_.auth_key;
    const json changes = json::array({item});
    bg<ApiResult>([key, changes]() { return api_library_put(key, changes); },
                  [](ApiResult &r) {
                      if (!r.ok)
                          dlog("library put: %s", r.error.c_str());
                  });
}

// ---------------------------------------------------------------------------
// The buttons

void Session::media_transport(int action, int direction)
{
    if(multiview_active) {
        if(action)for(auto& p:multi_players_)if(p)p->set_paused(action==1?!p->paused():action==3);
        return;
    }
    if (!watching || launch_open || !w_started_ || typing() || dropdown_open || sign_in_open)
        return;
    if (action)
    {
        player_.set_paused(action == 1 ? !player_.paused() : action == 3);
        if (player_.paused())
            watch_save_progress(false);
        watch_show_info();
    }
    if (direction && w_item_.type != "tv")
    {
        player_.seek(player_.position() + direction * prefs_.seek_seconds);
        watch_show_info();
    }
}

void Session::watch_input(const hui::InputFrame &in)
{
    using hui::Action;
    using hui::Direction;
    if (!watching)
        return;
    if(multiview_active){multi_input(in);return;}
    if (launch_open)
    {
        // The stream is still starting: only a way out.
        if (in.is_pressed(Action::back))
            watch_stop(false);
        return;
    }
    const bool any = in.pressed != 0 || in.nav != Direction::none;
    if (any)
        watch_show_info();

    if (playing.menu)
    {
        const std::size_t audio_count = playing.audio_tracks.size(), sub_count = playing.subtitle_tracks.size();
        const int columns=playing.chapters.empty()?2:3;
        if (in.nav == Direction::left)
            playing.menu_column = std::max(0,playing.menu_column-1);
        else if (in.nav == Direction::right)
            playing.menu_column = std::min(columns-1,playing.menu_column+1);
        else if (in.nav == Direction::up)
            playing.menu_row = std::max(0, playing.menu_row - 1);
        else if (in.nav == Direction::down)
            playing.menu_row += 1;
        const int count = static_cast<int>(playing.menu_column == 0 ? audio_count : playing.menu_column==1?sub_count:playing.chapters.size());
        playing.menu_row = std::clamp(playing.menu_row, 0, std::max(0, count - 1));
        if (in.is_pressed(Action::confirm))
        {
            if (playing.menu_column == 0)
            {
                const auto tracks = player_.audio_tracks();
                if (playing.menu_row < static_cast<int>(tracks.size()))
                    player_.select_audio(tracks[static_cast<std::size_t>(playing.menu_row)].stream);
            }
            else if(playing.menu_column==1)
            {
                watch_select_sub(playing.menu_row - 1);
            }
            else if(playing.menu_row<int(playing.chapters.size())) {
                player_.seek(playing.chapters[size_t(playing.menu_row)].start);
                playing.menu=false;watch_show_info();
            }
            w_menu_dirty_ = true;
        }
        const int sync_step=in.is_pressed(Action::jump_next)?50:in.is_pressed(Action::jump_prev)?-50:0;
        bool adjusted=false;
        if(sync_step && playing.menu_column<2) {
            if(playing.menu_column==0) {
                prefs_.audio_delay_ms=std::clamp(prefs_.audio_delay_ms+sync_step,-2000,2000);
                player_.set_audio_delay(prefs_.audio_delay_ms/1000.0);
            } else {
                prefs_.subtitle_delay_ms=std::clamp(int(std::lround(w_sub_delay_*1000))+sync_step,-10000,10000);
                w_sub_delay_=prefs_.subtitle_delay_ms/1000.0;
            }
            adjusted=true;
        }
        if(playing.menu_column==0) {
            const int step=in.is_pressed(Action::page_next)?25:in.is_pressed(Action::page_prev)?-25:0;
            if(step) {
                const int gain=std::clamp(prefs_.audio_gain_percent+step,50,300);
                if(player_.set_audio_gain(gain)) {prefs_.audio_gain_percent=gain;adjusted=true;}
                else say("Audio boost needs PCM output. Select PCM in Settings before reopening this stream.",7);
            }
        }
        if(in.is_pressed(Action::north) && playing.menu_column<2) {
            if(playing.menu_column==0) {prefs_.audio_delay_ms=0;player_.set_audio_delay(0);
                if(player_.set_audio_gain(100))prefs_.audio_gain_percent=100;
            } else {prefs_.subtitle_delay_ms=0;w_sub_delay_=0;}
            adjusted=true;
        }
        if(adjusted)save_prefs();
        if (in.is_pressed(Action::back) || in.is_pressed(Action::menu) || in.is_pressed(Action::touch))
        {
            playing.menu = false;
            watch_show_info();
        }
        if (playing.menu)
            watch_build_menu();
        return;
    }

    if (!w_trailer_ && w_item_.type != "tv" && !iptv::library_type(w_item_.type) && watch_episode_input(in)) return;

    if (in.is_pressed(Action::confirm) || in.is_pressed(Action::touch))
    {
        player_.set_paused(!player_.paused());
        if (player_.paused()) watch_save_progress(false);
    }
    else if (in.nav == Direction::up || in.is_pressed(Action::menu))
    {
        playing.menu = true;
        watch_build_menu();
        const auto tracks = player_.audio_tracks();
        playing.menu_column = tracks.size() > 1 ? 0 : 1;
        playing.menu_row = playing.menu_column == 0 ? playing.audio_active : playing.subtitle_active;
    }
    else if(w_item_.type == "tv" && (in.is_pressed(Action::l3) || in.nav == Direction::left ||
            in.nav == Direction::right || in.is_pressed(Action::page_prev) || in.is_pressed(Action::page_next)))
    {
        say("Seeking is unavailable for live channels",4);
    }
    else if (in.is_pressed(Action::l3))
    {
        player_.seek(0);
    }
    else if (in.nav == Direction::left)
    {
        player_.seek(player_.position() - prefs_.seek_seconds);
    }
    else if (in.nav == Direction::right)
    {
        player_.seek(player_.position() + prefs_.seek_seconds);
    }
    else if (in.is_pressed(Action::page_prev))
    {
        player_.seek(player_.position() - prefs_.shoulder_seek_seconds);
    }
    else if (in.is_pressed(Action::page_next))
    {
        player_.seek(player_.position() + prefs_.shoulder_seek_seconds);
    }
    else if (in.is_pressed(Action::west))
    {
        // Cycle: off, each subtitle, off.
        const int n = static_cast<int>(w_subs_.size());
        if (n == 0)
        {
            say("No subtitles for this video");
        }
        else
        {
            int next = w_sub_active_ + 1;
            if (next >= n)
                next = -1;
            watch_select_sub(next);
        }
    }
    else if (in.is_pressed(Action::north))
    {
        const auto tracks = player_.audio_tracks();
        if (tracks.size() < 2)
        {
            say(tracks.empty() ? "No audio" : "Only one audio track");
        }
        else
        {
            std::size_t cur = 0;
            for (std::size_t i = 0; i < tracks.size(); ++i)
                if (tracks[i].stream == player_.audio_stream())
                    cur = i;
            const auto &t = tracks[(cur + 1) % tracks.size()];
            player_.select_audio(t.stream);
            say("Audio: " + t.label);
        }
    }
    else if (in.is_pressed(Action::jump_prev))
    {
        w_sub_delay_ -= 0.25;
        say("Subtitle delay " + std::to_string(static_cast<int>(std::lround(w_sub_delay_ * 1000))) + " ms");
    }
    else if (in.is_pressed(Action::jump_next))
    {
        w_sub_delay_ += 0.25;
        say("Subtitle delay " + std::to_string(static_cast<int>(std::lround(w_sub_delay_ * 1000))) + " ms");
    }
    else if (in.is_pressed(Action::back))
    {
        watch_stop(false);
    }
}

void Session::watch_refresh_tracks()
{
    const bool reopened=w_menu_built_;
    std::optional<WatchSub> selected;
    if(w_sub_active_>=0 && w_sub_active_<int(w_subs_.size())) {
        const auto& old=w_subs_[size_t(w_sub_active_)];selected.emplace();
        selected->embedded=old.embedded;selected->label=old.label;selected->lang=old.lang;selected->url=old.url;
        selected->forced=old.forced;selected->hearing_impaired=old.hearing_impaired;
    }
    // Keep downloaded addon cues and explicit Off, but discard old in-file IDs.
    w_subs_.erase(std::remove_if(w_subs_.begin(),w_subs_.end(),[](const auto& s){return s.embedded>=0;}),w_subs_.end());
    w_sub_active_=-1;
    w_menu_built_ = true;
    w_player_revision_=player_.revision();w_chapters_at_=0;
    std::vector<WatchSub> embedded;
    for (const auto &t : player_.subtitle_tracks())
    {
        WatchSub s;
        s.label = t.label + " (in file)";
        s.lang = t.lang;
        s.embedded = t.stream;
        s.is_default=t.is_default; s.forced=t.forced; s.hearing_impaired=t.hearing_impaired;
        embedded.push_back(s);
    }
    w_subs_.insert(w_subs_.begin(), embedded.begin(), embedded.end());
    if(selected) {
        if(selected->embedded>=0)w_sub_active_=restored_subtitle(*selected,w_subs_);
        else for(size_t i=0;i<w_subs_.size();++i)
            if(w_subs_[i].embedded<0 && w_subs_[i].url==selected->url){w_sub_active_=int(i);break;}
    }
    // A vanished selected track becomes Off, rather than a different language.
    if(!reopened)watch_auto_subtitles();
    w_menu_dirty_ = true;

}

void Session::watch_build_menu()
{
    w_menu_dirty_ = false;
    playing.audio_tracks.clear();
    playing.audio_active = 0;
    const auto tracks = player_.audio_tracks();
    for (std::size_t i = 0; i < tracks.size(); ++i)
    {
        playing.audio_tracks.push_back(tracks[i].label);
        if (tracks[i].stream == player_.audio_stream())
            playing.audio_active = static_cast<int>(i);
    }
    playing.subtitle_tracks.clear();
    playing.subtitle_tracks.push_back("Off");
    for (const WatchSub &s : w_subs_)
    {
        std::string label = s.label;
        if (s.loading)
            label += "  (loading)";
        if (s.failed)
            label += "  (failed)";
        playing.subtitle_tracks.push_back(label);
    }
    playing.subtitle_active = w_sub_active_ + 1;
    playing.chapter_labels.clear();
    for(const auto& chapter:playing.chapters) {
        const int seconds=int(std::min(chapter.start,359999.0));char at[32];
        std::snprintf(at,sizeof(at),"%d:%02d:%02d",seconds/3600,seconds/60%60,seconds%60);
        playing.chapter_labels.push_back(std::string(at)+"  "+chapter.title);
    }
    playing.menu_column=std::clamp(playing.menu_column,0,playing.chapters.empty()?1:2);
    const auto count=playing.menu_column==0?playing.audio_tracks.size():playing.menu_column==1?playing.subtitle_tracks.size():playing.chapter_labels.size();
    playing.menu_row=std::clamp(playing.menu_row,0,std::max(0,int(count)-1));
    char delay[48];
    std::snprintf(delay, sizeof(delay), "%+.2f s", w_sub_delay_);
    playing.subtitle_delay = delay;
    playing.audio_adjustments=std::to_string(prefs_.audio_gain_percent)+"% · "+std::to_string(prefs_.audio_delay_ms)+" ms";
}

// ---------------------------------------------------------------------------
// Subtitles

void Session::watch_select_sub(int index)
{
    // Including Off: late addon answers must never undo a manual decision.
    w_auto_sub_done_=true;
    if (index < 0 || index >= static_cast<int>(w_subs_.size()))
    {
        w_sub_active_ = -1;
        say("Subtitles off");
        w_menu_dirty_ = true;
        return;
    }
    w_sub_active_ = index;
    WatchSub &ws = w_subs_[static_cast<std::size_t>(index)];
    say("Subtitles: " + ws.label);
    if (ws.embedded < 0 && ws.cues.empty() && !ws.loading)
    {
        ws.loading = true;
        ws.failed = false;
        const std::string url = ws.url, lang = ws.lang;
        const int gen = w_gen_;
        struct Res
        {
            std::vector<Cue> cues;
            std::string utf8;
            std::string error;
        };
        bg<Res>(
            [url, lang]() {
                Res r;
                const HttpResponse h = http_get(url, 30);
                if (!h.ok())
                {
                    r.error = h.describe();
                    return r;
                }
                if(h.body.size()>8u<<20) { r.error="Subtitle file is too large"; return r; }
                r.utf8=to_utf8(h.body,lang);
                r.cues = parse_subtitle_file(r.utf8);
                if (r.cues.empty())
                    r.error = "That subtitle file is empty or unreadable";
                return r;
            },
            [this, gen, url](Res &r) {
                if (gen != w_gen_)
                    return;
                for (WatchSub &s : w_subs_)
                {
                    if (s.url != url)
                        continue;
                    s.loading = false;
                    s.cues = std::move(r.cues);
                    s.rich_text=r.utf8;
                    s.rich_id=-2-int(&s-w_subs_.data());
                    if(!player_.subtitle_renderer().external(s.rich_id,r.utf8,s.lang)) s.rich_id=-1;
                    s.failed = !r.error.empty();
                }
                if (!r.error.empty())
                    say("Could not download the subtitles: " + r.error, 5.0, true);
                w_menu_dirty_ = true;
            });
    }
    w_menu_dirty_ = true;
}

void Session::watch_load_addon_subtitles()
{
    const std::string type = w_item_.type, vid = w_video_id_;
    std::vector<std::pair<std::string, std::string>> sources;
    for (const auto &a : addons_)
        if (a->supports("subtitles", type, vid))
            sources.push_back({a->name, a->resource_url("subtitles", type, vid)});
    const int gen = w_gen_;
    for (const auto &src : sources)
    {
        const std::string name = src.first, url = src.second;
        bg<std::vector<SubtitleTrack>>(
            [name, url]() {
                json j;
                std::string err;
                if (!fetch_json(url, j, err, 25))
                    return std::vector<SubtitleTrack>();
                return parse_subtitles(j, name);
            },
            [this, gen](std::vector<SubtitleTrack> &tracks) {
                if (gen != w_gen_)
                    return;
                // Preferred languages first, then the rest, at most 80.
                const auto prefs = pref_langs(prefs_.subtitle_langs);
                const auto rank = [&](const std::string &lang) {
                    const std::string l = language_to_iso639_2(lang);
                    for (std::size_t i = 0; i < prefs.size(); ++i)
                        if (prefs[i] == l)
                            return static_cast<int>(i);
                    return static_cast<int>(prefs.size());
                };
                std::stable_sort(tracks.begin(), tracks.end(),
                                 [&](const SubtitleTrack &a, const SubtitleTrack &b) { return rank(a.lang) < rank(b.lang); });
                std::map<std::string, int> per_lang;
                for (const auto &t : tracks)
                {
                    if (w_subs_.size() >= 80)
                        break;
                    int &n = per_lang[t.lang];
                    ++n;
                    WatchSub s;
                    s.label = t.label + " " + std::to_string(n) + "  \xC2\xB7  " + t.addon;
                    s.lang = language_to_iso639_2(t.lang);
                    s.url = t.url;
                    w_subs_.push_back(s);
                }
                watch_auto_subtitles();
                w_menu_dirty_ = true;
            });
    }
}

void Session::watch_auto_subtitles()
{
    if(!prefs_.auto_subtitles) return;
    std::vector<SubtitleCandidate> tracks;
    for(const auto& s:w_subs_)
        tracks.push_back({s.embedded>=0,s.lang,s.is_default,s.forced,s.hearing_impaired});
    const int pick=default_subtitle(tracks,pref_langs(prefs_.subtitle_langs),w_menu_built_,
        w_auto_sub_done_ || w_sub_active_>=0);
    if(pick>=0) watch_select_sub(pick);
}

// ---------------------------------------------------------------------------
// The next episode

void Session::watch_next_episode()
{
    const auto* found = episode_playback::next_video(d_meta_.videos, w_video_id_, now_epoch_ms());
    if (!found) return;
    const ::Video next = *found;
    const ::Stream previous = w_stream_;
    const bool cached = w_next_ && w_next_->id == next.id && w_next_requested_ &&
        w_next_answered_ >= w_next_pending_ && now_seconds() - w_next_fetched_ < 90;
    const auto candidates = w_next_streams_;
    if (cached && w_next_pick_ >= 0 && w_next_pick_ < static_cast<int>(candidates.size()))
        prepared_->promote(candidates[w_next_pick_], next.season, next.episode);
    if (watching)
    {
        watch_save_progress(true, true);
        watch_stop(true);
    }

    for (std::size_t i = 0; i < d_seasons_.size(); ++i)
        if (d_seasons_[i] == next.season && static_cast<int>(i) != d_season_idx_)
            detail_set_season(static_cast<int>(i));
    for (std::size_t i = 0; i < d_season_videos_.size(); ++i)
        if (d_season_videos_[i]->id == next.id)
        {
            detail.episode_hint = static_cast<int>(i);
            ++detail.hint_serial;
        }

    autoplay_pending_ = true;
    autoplay_previous_ = previous;
    std::string heading = "S" + std::to_string(next.season) + " E" + std::to_string(next.episode);
    if (!next.title.empty())
        heading += "  \xC2\xB7  " + next.title;
    say("Next: " + heading);
    detail_load_streams(next.id, heading, cached ? &candidates : nullptr);
}

} // namespace sx
