#include "../ui/i18n.hpp"
// Torrents & Magnet streaming hub: local .torrent file scanner (USB & PS5 /data),
// magnet link and .torrent URL input, multi-file picker, and Phone/PC Web Drop QR server.

#include "session.hpp"
#include "http.h"
#include "qrcodegen.h"
#include "torrent/engine.h"
#include "torrent/torrent_stream.h"

#include <algorithm>
#include <cmath>
#include <charconv>
#include <limits>
#include <cstdio>

namespace sx
{

namespace
{

std::string format_bytes(int64_t bytes)
{
    if (bytes <= 0)
        return "0 B";
    char buf[48];
    if (bytes >= (1ll << 30))
        std::snprintf(buf, sizeof(buf), "%.2f GB", double(bytes) / double(1ll << 30));
    else if (bytes >= (1ll << 20))
        std::snprintf(buf, sizeof(buf), "%.1f MB", double(bytes) / double(1ll << 20));
    else if (bytes >= (1ll << 10))
        std::snprintf(buf, sizeof(buf), "%.1f KB", double(bytes) / double(1ll << 10));
    else
        std::snprintf(buf, sizeof(buf), "%lld B", static_cast<long long>(bytes));
    return buf;
}

// Saved progress belongs to one concrete file, never an arbitrary file in the swarm.
double valid_resume_position(const json &entry)
{
    const double pos = jnum(entry, "position", 0), dur = jnum(entry, "duration", 0);
    return std::isfinite(pos) && std::isfinite(dur) && pos > 5 &&
           pos < std::numeric_limits<int>::max() - 1.0 && dur >= 0 &&
           (dur == 0 || pos < dur * 0.95) ? pos : 0;
}
struct TorrentResume { int file = -1; double position = 0, updated = -1; std::string name; };
TorrentResume latest_resume(const json &state, const std::string &hash)
{
    TorrentResume best;
    const auto &res = jobj(state, "resume");
    if (!res.is_object() || hash.empty()) return best;
    const auto prefix = "torrent:" + hash + ":";
    for (auto it = res.begin(); it != res.end(); ++it) {
        if (!starts_with(it.key(), prefix)) continue;
        const auto suffix = it.key().substr(prefix.size());
        int file = -1;
        const auto parsed = std::from_chars(suffix.data(), suffix.data()+suffix.size(), file);
        // Match the metadata parser's 10,000-file bound before formatting file+1.
        if (parsed.ec != std::errc{} || parsed.ptr != suffix.data()+suffix.size() || file < 0 || file >= 10000) continue;
        const double pos = valid_resume_position(it.value());
        double updated = jnum(it.value(), "updated", 0);
        if (!std::isfinite(updated) || updated < 0) updated = 0;
        if (pos > 0 && (best.file < 0 || updated > best.updated)) best = {file, pos, updated, jstr(it.value(), "fileName")};
    }
    return best;
}

std::string format_resume_time(double seconds)
{
    if (seconds <= 0)
        return "";
    const int total = static_cast<int>(std::lround(seconds));
    const int h = total / 3600;
    const int m = (total % 3600) / 60;
    const int s = total % 60;
    char buf[48];
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
    else
        std::snprintf(buf, sizeof(buf), "%d:%02d", m, s);
    return buf;
}

bool is_video_filename(const std::string &path)
{
    static const char *const exts[] = {".mkv", ".mp4", ".avi", ".m4v", ".mov", ".ts",
                                       ".m2ts", ".webm", ".wmv", ".mpg", ".mpeg", ".flv"};
    const std::string p = lower(path);
    for (const char *ext : exts)
        if (ends_with(p, ext))
            return true;
    return false;
}

std::string first_letter(const std::string &name)
{
    std::size_t len = name.empty() ? 0 : 1;
    while (len < name.size() && (static_cast<unsigned char>(name[len]) & 0xC0) == 0x80)
        ++len;
    return name.substr(0, len);
}

std::string short_filename(const std::string &path)
{
    const std::size_t sl = path.find_last_of("/\\");
    return sl == std::string::npos ? path : path.substr(sl + 1);
}

} // namespace

void Session::torrent_reset()
{
    bt::Engine::get().stop_web_drop();
    if(torrent_drop_open_){sign_in_open=false;torrent_drop_open_=false;}
    if (torrent_cancel_)
        *torrent_cancel_ = true;
    torrent_cancel_.reset();
    bt::Engine::get().shutdown();
    torrent_state_ = json::object();
    torrent_local_files_.clear();
    torrent_entries_.clear();
    torrent_query_.clear();
    torrent_view_ = 0;
    torrents_content.items.clear();
    changed_ |= kTorrentPage;
}

void Session::torrent_load()
{
    torrent_reset();
    load_json(profile_dir_ + "/torrents.json", torrent_state_);
    if (!torrent_state_.is_object())
        torrent_state_ = json::object();
    if (!jobj(torrent_state_, "recent").is_array())
        torrent_state_["recent"] = json::array();
    if (!jobj(torrent_state_, "resume").is_object())
        torrent_state_["resume"] = json::object();
    torrents_visible = prefs_.builtin_torrents;
    if(prefs_.builtin_torrents)torrent_local_files_ = bt::Engine::scan_local_torrents(data_dir_);
    torrent_build();
}

void Session::torrent_save()
{
    if (!profile_ready_ || profile_dir_.empty())
        return;
    save_json(profile_dir_ + "/torrents.json", torrent_state_);
}

double Session::torrent_resume(const std::string &id) const
{
    const json &res = jobj(torrent_state_, "resume");
    if (!res.is_object() || !res.contains(id))
        return 0;
    return valid_resume_position(res[id]);
}

void Session::torrent_progress(bool finished)
{
    if (w_item_.type != "torrent" || w_item_.id.empty() || ends_with(w_item_.id, ":-1"))
        return;
    const double pos = player_.position(), dur = player_.duration();
    if (!std::isfinite(pos) || !std::isfinite(dur) || pos < 0 || dur < 0)
        return;
    const bool done = finished || (dur > 0 && pos >= dur * 0.95);
    if (!torrent_state_.is_object())
        torrent_state_ = json::object();
    if (!jobj(torrent_state_, "resume").is_object())
        torrent_state_["resume"] = json::object();
    if (done)
    {
        torrent_state_["resume"].erase(w_item_.id);
    }
    else if (pos >= 5.0)
    {
        torrent_state_["resume"][w_item_.id] = {
            {"fileName", w_item_.name},
            {"position", pos},
            {"duration", dur},
            {"updated", now_epoch_ms()},
        };
    }
    torrent_save();
}

void Session::torrent_rescan()
{
    if(!prefs_.builtin_torrents)return;
    torrent_local_files_ = bt::Engine::scan_local_torrents(data_dir_);
    torrent_build();
    say(sx::i18n::tr("Scanned USB & PS5 storage: ") + std::to_string(torrent_local_files_.size()) + sx::i18n::tr(" .torrent file(s) found"), 4.5);
}

void Session::torrent_cycle()
{
    torrent_view_ = (torrent_view_ + 1) % 3;
    torrent_build();
}

void Session::torrent_search()
{
    ask_text(
        [this](const std::string &text) {
            const std::string v = trim(text);
            if (!v.empty() && torrent_try_search_input(v))
                return;
            torrent_query_ = v;
            torrent_build();
        },
        sx::i18n::tr("Search .torrent files, or paste a magnet:? link / .torrent URL (empty clears filter)"));
}

void Session::torrent_build()
{
    torrents_content.items.clear();
    torrent_entries_.clear();

    const bool drop_active = !bt::Engine::get().web_drop_url().empty();
    const std::string view_label = torrent_view_ == 1   ? sx::i18n::tr("Local .torrent files")
                                   : torrent_view_ == 2 ? sx::i18n::tr("Recent streams")
                                                        : sx::i18n::tr("All sources");

    // Tile 0: Hub & Filter Management
    {
        sx::Addon hub;
        hub.name = sx::i18n::tr("Torrents & Magnet Hub");
        hub.initial = "BT";
        hub.colour = 0x7b5bf5;
        hub.local = true;
        hub.version = sx::i18n::tr("Cache: ") + std::to_string(prefs_.torrent_cache_gb) + " GB · " + view_label;
        if (!torrent_query_.empty())
            hub.version += sx::i18n::tr(" · Filter: ") + torrent_query_;
        hub.types = sx::i18n::tr("Native torrent streaming");
        hub.description = sx::i18n::tr("X: open Torrents menu · Triangle: search / paste magnet · L1: rescan USB & /data · R1: filter view");
        torrents_content.items.push_back(std::move(hub));
    }

    // Tile 1: Direct Magnet / .torrent URL / Path Input
    {
        sx::Addon add_mag;
        add_mag.name = sx::i18n::tr("Enter Magnet or .torrent URL");
        add_mag.initial = "M";
        add_mag.colour = 0x0ea5a4;
        add_mag.local = true;
        add_mag.version = sx::i18n::tr("Keyboard input");
        add_mag.types = sx::i18n::tr("Magnet InfoHash .torrent-URL");
        add_mag.description = sx::i18n::tr("Press X to type or paste a magnet:?xt=urn:btih:... link, 40-char hash, or http(s):// .torrent URL.");
        torrents_content.items.push_back(std::move(add_mag));
    }

    // Tile 2: Phone / PC Web Drop QR Code
    {
        sx::Addon drop;
        drop.name = sx::i18n::tr("Phone / PC Web Drop (QR Code)");
        drop.initial = "QR";
        drop.colour = 0x3b82f6;
        drop.local = true;
        drop.version = drop_active ? sx::i18n::tr("Pairing active") : sx::i18n::tr("Local Wi-Fi Drop");
        drop.types = sx::i18n::tr("Upload-.torrent Paste-Magnet");
        drop.description = sx::i18n::tr("Press X to show a QR code. Scan with your phone or open on your PC to upload a .torrent file or paste a magnet link.");
        torrents_content.items.push_back(std::move(drop));
    }

    const std::string q = lower(trim(torrent_query_));

    // Recent / Saved torrents (shown in All or Recent views)
    if (torrent_view_ == 0 || torrent_view_ == 2)
    {
        const json &rec = jobj(torrent_state_, "recent");
        if (rec.is_array())
        {
            for (const auto &entry : rec)
            {
                const std::string id = jstr(entry, "id");
                const std::string name = jstr(entry, "name", id);
                const std::string uri = jstr(entry, "uri");
                if (id.empty() || uri.empty())
                    continue;
                if (!q.empty() && lower(name).find(q) == std::string::npos && lower(id).find(q) == std::string::npos)
                    continue;
                TorrentEntry te;
                te.is_local_file = starts_with(uri, "/");
                te.id = id;
                te.name = name;
                te.path_or_uri = uri;
                te.subtitle = jstr(entry, "subtitle", te.is_local_file ? sx::i18n::tr("Saved .torrent") : sx::i18n::tr("Recent Magnet"));
                const json &tr = jobj(entry, "trackers");
                if (tr.is_array())
                    for (const auto &v : tr)
                        if (v.is_string())
                            te.trackers.push_back(v.get<std::string>());
                const json &ws = jobj(entry, "webseeds");
                if (ws.is_array())
                    for (const auto &v : ws)
                        if (v.is_string())
                            te.webseeds.push_back(v.get<std::string>());

                const auto saved = latest_resume(torrent_state_, id);
                if (saved.file >= 0) te.subtitle += sx::i18n::tr(" · Resume ") + format_resume_time(saved.position);

                sx::Addon row;
                row.name = te.name;
                row.version = te.subtitle;
                row.types = te.is_local_file ? sx::i18n::tr("Recent .torrent") : sx::i18n::tr("Recent Magnet");
                row.initial = first_letter(te.name.empty() ? "T" : te.name);
                row.colour = 0x6d6ae0;
                row.local = true;
                row.description = sx::i18n::tr("X: play/resume · Square: resume, restart, choose file or remove");
                torrents_content.items.push_back(std::move(row));
                torrent_entries_.push_back(std::move(te));
            }
        }
    }

    // Discovered local .torrent files on USB (/mnt/usb0..7) and PS5 storage (/data/torrents, /data)
    if (torrent_view_ == 0 || torrent_view_ == 1)
    {
        for (const auto &lf : torrent_local_files_)
        {
            if (!q.empty() && lower(lf.filename).find(q) == std::string::npos &&
                lower(lf.path).find(q) == std::string::npos)
                continue;
            // Avoid duplicating a file already listed in Recent above
            bool dup = false;
            for (const auto &existing : torrent_entries_)
                if (existing.path_or_uri == lf.path)
                    dup = true;
            if (dup)
                continue;

            TorrentEntry te;
            te.is_local_file = true;
            te.name = lf.filename;
            te.path_or_uri = lf.path;
            te.subtitle = lf.source_label + " · " + format_bytes(lf.file_size);

            sx::Addon row;
            row.name = lf.filename;
            row.version = te.subtitle;
            row.types = sx::i18n::tr("Local .torrent");
            row.initial = "T";
            row.colour = 0xe0568a;
            row.local = true;
            row.description = lf.path + sx::i18n::tr("\nX: open & stream · Square: choose file in torrent");
            torrents_content.items.push_back(std::move(row));
            torrent_entries_.push_back(std::move(te));
        }
    }

    changed_ |= kTorrentPage;
}

void Session::torrent_show_web_drop_qr()
{
    if(!prefs_.builtin_torrents){say(sx::i18n::tr("Enable torrent streaming first"),5,true);return;}
    std::string url = bt::Engine::get().start_web_drop(8088);
    if (url.empty())
    {
        say(sx::i18n::tr("Could not start the local Web Drop listener."), 6.0, true);
        return;
    }
    sign_in = SignInState{};
    sign_in.link_only = true;
    sign_in.title = sx::i18n::tr("Phone / PC Torrent Web Drop");
    sign_in.link = url;
    sign_in.status = sx::i18n::tr("Pair with this QR code. Expires after 10 minutes; confirm received torrents on this console.");
    sign_in.cancel_label = sx::i18n::tr("Close");
    std::uint8_t temp[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
    if (qrcodegen_encodeText(url.c_str(), temp, qr, qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, 20,
                             qrcodegen_Mask_AUTO, true))
    {
        sign_in.qr_size = qrcodegen_getSize(qr);
        sign_in.qr_modules.resize(sign_in.qr_size * sign_in.qr_size);
        for (int y = 0; y < sign_in.qr_size; ++y)
            for (int x = 0; x < sign_in.qr_size; ++x)
                sign_in.qr_modules[y * sign_in.qr_size + x] = qrcodegen_getModule(qr, x, y);
    }
    sign_in_open = true;
    torrent_drop_open_ = true;
    torrent_build();
}

void Session::torrent_menu(bool settings_only)
{
    if(!prefs_.builtin_torrents && !settings_only){say(sx::i18n::tr("Enable torrent streaming first"),5,true);return;}
    std::vector<std::string> options = {
        sx::i18n::tr("Enter Magnet link, Info-Hash or .torrent URL"),
        sx::i18n::tr("Scan USB & PS5 storage for .torrent files"),
        sx::i18n::tr("Search / filter .torrent files"),
        sx::i18n::tr("Clear search filter"),
        sx::i18n::tr("Phone / PC Web Drop (Show QR Code)"),
        sx::i18n::tr("Rolling cache size (") + std::to_string(prefs_.torrent_cache_gb) + " GB)",
        sx::i18n::tr("View: All sources"),
        sx::i18n::tr("View: Local .torrent files only"),
        sx::i18n::tr("View: Recent streams only"),
        sx::i18n::tr("Clear recent torrent history"),
        sx::i18n::tr("Cached storage: ") + format_bytes(bt::Engine::get().cache_storage_bytes()) + sx::i18n::tr(" (limit ") + std::to_string(prefs_.torrent_cache_gb) + " GB)",
        sx::i18n::tr("Turn off torrents and clear buffered data"),
        bt::Engine::get().web_drop_url().empty() ? sx::i18n::tr("Web Drop: inactive") : sx::i18n::tr("Web Drop: pairing active (close pairing)"),
    };
    if(settings_only)options={options[5],options[10],options[11],options[12]};
    open_dropdown(settings_only?sx::i18n::tr("Torrent settings"):sx::i18n::tr("Torrents & Magnet Streaming"), options, 0, [this, settings_only, generation=profile_generation_](int pick) {
        if(settings_only){if(pick<0 || pick>=4)return;static constexpr int actions[]={5,10,11,12};pick=actions[pick];}
        if(generation!=profile_generation_ || !profile_ready_)return;
        switch (pick)
        {
        case 0:
            ask_text([this](const std::string &v) {
                if (!trim(v).empty())
                    torrent_open_input(trim(v), false);
            }, sx::i18n::tr("Enter magnet:? link, 40-char info-hash, or .torrent URL / path"));
            break;
        case 1:
            torrent_rescan();
            break;
        case 2:
            torrent_search();
            break;
        case 3:
            torrent_query_.clear();
            torrent_build();
            break;
        case 4:
            torrent_show_web_drop_qr();
            break;
        case 5:
        {
            static constexpr int kSizes[] = {1, 2, 4, 6};
            int active = 0;
            for (int i = 0; i < 4; ++i)
                if (prefs_.torrent_cache_gb == kSizes[i])
                    active = i;
            open_dropdown(sx::i18n::tr("Torrent rolling cache size"),
                          {"1 GB", "2 GB", "4 GB", "6 GB (default)"}, active,
                          [this](int idx) {
                              static constexpr int kVals[] = {1, 2, 4, 6};
                              if (idx < 0 || idx >= 4)
                                  return;
                              torrent_set_cache(kVals[idx]);
                              say(sx::i18n::tr("Torrent rolling cache set to ") + std::to_string(prefs_.torrent_cache_gb) + " GB");
                          });
            break;
        }
        case 6:
        case 7:
        case 8:
            torrent_view_ = pick - 6;
            torrent_build();
            break;
        case 10:
            say(sx::i18n::tr("Buffered torrent pieces use this storage. .torrent files and saved progress are separate. The limit controls rolling storage, not total video size."),12);break;
        case 11:
            open_dropdown(sx::i18n::tr("Clear buffered torrent data?"),{sx::i18n::tr("Cancel"),sx::i18n::tr("Turn off torrents and clear buffer")},0,
                [this,generation=profile_generation_](int choice){
                    if(choice!=1 || generation!=profile_generation_ || !profile_ready_)return;
                    if(watching || resuming_){say(sx::i18n::tr("Stop playback before clearing buffered data."),7,true);return;}
                    if(torrent_cancel_)*torrent_cancel_=true;
                    std::string error;const bool ok=bt::Engine::get().clear_cache(&error);
                    prefs_.builtin_torrents=bt::Engine::get().enabled();torrents_visible=prefs_.builtin_torrents;
                    if(!prefs_.builtin_torrents){torrent_reset();torrent_drop_open_=false;sign_in_open=false;}
                    save_prefs();refresh_settings();
                    say(ok?sx::i18n::tr("Buffered data cleared; torrent streaming is off. Saved progress and .torrent files were kept."):error,9,!ok);
                });break;
        case 12:
            if(!bt::Engine::get().web_drop_url().empty()){bt::Engine::get().stop_web_drop();torrent_drop_open_=false;say(sx::i18n::tr("Web Drop pairing closed."),5);}
            else torrent_show_web_drop_qr();
            break;
        case 9:
            torrent_state_["recent"] = json::array();
            torrent_state_["resume"] = json::object();
            torrent_save();
            torrent_build();
            say(sx::i18n::tr("Cleared recent torrent history"));
            break;
        default:
            break;
        }
    });
    dropdown.message = sx::i18n::tr("Place .torrent files on any USB drive (/mnt/usb0..7) or in /data/torrents on your PS5, paste a magnet link, or scan the Web Drop QR code with your phone.");
}

void Session::torrent_activate(int index, bool secondary)
{
    if (index == 0)
    {
        torrent_menu();
        return;
    }
    if (index == 1)
    {
        ask_text([this](const std::string &v) {
            if (!trim(v).empty())
                torrent_open_input(trim(v), false);
        }, sx::i18n::tr("Enter magnet:? link, info-hash, or .torrent URL / path"));
        return;
    }
    if (index == 2)
    {
        torrent_show_web_drop_qr();
        return;
    }
    const int entry_idx = index - 3;
    if (entry_idx < 0 || static_cast<std::size_t>(entry_idx) >= torrent_entries_.size())
        return;
    const TorrentEntry entry = torrent_entries_[static_cast<std::size_t>(entry_idx)];
    auto open_entry = [this, entry](bool force_picker) {
        if (entry.is_local_file && !file_exists(entry.path_or_uri) && !entry.id.empty())
        {
            const std::string cached_torrent = data_dir_ + "/torrents/" + entry.id + ".torrent";
            if (file_exists(cached_torrent))
            {
                torrent_open_input(cached_torrent, force_picker);
                return;
            }
            if (!force_picker)
            {
                torrent_play_hash(entry.id, entry.name, entry.trackers, entry.webseeds, -1);
                return;
            }
        }
        torrent_open_input(entry.path_or_uri, force_picker);
    };
    auto saved = latest_resume(torrent_state_, entry.id);
    if(saved.file>=0 && saved.name.empty()) {
        std::vector<bt::FileInfo> files;std::string error;
        if(bt::Engine::get().wait_metadata(entry.id,files,nullptr,0,&error) && size_t(saved.file)<files.size())
            saved.name=short_filename(files[size_t(saved.file)].path);
    }
    if(saved.name.empty())saved.name=sx::i18n::tr("File ")+std::to_string(saved.file+1);
    if (!secondary) {
        if (saved.file >= 0) torrent_play_hash(entry.id, saved.name, entry.trackers, entry.webseeds, saved.file);
        else open_entry(false);
        return;
    }
    std::vector<std::string> actions;
    if (saved.file >= 0) {
        actions.push_back(sx::i18n::tr("Resume from ") + format_resume_time(saved.position));
        actions.push_back(sx::i18n::tr("Start over"));
    } else actions.push_back(sx::i18n::tr("Stream default video file"));
    actions.push_back(sx::i18n::tr("Choose another file"));
    if (!entry.id.empty()) actions.push_back(sx::i18n::tr("Remove from recent list"));
    const int count = static_cast<int>(actions.size());
    open_dropdown(entry.name, actions, 0, [this, entry, open_entry, saved, count, generation=profile_generation_](int pick) {
        if (generation != profile_generation_ || !profile_ready_ || !prefs_.builtin_torrents ||
            watching || resuming_ || pick < 0 || pick >= count) return;
        if (saved.file >= 0 && pick < 2) {
            torrent_play_hash(entry.id, saved.name, entry.trackers, entry.webseeds, saved.file, pick == 1);
            return;
        }
        if (saved.file < 0 && pick == 0) { open_entry(false); return; }
        const int action = pick - (saved.file >= 0 ? 2 : 1);
        if (action == 0) open_entry(true);
        else if (action == 1 && !entry.id.empty()) {
            auto &rec = torrent_state_["recent"];
            if (rec.is_array()) {
                rec.erase(std::remove_if(rec.begin(), rec.end(),
                    [&](const json &j) { return jstr(j, "id") == entry.id; }), rec.end());
                torrent_save(); torrent_build();
            }
        }
    });
    if (saved.file >= 0) dropdown.message = sx::i18n::tr("File: ") + saved.name;
}

void Session::torrent_play_hash(const std::string &info_hash, const std::string &name,
                                const std::vector<std::string> &trackers,
                                const std::vector<std::string> &webseeds, int file_idx, bool from_beginning)
{
    if (watching || resuming_)
        return;
    cancel_autoplay();
    next_preparation_cancel();
    close_detail();

    if(!prefs_.builtin_torrents || !profile_ready_)return;
    bt::Engine::get().set_cache_bytes(static_cast<int64_t>(std::max(1, prefs_.torrent_cache_gb)) << 30);
    bt::Engine::get().start_with_webseeds(info_hash, trackers, webseeds);
    if (file_idx >= 0)
        bt::Engine::get().select_file(info_hash, file_idx);

    const std::string item_id = "torrent:" + info_hash + ":" + std::to_string(file_idx);
    if (from_beginning) {
        auto &resume = torrent_state_["resume"];
        if (resume.is_object()) resume.erase(item_id);
        torrent_save();
    }
    d_item_ = Item{};
    d_item_.id = item_id;
    d_item_.name = name.empty() ? (sx::i18n::tr("Torrent ") + info_hash.substr(0, 8)) : name;
    d_item_.type = "torrent";
    d_meta_ = ::Meta{};
    d_meta_loaded_ = false;
    d_video_id_ = item_id;
    detail = DetailContent{};
    detail.title.year = sx::i18n::tr("Torrent video");

    ::Stream st;
    st.info_hash = info_hash;
    st.file_idx = file_idx;
    st.sources = trackers;
    st.name = sx::i18n::tr("Torrent Stream");
    st.description = d_item_.name;
    st.filename = d_item_.name;
    st.addon = sx::i18n::tr("Torrent Engine");
    play_stream(st, false);
}

void Session::torrent_present_files(const std::string &info_hash, const std::string &name,
                                    const std::vector<bt::FileInfo> &files,
                                    const std::vector<std::string> &trackers,
                                    const std::vector<std::string> &webseeds,
                                    int preferred_idx, bool force_picker)
{
    if (files.empty())
    {
        say(sx::i18n::tr("This torrent contains no files."), 6.0, true);
        return;
    }
    int video_count = 0;
    int single_video_idx = -1;
    for (std::size_t i = 0; i < files.size(); ++i)
    {
        if (is_video_filename(files[i].path))
        {
            ++video_count;
            single_video_idx = static_cast<int>(i);
        }
    }
    if (!force_picker && preferred_idx >= 0 && static_cast<std::size_t>(preferred_idx) < files.size())
    {
        const std::string ftitle = short_filename(files[static_cast<std::size_t>(preferred_idx)].path);
        torrent_play_hash(info_hash, ftitle.empty() ? name : ftitle, trackers, webseeds, preferred_idx);
        return;
    }
    if (!force_picker && (files.size() == 1 || video_count == 1))
    {
        const int idx = single_video_idx >= 0 ? single_video_idx : 0;
        const std::string ftitle = short_filename(files[static_cast<std::size_t>(idx)].path);
        torrent_play_hash(info_hash, ftitle.empty() ? name : ftitle, trackers, webseeds, idx);
        return;
    }

    // Multiple files: show a clean file picker sorted by file index
    std::vector<std::string> options;
    std::vector<int> file_indices;
    const int guessed = bt::Engine::guess_file(files, -1, -1);
    options.push_back(sx::i18n::tr("Auto-pick largest video (") +
                      (guessed >= 0 ? short_filename(files[static_cast<std::size_t>(guessed)].path) : sx::i18n::tr("best match")) + ")");
    file_indices.push_back(guessed);

    for (std::size_t i = 0; i < files.size() && options.size() < 200; ++i)
    {
        const bool is_vid = is_video_filename(files[i].path);
        if (video_count > 0 && !is_vid && files[i].size < (5ll << 20))
            continue; // skip tiny .nfo / .txt padding files when video files exist
        const std::string label = std::to_string(i + 1) + ". " + short_filename(files[i].path) +
                                  " (" + format_bytes(files[i].size) + ")";
        options.push_back(label);
        file_indices.push_back(static_cast<int>(i));
    }

    const auto generation=profile_generation_;
    open_dropdown(name.empty() ? sx::i18n::tr("Choose file to stream") : name, options, 0,
                  [this, generation, info_hash, name, files, trackers, webseeds, file_indices](int pick) {
                      if(generation!=profile_generation_ || !prefs_.builtin_torrents || !profile_ready_)return;
                      if (pick < 0 || static_cast<std::size_t>(pick) >= file_indices.size())
                          return;
                      const int fidx = file_indices[static_cast<std::size_t>(pick)];
                      std::string play_title = name;
                      if (fidx >= 0 && static_cast<std::size_t>(fidx) < files.size())
                          play_title = short_filename(files[static_cast<std::size_t>(fidx)].path);
                      torrent_play_hash(info_hash, play_title, trackers, webseeds, fidx);
                  });
}

void Session::torrent_open_input(const std::string &raw_input, bool force_file_picker)
{
    if(!prefs_.builtin_torrents || !profile_ready_ || watching || resuming_)return;
    std::string input = trim(raw_input);
    while (input.size() >= 2 &&
           ((input.front() == '"' && input.back() == '"') || (input.front() == '\'' && input.back() == '\'')))
    {
        input = trim(input.substr(1, input.size() - 2));
    }
    if (input.empty())
        return;

    auto record_recent = [this](const std::string &id, const std::string &name, const std::string &uri,
                                const std::string &subtitle, const std::vector<std::string> &trackers,
                                const std::vector<std::string> &webseeds) {
        if (!torrent_state_.is_object())
            torrent_state_ = json::object();
        auto &rec = torrent_state_["recent"];
        if (!rec.is_array())
            rec = json::array();
        rec.erase(std::remove_if(rec.begin(), rec.end(),
                                 [&](const json &j) { return jstr(j, "id") == id || jstr(j, "uri") == uri; }),
                  rec.end());
        json item = {{"id", id}, {"name", name}, {"uri", uri}, {"subtitle", subtitle},
                     {"trackers", trackers}, {"webseeds", webseeds}};
        rec.insert(rec.begin(), std::move(item));
        while (rec.size() > 40)
            rec.erase(rec.end() - 1);
        torrent_save();
        torrent_build();
    };

    // 1. Local .torrent file path
    if (starts_with(input, "/") || (ends_with(lower(input), ".torrent") && file_exists(input)))
    {
        bt::TorrentMeta meta;
        std::string err;
        bt::Engine::get().set_cache_bytes(static_cast<int64_t>(std::max(1, prefs_.torrent_cache_gb)) << 30);
        if (!bt::Engine::get().load_torrent_file(input, meta, &err))
        {
            say(err.empty() ? sx::i18n::tr("Could not open .torrent file") : err, 7.0, true);
            return;
        }
        const std::string display = meta.name.empty() ? short_filename(input) : meta.name;
        record_recent(meta.info_hash_hex, display, input,
                      sx::i18n::tr("Local .torrent · ") + format_bytes(meta.total_size) + " · " +
                          std::to_string(meta.files.size()) + " file(s)",
                      meta.trackers, meta.webseeds);
        torrent_present_files(meta.info_hash_hex, display, meta.files, meta.trackers, meta.webseeds, -1,
                              force_file_picker);
        return;
    }

    // 2. Magnet link or bare 40-char Hex / 32-char Base32 Info-Hash
    bt::MagnetInfo mi;
    std::string mag_err;
    if (bt::Engine::parse_magnet(input, mi, &mag_err))
    {
        const std::string display = mi.display_name.empty() ? (sx::i18n::tr("Magnet ") + mi.info_hash_hex.substr(0, 10)) : mi.display_name;
        record_recent(mi.info_hash_hex, display, input, sx::i18n::tr("Magnet Stream"), mi.trackers, mi.webseeds);
        if (!force_file_picker)
        {
            torrent_play_hash(mi.info_hash_hex, display, mi.trackers, mi.webseeds, mi.select_only_file);
            return;
        }
        // User explicitly asked to pick a file from a magnet: wait for metadata in the background
        say(sx::i18n::tr("Fetching file list from peers for ") + display + "...", 6.0);
        bt::Engine::get().set_cache_bytes(static_cast<int64_t>(std::max(1, prefs_.torrent_cache_gb)) << 30);
        bt::Engine::get().start_with_webseeds(mi.info_hash_hex, mi.trackers, mi.webseeds);
        if (torrent_cancel_)
            *torrent_cancel_ = true;
        auto cancel = std::make_shared<std::atomic<bool>>(false);
        torrent_cancel_ = cancel;
        struct WaitRes
        {
            bool ok = false;
            std::vector<bt::FileInfo> files;
            std::string error;
        };
        const std::string hash = mi.info_hash_hex;
        const auto trackers = mi.trackers;
        const auto webseeds = mi.webseeds;
        bg<WaitRes>(
            [hash, cancel]() {
                WaitRes r;
                r.ok = bt::Engine::get().wait_metadata(hash, r.files, cancel.get(), 60.0, &r.error);
                return r;
            },
            [this, hash, display, trackers, webseeds, cancel](WaitRes &r) {
                if (cancel->load() || !prefs_.builtin_torrents || !profile_ready_)
                    return;
                if (!r.ok)
                {
                    if (r.error == "cancelled")
                        return;
                    say(sx::i18n::tr("Could not fetch file list yet (") + r.error + sx::i18n::tr("). Starting stream directly..."), 6.0);
                    torrent_play_hash(hash, display, trackers, webseeds, -1);
                    return;
                }
                torrent_present_files(hash, display, r.files, trackers, webseeds, -1, true);
            });
        return;
    }

    // 3. Remote HTTP / HTTPS .torrent URL
    if (starts_with(lower(input), "http://") || starts_with(lower(input), "https://"))
    {
        say(sx::i18n::tr("Downloading .torrent file..."), 4.0);
        if (torrent_cancel_)
            *torrent_cancel_ = true;
        auto cancel = std::make_shared<std::atomic<bool>>(false);
        torrent_cancel_ = cancel;
        struct FetchRes
        {
            HttpResponse http;
        };
        bg<FetchRes>(
            [input, cancel]() {
                FetchRes r;
                r.http = http_get(input, 25, cancel.get());
                return r;
            },
            [this, input, force_file_picker, record_recent, cancel](FetchRes &r) {
                if (cancel->load() || !prefs_.builtin_torrents || !profile_ready_)
                    return;
                if (!r.http.ok() || r.http.body.empty())
                {
                    say(sx::i18n::tr("Could not download .torrent URL: ") + r.http.describe(), 7.0, true);
                    return;
                }
                bt::TorrentMeta meta;
                std::string err;
                bt::Engine::get().set_cache_bytes(static_cast<int64_t>(std::max(1, prefs_.torrent_cache_gb)) << 30);
                if (!bt::Engine::get().load_torrent_buffer(r.http.body, meta, &err))
                {
                    // Maybe the URL redirected to a magnet link?
                    if (starts_with(lower(trim(r.http.body)), "magnet:"))
                    {
                        torrent_open_input(trim(r.http.body), force_file_picker);
                        return;
                    }
                    say(err.empty() ? sx::i18n::tr("Downloaded URL is not a valid .torrent file") : err, 7.0, true);
                    return;
                }
                const std::string display = meta.name.empty() ? short_filename(input) : meta.name;
                record_recent(meta.info_hash_hex, display, input,
                              sx::i18n::tr("Remote .torrent · ") + format_bytes(meta.total_size), meta.trackers, meta.webseeds);
                torrent_present_files(meta.info_hash_hex, display, meta.files, meta.trackers, meta.webseeds, -1,
                                      force_file_picker);
            });
        return;
    }

    say(sx::i18n::tr("Enter a valid magnet:? link, 40-char info-hash, .torrent URL, or local .torrent path."), 7.0, true);
}

bool Session::torrent_try_search_input(const std::string &query)
{
    const std::string q = trim(query);
    if (q.empty())
        return false;
    bt::MagnetInfo mi;
    if (starts_with(lower(q), "magnet:") || bt::Engine::parse_magnet(q, mi) ||
        starts_with(q, "/mnt/usb") || starts_with(q, "/data/") || ends_with(lower(q), ".torrent"))
    {
        torrent_open_input(q, false);
        return true;
    }
    return false;
}

void Session::torrent_set_cache(int gb) {
    if(gb!=1&&gb!=2&&gb!=4&&gb!=6)return;
    if(watching||resuming_){say(sx::i18n::tr("Stop playback before changing the torrent cache"),5,true);return;}
    torrent_reset();bt::Engine::get().shutdown();
    prefs_.torrent_cache_gb=gb;
    bt::Engine::get().set_cache_bytes(int64_t(gb)<<30);
    bt::Engine::get().set_enabled(prefs_.builtin_torrents);
    save_prefs();torrent_load();refresh_settings();
}

void Session::torrent_poll_drop() {
    if(!torrent_drop_open_)return;
    if(!sign_in_open || !prefs_.builtin_torrents || !profile_ready_){
        bt::Engine::get().stop_web_drop();torrent_drop_open_=false;return;
    }
    bt::WebDropSubmission incoming;
    if(bt::Engine::get().take_web_submission(&incoming)){
        bt::Engine::get().stop_web_drop();torrent_drop_open_=false;sign_in_open=false;
        const auto generation=profile_generation_;
        open_dropdown(sx::i18n::tr("Received: ")+incoming.title,{sx::i18n::tr("Cancel"),sx::i18n::tr("Open torrent")},0,
            [this,incoming,generation](int choice){
                if(choice!=1 || generation!=profile_generation_ || !prefs_.builtin_torrents || !profile_ready_)return;
                if(incoming.kind=="buffer"){
                    bt::TorrentMeta meta;std::string error;
                    if(!bt::Engine::get().load_torrent_buffer(incoming.value,meta,&error)){say(error,6,true);return;}
                    torrent_open_input(data_dir_+"/torrents/"+meta.info_hash_hex+".torrent",true);
                }else torrent_open_input(incoming.value,true);
            });
    }else if(bt::Engine::get().web_drop_url().empty()){
        bt::Engine::get().stop_web_drop();torrent_drop_open_=false;sign_in_open=false;
        say(sx::i18n::tr("Web Drop pairing expired"));
    }
}

} // namespace sx
