#include "../ui/i18n.hpp"
#include "live_tv.h"
#include "live_usb.hpp"
#include "usb_reader.hpp"
#include "session.hpp"
#include "stalker.h"
#include "config_backup.h"
#include <algorithm>
#include <cstdio>
#include <fcntl.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace sx {
namespace {
std::string channel_key(const std::string &source, const live_tv::Channel &c) {
  return std::to_string(fnv1a(source + "\n" + c.id + "\n" + c.name));
}
std::string label(const std::string &s) {
  std::string o;
  for (unsigned char c : s)
    if (c >= 32 && c != 127)
      o += char(c);
  return o.substr(0, 100);
}
bool contains(const json &a, const std::string &s) {
  if (a.is_array())
    for (const auto &v : a)
      if (v.is_string() && v.get<std::string>() == s)
        return true;
  return false;
}
} // namespace
void Session::live_reset() {
  iptv_reset();
  live_guide_reset();
  live_guide_auto_url_.clear();
  if(multiview_active)multi_stop();
  if (live_cancel_)
    *live_cancel_ = true;
  if(live_usb_cancel_)*live_usb_cancel_=true;
  ++live_generation_;
  live_loading_ = false;
  live_channels_.clear();
  live_indices_.clear();
  live_state_ = json::object();
  live_source_.clear();
  live_group_.clear();
  live_query_.clear();
  live_view_ = 0;
  live_page_ = 0;
  live_filter_.clear();
  live_content.items.clear();
  changed_ |= kLive;
}
void Session::live_load() {
  live_reset();
  load_json(profile_dir_ + "/live-tv.json", live_state_);
  if (!live_state_.is_object())
    live_state_ = json::object();
  if (!jobj(live_state_, "sources").is_array())
    live_state_["sources"] = json::array();
  if (!jobj(live_state_, "favorites").is_array())
    live_state_["favorites"] = json::array();
  if (!jobj(live_state_, "recent").is_array())
    live_state_["recent"] = json::array();
  live_source_ = jstr(live_state_, "selected");
  iptv_load();
  live_build();
}
void Session::live_save() {
  if (!profile_ready_ || profile_dir_.empty())
    return;
  live_state_["selected"] = live_source_;
  const auto path = profile_dir_ + "/live-tv.json";
  if(!config_backup::save(path,live_state_.dump(1,'\t')))
    say(sx::i18n::tr("Could not save Live TV settings"),7,true);
}
void Session::live_build() {
  live_content.items.clear();
  live_content.guide_grid=jbool(live_state_,"guide_grid",false);
  live_content.guide_now=now_epoch_ms()/1000;
  live_indices_.clear();
  const auto filter = live_source_ + "\n" + live_group_ + "\n" + live_query_ +
                      "\n" + std::to_string(live_view_);
  if (filter != live_filter_) {
    live_page_ = 0;
    live_filter_ = filter;
  }
  sx::Addon manage;
  manage.name = sx::i18n::tr("Live TV sources & filters");
  manage.initial = sx::i18n::tr("TV");
  manage.colour = 0x0ea5a4;
  manage.version = live_loading_
                       ? sx::i18n::tr("Loading channels...")
                       : std::to_string(live_channels_.size()) + " channels";
  manage.types = "M3U Xtream Stalker";
  manage.description = sx::i18n::tr("X: sources and filters · Triangle: channel search · L1: groups · R1: All / Favorites / Recent");
  if (!live_group_.empty())
    manage.version += " · " + live_group_;
  if (!live_query_.empty())
    manage.version += sx::i18n::tr(" · Search: ") + live_query_;
  if (live_view_ == 1)
    manage.version += sx::i18n::tr(" · Favorites");
  else if (live_view_ == 2)
    manage.version += sx::i18n::tr(" · Recent");
  live_content.items.push_back(manage);
  std::vector<size_t> indices;
  const auto query = lower(live_query_);
  for (size_t i = 0; i < live_channels_.size(); ++i) {
    const auto &c = live_channels_[i];
    if (!c.in_group(live_group_))
      continue;
    if (!query.empty() &&
        lower(c.name).find(query) == std::string::npos)
      continue;
    const auto key = live_view_ ? channel_key(live_source_, c) : std::string();
    if (live_view_ == 1 && !contains(live_state_["favorites"], key))
      continue;
    if (live_view_ == 2 && !contains(live_state_["recent"], key))
      continue;
    indices.push_back(i);
  }
  if (live_view_ == 2) {
    const auto recent = live_state_["recent"];
    std::stable_sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
      const auto rank = [&](size_t i) {
        auto key = channel_key(live_source_, live_channels_[i]);
        size_t r = 0;
        for (const auto &v : recent) {
          if (v == key)
            break;
          ++r;
        }
        return r;
      };
      return rank(a) < rank(b);
    });
  }
  // Render pages remain bounded; the channel search/group filters cover larger
  // lists.
  live_page_ = std::min(
      live_page_, indices.empty() ? size_t(0) : (indices.size() - 1) / 500);
  for (size_t at = live_page_ * 500;
       at < indices.size() && at < (live_page_ + 1) * 500; ++at) {
    auto i = indices[at];
    const auto &c = live_channels_[i];
    sx::Addon row;
    const auto key = channel_key(live_source_, c);
    row.name = c.name;
    row.version = c.group;
    row.types =
        contains(live_state_["favorites"], key) ? sx::i18n::tr("Favorite Live") : sx::i18n::tr("Live");
    row.initial = std::to_string(live_indices_.size() + 1);
    row.colour = 0x0ea5a4;
    row.description = sx::i18n::tr("X: watch channel · Square: toggle favorite");
    if(live_content.guide_grid && jbool(live_state_,"guide_enabled",true)) {
      const live_guide::Listings* listings=nullptr;
      auto xml=live_guide_xml_.channels.find(c.epg_id.empty()?c.id:c.epg_id);
      if(xml!=live_guide_xml_.channels.end())listings=&xml->second;
      auto short_epg=live_guide_channels_.find(c.id);
      if(short_epg!=live_guide_channels_.end())listings=&short_epg->second.programmes;
      if(listings)for(const auto& programme:*listings)
        row.programmes.push_back({programme.start+int64_t(live_guide_offset())*60,programme.end+int64_t(live_guide_offset())*60,programme.title});
    }
    row.identity=key;
    auto guide=live_guide_labels(c);row.programme_now=std::move(guide.first);row.programme_next=std::move(guide.second);
    live_content.items.push_back(std::move(row));
    live_indices_.push_back(i);
  }
  if (indices.size() > 500)
    live_content.items[0].description =
        sx::i18n::tr("Page ") + std::to_string(live_page_ + 1) + " of " +
        std::to_string((indices.size() + 499) / 500) +
        sx::i18n::tr(". Open sources and filters for the next/previous page, or search.");
  else if (!live_loading_ && live_indices_.empty())
    live_content.items[0].description =
        live_channels_.empty()
            ? sx::i18n::tr("Add an M3U playlist, Xtream account or Stalker portal to start watching.")
            : sx::i18n::tr("No channels match these filters. Open sources and filters to change them.");
  changed_ |= kLive;
}
void Session::live_reload() {
  live_guide_reset();live_guide_auto_url_.clear();
  iptv_probe();
  if (!profile_ready_) {
    say(sx::i18n::tr("Choose a profile first"), 5);
    return;
  }
  json source;
  for (const auto &s : live_state_["sources"])
    if (jstr(s, "id") == live_source_)
      source = s;
  if (!source.is_object()) {
    live_build();
    return;
  }
  if (live_cancel_)
    *live_cancel_ = true;
  live_cancel_ = std::make_shared<std::atomic<bool>>(false);
  const auto cancel = live_cancel_;
  const auto generation = ++live_generation_, profile = profile_generation_;
  live_loading_ = true;
  live_build();
  g_tasks.run<live_tv::Result>(
      [source, cancel, directory=profile_dir_] {
        if(jstr(source,"type")=="stalker")return stalker::channels(stalker::source(source),cancel.get());
        if(jstr(source,"type")=="usb")return live_usb::load(directory,jstr(source,"id"),cancel.get());
        if (jstr(source, "type") == "xtream")
          return live_tv::fetch_xtream(jstr(source, "url"),
                                       jstr(source, "username"),
                                       jstr(source, "password"), cancel.get());
        return live_tv::fetch(jstr(source, "url"), cancel.get());
      },
      [this, generation, profile](live_tv::Result &r) {
        if (generation != live_generation_ || profile != profile_generation_)
          return;
        live_loading_ = false;
        if (!r.error.empty()) {
          say(r.error, 10, true);
          live_build();
          return;
        }
        live_guide_auto_url_ = r.guide_url;
        live_channels_ = std::move(r.channels);
        live_build();
        say(sx::i18n::tr("Live TV channels loaded"));
      });
}
void Session::live_groups() {
  std::vector<std::string> groups{sx::i18n::tr("All groups")};
  std::set<std::string> seen;
  for (const auto &c : live_channels_) {
    if (seen.insert(c.group).second) groups.push_back(c.group);
    for (const auto& group : c.groups)
      if (seen.insert(group).second) groups.push_back(group);
  }
  const auto generation = live_generation_;
  open_dropdown(sx::i18n::tr("Live TV groups"), groups, 0, [this, groups, generation](int i) {
    if (generation != live_generation_ || i < 0 || size_t(i) >= groups.size())
      return;
    live_group_ = i ? groups[i] : "";
    live_build();
  });
}
void Session::live_search() {
  const auto generation = live_generation_;
  ask_text(
      [this, generation](const std::string &text) {
        if (generation != live_generation_)
          return;
        live_query_ = label(trim(text));
        live_build();
      },
      sx::i18n::tr("Search live channels (empty clears search)"));
}
void Session::live_cycle() {
  live_view_ = (live_view_ + 1) % 3;
  live_build();
}
void Session::live_activate(int index, bool favorite) {
  if (index == 0) {
    live_menu();
    return;
  }
  if (index < 1 || size_t(index - 1) >= live_indices_.size() || watching ||
      resuming_)
    return;
  const auto &channel = live_channels_[live_indices_[index - 1]];
  const auto key = channel_key(live_source_, channel);
  if (favorite) {
    auto &f = live_state_["favorites"];
    auto it = std::find(f.begin(), f.end(), json(key));
    if (it != f.end())
      f.erase(it);
    else if (f.size() < 1000)
      f.push_back(key);
    else {
      say(sx::i18n::tr("Favorites limit reached"), 5);
      return;
    }
    live_save();
    live_build();
    return;
  }
  live_play(channel);
}
void Session::live_play(const live_tv::Channel& channel) {
  if(channel.url.rfind(stalker::marker,0)==0) {
    json source;for(const auto& s:live_state_["sources"])if(jstr(s,"id")==live_source_)source=s;
    if(jstr(source,"type")!="stalker" || live_loading_)return;
    const auto generation=live_generation_,profile=profile_generation_;const auto cancel=live_cancel_;
    live_loading_=true;live_build();
    bg<stalker::Link>([source,channel,cancel]{return stalker::resolve(stalker::source(source),channel.url.substr(std::char_traits<char>::length(stalker::marker)),false,0,cancel.get());},
      [this,channel=channel,generation,profile](auto& r)mutable{
        if(generation!=live_generation_ || profile!=profile_generation_ || !profile_ready_)return;
        live_loading_=false;live_build();if(!r.error.empty()){say(r.error,8,true);return;}
        channel.url=r.url;channel.headers=r.headers;live_play(channel);
      });return;
  }
  const auto key=channel_key(live_source_,channel);
  // Live channels never enter title Continue Watching, Trakt or episode flows.
  d_item_ = Item{};
  d_item_.id = "live:" + key;
  d_item_.name = channel.name;
  d_item_.type = "tv";
  d_meta_ = Meta{};
  d_meta_loaded_ = false;
  d_video_id_ = d_item_.id;
  detail = DetailContent{};
  ::Stream stream;
  stream.url = channel.url;
  stream.name = channel.name;
  stream.addon = sx::i18n::tr("Live TV");
  stream.request_headers = channel.headers;
  play_stream(stream, false);
  if (watching) {
    auto &recent = live_state_["recent"];
    recent.erase(std::remove(recent.begin(), recent.end(), json(key)),
                 recent.end());
    recent.insert(recent.begin(), key);
    while (recent.size() > 30)
      recent.erase(recent.end() - 1);
    live_save();
    live_build();
  }
}
void Session::live_add(json source, int step) {
  if (!profile_ready_)
    return;
  const auto profile = profile_generation_;
  const bool xtream = jstr(source, "type") == "xtream";
  const bool portal = jstr(source,"type")=="stalker";
  const std::vector<std::string> prompts =
      portal ? std::vector<std::string>{sx::i18n::tr("Live TV source name"),"Stalker / Ministra portal URL","Provider-assigned MAC address","Portal username (optional)","Portal password (optional)","Portal serial number (optional)"} : xtream
          ? std::vector<std::string>{sx::i18n::tr("Live TV source name"),
                                     "Xtream server URL (http:// or https://)",
                                     sx::i18n::tr("Xtream username"), sx::i18n::tr("Xtream password")}
          : std::vector<std::string>{sx::i18n::tr("Live TV source name"),
                                     "M3U playlist URL (http:// or https://)"};
  if (step == int(prompts.size())) {
    if(portal && !stalker::valid(stalker::source(source))){say("Enter the portal URL and MAC address supplied by your provider.",8,true);return;}
    if (live_state_["sources"].size() >= 16) {
      say(sx::i18n::tr("You can save up to 16 Live TV sources"), 5);
      return;
    }
    source["id"] =
        std::to_string(fnv1a(std::to_string(now_epoch_ms()) +
                             jstr(source, "url") + jstr(source, "name")));
    live_source_ = jstr(source, "id");
    live_state_["sources"].push_back(source);
    live_channels_.clear();
    live_group_.clear();
    live_query_.clear();
    live_view_ = 0;
    live_save();
    iptv_sources_changed();
    live_reload();
    return;
  }
  const char *key = portal ? (step==0?"name":step==1?"url":step==2?"mac":step==3?"username":step==4?"password":"serial") : step == 0   ? "name"
                    : step == 1 ? "url"
                    : step == 2 ? "username"
                                : "password";
  ask_text(
      [this, source, step, key, profile,portal](const std::string &value) mutable {
        if (profile != profile_generation_ || !profile_ready_)
          return;
        auto v = std::string(key)=="password" ? value : trim(value);
        if ((v.empty() && !(portal && step>=3)) || v.size() > (step == 1 ? 8192u : 256u)) {
          say(sx::i18n::tr("The field is empty or too long. Add the source again."), 6);
          return;
        }
        if (step == 1 && !live_tv::http_url(v)) {
          say(sx::i18n::tr("Enter an HTTP or HTTPS URL. Add the source again."), 6);
          return;
        }
        source[key] = step == 0 ? label(v) : v;
        live_add(source, step + 1);
      },
      prompts[step],std::string(key)=="password" || std::string(key)=="serial");
}
void Session::live_menu() {
  if(backup_busy_){say(sx::i18n::tr("Backup operation in progress..."));return;}
  if (!profile_ready_) {
    say(sx::i18n::tr("Choose a profile before configuring Live TV"), 6);
    return;
  }
  const auto generation = profile_generation_;
  std::vector<std::string> options{
      sx::i18n::tr("Add M3U playlist"),  sx::i18n::tr("Add Xtream Codes account"),
      sx::i18n::tr("Choose source"),     sx::i18n::tr("Reload channels"),
      sx::i18n::tr("Channel groups"),    sx::i18n::tr("Search channels"),
      sx::i18n::tr("All channels"),      sx::i18n::tr("Favorites"),
      sx::i18n::tr("Recent channels"),   sx::i18n::tr("Remove current source"),
      sx::i18n::tr("Next channel page"), sx::i18n::tr("Previous channel page"), sx::i18n::tr("Four-screen TV (experimental)"), sx::i18n::tr("Check IPTV Library availability"), sx::i18n::tr("Set XMLTV guide URL"), sx::i18n::tr("Refresh programme guide"), jbool(live_state_,"guide_enabled",true)?sx::i18n::tr("Programme guide: On"):sx::i18n::tr("Programme guide: Off"), sx::i18n::tr("Correct provider guide clock"), jbool(live_state_,"guide_grid",false)?sx::i18n::tr("Guide layout: Time grid"):sx::i18n::tr("Guide layout: Channel tiles"), sx::i18n::tr("Import playlist from USB"), sx::i18n::tr("Replace current USB playlist"),sx::i18n::tr("Add Stalker / Ministra portal")};
  open_dropdown(sx::i18n::tr("Live TV"), options, 0, [this, generation](int i) {
    if (generation != profile_generation_)
      return;
    if(i==21){live_add(json{{"type","stalker"}},0);return;}
    if(i==19||i==20){live_usb_import(i==20);return;}
    if(i==18){live_state_["guide_grid"]=!jbool(live_state_,"guide_grid",false);live_save();live_build();return;}
    if (i == 17) {live_guide_clock();return;}
    if (i == 14) {live_guide_configure();return;}
    if (i == 15) {live_guide_reset();live_build();say(sx::i18n::tr("Programme guide will refresh in the background"),5);return;}
    if (i == 16) {live_state_["guide_enabled"]=!jbool(live_state_,"guide_enabled",true);live_guide_reset();live_save();live_build();return;}
    if (i == 12) {multi_choose();return;}
    if (i == 13) {iptv_probe();say(sx::i18n::tr("Checking IPTV movie and series availability in the background"),6);return;}
    if (i == 0 || i == 1) {
      live_add(json{{"type", i ? "xtream" : "m3u"}}, 0);
      return;
    }
    if (i == 2) {
      std::vector<std::string> names, ids;
      for (const auto &s : live_state_["sources"]) {
        names.push_back(jstr(s, "name"));
        ids.push_back(jstr(s, "id"));
      }
      if (names.empty()) {
        say(sx::i18n::tr("Add a Live TV source first"), 5);
        return;
      }
      open_dropdown(sx::i18n::tr("Choose Live TV source"), names, 0,
                    [this, ids, generation](int n) {
                      if (generation != profile_generation_ || n < 0 ||
                          size_t(n) >= ids.size())
                        return;
                      live_source_ = ids[n];
                      live_channels_.clear();
                      live_group_.clear();
                      live_query_.clear();
                      live_view_ = 0;
                      live_save();
                      live_reload();
                    });
      return;
    }
    if (i == 3) {
      live_reload();
      return;
    }
    if (i == 4) {
      live_groups();
      return;
    }
    if (i == 5) {
      live_search();
      return;
    }
    if (i >= 6 && i <= 8) {
      live_view_ = i - 6;
      live_group_.clear();
      live_query_.clear();
      live_build();
      return;
    }
    if (i == 10 || i == 11) {
      if (i == 10)
        ++live_page_;
      else if (live_page_)
        --live_page_;
      live_build();
      return;
    }
    if (i == 9) {
      const auto source = live_source_;
      open_dropdown(
          sx::i18n::tr("Remove this Live TV source?"), {sx::i18n::tr("Cancel"), sx::i18n::tr("Remove source")}, 0,
          [this, source, generation](int n) {
            if (n != 1 || generation != profile_generation_)
              return;
            if (live_cancel_)
              *live_cancel_ = true;
            ++live_generation_;
            live_loading_ = false;
            // Remove this source's associated favorites and recent entries too.
            for (const auto &c : live_channels_) {
              auto key = channel_key(source, c);
              for (auto field : {"favorites", "recent"}) {
                auto &list = live_state_[field];
                list.erase(std::remove(list.begin(), list.end(), json(key)),
                           list.end());
              }
            }
            for(const auto& row:live_state_["sources"])if(jstr(row,"id")==source && jstr(row,"type")=="usb"){
              const auto path=live_usb::cache_path(profile_dir_,source);if(!path.empty())unlink(path.c_str());
            }
            auto &sources = live_state_["sources"];
            sources.erase(std::remove_if(sources.begin(), sources.end(),
                                         [&](const json &s) {
                                           return jstr(s, "id") == source;
                                         }),
                          sources.end());
            live_source_ = sources.empty() ? "" : jstr(sources[0], "id");
            live_channels_.clear();
            live_group_.clear();
            live_query_.clear();
            live_view_ = 0;
            live_save();
            iptv_sources_changed();
            live_build();
            live_reload();
          });
    }
  });
  dropdown.message =
      sx::i18n::tr("Use your own M3U playlist, Xtream subscription or Stalker/Ministra portal. Sources, favorites and recent channels are saved locally for this profile. IPTV Library appears separately when the service provides movies or series.");
}
} // namespace sx

namespace sx {
void Session::live_usb_import(bool replace,bool reader) {
  if(!profile_ready_ || watching || resuming_)return;
  const auto profile=profile_generation_;const auto directory=profile_dir_;
  std::string id=std::to_string(now_epoch_ms());
  if(replace){bool found=false;for(const auto& row:live_state_["sources"])if(jstr(row,"id")==live_source_ && jstr(row,"type")=="usb"){id=live_source_;found=true;}
    if(!found){say(sx::i18n::tr("Choose an imported USB source first."),6);return;}}
  else {size_t count=0;for(const auto& row:live_state_["sources"])if(jstr(row,"type")=="usb")++count;
    if(count>=16){say(sx::i18n::tr("Up to 16 USB playlists per profile. Replace or remove an existing source."),7);return;}}
  const auto scan_cancel=std::make_shared<std::atomic<bool>>(false);live_usb_cancel_=scan_cancel;
  open_dropdown(sx::i18n::tr("Scanning USB playlists"),{sx::i18n::tr("Cancel")},0,[scan_cancel](int){*scan_cancel=true;});
  dropdown.purpose=DropdownState::Purpose::usb_scan;
  const auto generation=++live_generation_;
  struct ScanResult {std::vector<live_usb::File> files;std::vector<live_usb::MountDiagnostics> mounts;std::string error;};
  bg<ScanResult>([scan_cancel,reader]{std::vector<std::string> roots;for(int i=0;i<8;++i)roots.push_back("/mnt/usb"+std::to_string(i));
    dlog("%s",reader?"USB reader scan: existing loader, 8 roots":"USB playlist scan: native directory imports, 8 roots");ScanResult result;
    if(reader){dlog("USB reader scan: explicit helper request");auto scan=usb_reader_client::scan(*scan_cancel);result.files=std::move(scan.files);result.mounts=std::move(scan.mounts);result.error=std::move(scan.error);}
    else result.files=live_usb::scan(roots,&result.mounts,scan_cancel.get());
    for(size_t i=0;i<result.mounts.size();++i)dlog("%s%s",reader?"USB reader ":"",result.mounts[i].summary(i).c_str());
    dlog("USB playlist scan: completed, %zu candidate(s)",result.files.size());return result;},
    [this,profile,directory,id,replace,generation,reader](auto& scan){
      auto& files=scan.files;
      if(profile!=profile_generation_||generation!=live_generation_||!dropdown_open||dropdown.purpose!=DropdownState::Purpose::usb_scan)return;
      if(!scan.error.empty()){dropdown_cancel();say(scan.error,10,true);return;}
      if(files.empty()){dropdown_cancel();
        bool denied=false;for(const auto& d:scan.mounts)denied|=d.access_denied;
        if(denied&&!reader){
          open_dropdown(sx::i18n::tr("USB access is restricted"),{sx::i18n::tr("Cancel"),sx::i18n::tr("Use USB reader helper")},0,[this,profile,generation,replace](int choice){if(choice==1&&profile==profile_generation_&&generation==live_generation_)live_usb_import(replace,true);});
          dropdown.message=sx::i18n::tr("The reader runs with the existing payload loader's privileges to read USB playlists. Max'd stays sandboxed. Only a selected, validated playlist is saved privately to this profile.");return;
        }
        bool opened=false,errors=false;for(const auto& d:scan.mounts){opened|=d.directories>0;errors|=!d.missing&&(d.access_denied||d.non_directory||d.metadata_failures||d.directory_failures||d.read_errno||d.open_errno);}
        say(errors?sx::i18n::tr("USB scan could not fully read the drive. Scan diagnostics were saved to the app log."):
          opened?sx::i18n::tr("USB directories opened, but no M3U/M3U8 playlists found within four folder levels."):
          sx::i18n::tr("No USB mount visible to this app at /mnt/usb0 through /mnt/usb7. Reconnect the drive and scan again."),10);return;}
      std::vector<std::string> rows;for(const auto& file:files)rows.push_back(file.root+"/"+file.path);
      open_dropdown(sx::i18n::tr("Import USB playlist"),rows,0,[this,profile,directory,id,replace,generation,files](int pick){
        if(profile!=profile_generation_||generation!=live_generation_||pick<0||pick>=int(files.size()))return;
        if(live_cancel_)*live_cancel_=true;live_cancel_=std::make_shared<std::atomic<bool>>(false);const auto cancel=live_cancel_;live_usb_cancel_=cancel;
        const auto file=files[size_t(pick)];
        open_dropdown(sx::i18n::tr("Importing USB playlist"),{sx::i18n::tr("Cancel")},0,[cancel](int){*cancel=true;});
        dropdown.purpose=DropdownState::Purpose::usb_import;
        bg<live_tv::Result>([file,directory,id,cancel]{dlog("USB playlist import: validating selected file");auto result=file.reader?usb_reader_client::import_playlist(file,directory,id,*cancel):live_usb::import_playlist(file,directory,id,cancel.get());dlog("USB playlist import: %s",result.error.empty()?"saved profile copy":"cancelled or rejected");return result;},
          [this,profile,generation,id,replace,file,cancel](auto& result){
            if(profile!=profile_generation_||generation!=live_generation_)return;
            if(dropdown_open&&dropdown.purpose==DropdownState::Purpose::usb_import)dropdown_cancel();
            if(!result.error.empty()){say(result.error,8,true);return;}
            if(replace){for(auto& row:live_state_["sources"])if(jstr(row,"id")==id)row["name"]=label(file.path);}
            else live_state_["sources"].push_back({{"id",id},{"type","usb"},{"name",label(file.path)}});
            live_source_=id;live_group_.clear();live_query_.clear();live_view_=0;live_save();live_reload();
            say(sx::i18n::tr("Playlist copied to this profile. The USB drive can be removed."),6);
        });
      });
      dropdown.message=sx::i18n::tr("Choose a channel playlist. Single-stream HLS manifests and local media paths are not supported. Imports keep a private copy; online channels still need network access.");
  });
}
}
