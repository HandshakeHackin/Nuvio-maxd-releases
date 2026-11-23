#include "../ui/i18n.hpp"
#include "session.hpp"
#include "free_tv.h"
#include "stalker.h"
#include <algorithm>
namespace sx {
void Session::live_guide_reset() {
  if (live_guide_cancel_)
    *live_guide_cancel_ = true;
  live_guide_cancel_ = std::make_shared<std::atomic<bool>>(false);
  ++live_guide_generation_;
  live_guide_documents_.clear();live_guide_source_retry_.clear();live_guide_sources_pending_.clear();
  live_guide_xml_ = {};
  live_guide_document_.reset();
  live_guide_document_expires_ = 0;
  live_guide_window_.clear();
  live_guide_channels_.clear();
  live_guide_pending_.clear();
  live_guide_inflight_ = 0;
  live_guide_loading_ = false;
  live_guide_refresh_ = live_guide_tick_ = live_guide_redraw_ = 0;
}
int Session::live_guide_offset() const {
  for(const auto& row:free_tv::sources(live_state_))
    if(jstr(row,"id")==live_source_)return live_guide::offset_minutes(row);
  return 0;
}
void Session::live_guide_source_menu(bool clock) {
 std::vector<std::string> names,ids;
 for(const auto& row:free_tv::sources(live_state_)){names.push_back(jstr(row,"id")==free_tv::source_id?i18n::tr("Free live TV"):jstr(row,"name"));ids.push_back(jstr(row,"id"));}
 const auto profile=profile_generation_;const auto view=live_source_;
 if(ids.empty()){say(i18n::tr("Choose a Live TV source first"),6);return;}
 open_dropdown(i18n::tr("Choose a source for guide settings"),names,0,[this,profile,view,ids,clock](int pick){
  if(profile!=profile_generation_||view!=live_source_||pick<0||size_t(pick)>=ids.size())return;
  if(clock)live_guide_clock(ids[size_t(pick)]);else live_guide_configure(ids[size_t(pick)]);
 });
}
void Session::live_guide_clock(const std::string& requested) {
 if(live_source_==unified_tv::source_id&&requested.empty()){live_guide_source_menu(true);return;}
 const auto source=requested.empty()?live_source_:requested;const auto view=live_source_;
 if(source.empty()){say(i18n::tr("Choose a Live TV source first"),5);return;}
 live_tv::Channel channel;channel.source_id=source;
 const auto config=unified_tv::source(live_state_,channel,source);
 const auto profile=profile_generation_;
 std::vector<std::string> rows;
 for(int n=-720;n<=720;n+=30)rows.push_back(n==0?i18n::tr("No correction (default)"):std::string(n>0?"+":"")+std::to_string(n)+" minutes");
 open_dropdown(i18n::tr("Provider guide clock correction"),rows,(live_guide::offset_minutes(config)+720)/30,[this,source,view,profile](int pick){
  if(profile!=profile_generation_||view!=live_source_||pick<0||pick>48)return;
  const auto previous=live_state_;
  if(source==free_tv::source_id)live_state_["free_tv_guide_offset_minutes"]=-720+pick*30;
  for(auto& row:live_state_["sources"])if(jstr(row,"id")==source)row["guide_offset_minutes"]=-720+pick*30;
  if(!live_save()){live_state_=previous;return;}
  auto document=live_guide_document_;auto expires=live_guide_document_expires_;auto documents=live_guide_documents_;
  live_guide_reset();live_guide_document_=std::move(document);live_guide_document_expires_=expires;live_guide_documents_=std::move(documents);
  live_build();
 });
}
int Session::live_guide_offset(const live_tv::Channel& c) const {
 return live_source_==unified_tv::source_id?live_guide::offset_minutes(unified_tv::source(live_state_,c,live_source_)):live_guide_offset();
}
const live_guide::Listings* Session::live_guide_programmes(const live_tv::Channel& c) const {
 const auto short_epg=live_guide_channels_.find(unified_tv::guide_key(c,live_source_));
 if(short_epg!=live_guide_channels_.end())return &short_epg->second.programmes;
 if(live_source_!=unified_tv::source_id) {
  const auto xml=live_guide_xml_.channels.find(c.epg_id.empty()?c.id:c.epg_id);
  if(xml!=live_guide_xml_.channels.end())return &xml->second;
 }
 return nullptr;
}
std::pair<std::string, std::string>
Session::live_guide_labels(const live_tv::Channel &channel) const {
  if (!jbool(live_state_, "guide_enabled", true))
    return {};
  const auto* list=live_guide_programmes(channel);
  if (!list)
    return {live_guide_loading_ || live_guide_pending_.count(unified_tv::guide_key(channel,live_source_))
                ? sx::i18n::tr("Loading programme guide...")
                : sx::i18n::tr("Programme guide unavailable"),
            sx::i18n::tr("X: watch channel · Square: favorite")};
  const int offset=live_guide_offset(channel);
  auto pair = live_guide::current(*list, now_epoch_ms() / 1000-int64_t(offset)*60);
  return {pair.now ? sx::i18n::tr("Now: ") + live_guide::label(*pair.now,offset)
                   : sx::i18n::tr("Now: No listing supplied"),
          pair.next ? sx::i18n::tr("Next: ") + live_guide::label(*pair.next,offset)
                    : sx::i18n::tr("Next: No listing supplied")};
}
void Session::live_guide_configure(const std::string& requested) {
  if(live_source_==unified_tv::source_id&&requested.empty()){live_guide_source_menu(false);return;}
  const auto view=live_source_;
  const auto source=requested.empty()?live_source_:requested;
  if(source==free_tv::source_id){say(sx::i18n::tr("Free TV uses its supplied programme guide."),6);return;}
  const auto profile = profile_generation_;
  if (source.empty()) {
    say(sx::i18n::tr("Choose a Live TV source first"), 5);
    return;
  }
  ask_text(
      [this, source, view, profile](const std::string &input) {
        if (profile != profile_generation_ || view != live_source_)
          return;
        auto url = trim(input);
        if (!url.empty() && !live_tv::http_url(url)) {
          say(sx::i18n::tr("Enter an HTTP or HTTPS XMLTV URL"), 6, true);
          return;
        }
        const auto previous=live_state_;
        for (auto &row : live_state_["sources"])
          if (jstr(row, "id") == source) {
            row["epg_url"] = url;
            break;
          }
        if(!live_save()){live_state_=previous;return;}
        live_guide_reset();
        live_build();
      },
      sx::i18n::tr("XMLTV guide URL (empty uses the provider default)"));
}
void Session::live_guide_update(int focus) {
  if ((page_ != Page::livetv && page_ != Page::sports) || watching || multiview_active || live_loading_ ||
      live_channels_.empty() || !jbool(live_state_, "guide_enabled", true))
    return;
  if(live_source_==unified_tv::source_id){live_guide_unified_update(focus);return;}
  const auto& guide_indices=page_==Page::sports?sports_guide_indices_:live_indices_;
  if(page_==Page::sports){if(!sports_visible||guide_indices.empty())return;focus=sports_guide_cursor_;}
  const double time = now_seconds();
  if (time < live_guide_tick_)
    return;
  live_guide_tick_ = time + 1;
  if (!live_guide_cancel_)
    live_guide_cancel_ = std::make_shared<std::atomic<bool>>(false);
  json source;
  for (const auto &row : free_tv::sources(live_state_))
    if (jstr(row, "id") == live_source_)
      source = row;
  if (!source.is_object())
    return;
  const auto offset=int64_t(live_guide::offset_minutes(source))*60;
  const auto url = jstr(source, "epg_url", "").empty()
                       ? live_guide_auto_url_
                       : jstr(source, "epg_url");
  if (!url.empty()) {
    std::string sports_window;
    if(page_==Page::sports)for(auto index:guide_indices)sports_window+=std::to_string(index)+",";
    const auto window = page_==Page::sports?"sports:"+live_source_+":"+std::to_string(fnv1a(sports_window)):live_filter_ + ":" + std::to_string(live_page_);
    if (window != live_guide_window_) {
      // Keep one capped document while changing pages; do not re-download it
      // for every group or search.
      auto document = live_guide_document_;
      auto expires = live_guide_document_expires_;
      live_guide_reset();
      live_guide_window_ = window;
      live_guide_document_ = std::move(document);
      live_guide_document_expires_ = expires;
    }
    const auto cancel = live_guide_cancel_;
    const auto generation = live_guide_generation_,
               profile = profile_generation_;
    if (!live_guide_loading_ && time >= live_guide_refresh_) {
      std::set<std::string> ids;
      // XML parsing keeps a bounded near-future snapshot; channel IDs, never
      // name guesses.
      for (auto index : guide_indices) {
        const auto &c = live_channels_[index];
        ids.insert(c.epg_id.empty() ? c.id : c.epg_id);
      }
      const auto document =
          time < live_guide_document_expires_ ? live_guide_document_ : nullptr;
      live_guide_loading_ = true;
      live_guide_refresh_ =
          document ? live_guide_document_expires_ : time + 900;
      bg<live_guide::Result>(
          [url, ids, cancel, document, offset] {
            if (document)
              return live_guide::parse_xmltv(
                  *document, ids, now_epoch_ms() / 1000-offset, cancel.get());
            return live_guide::fetch_xmltv(url, ids, now_epoch_ms() / 1000-offset,
                                           cancel.get());
          },
          [this, generation, profile](auto &r) {
            if (generation != live_guide_generation_ ||
                profile != profile_generation_)
              return;
            live_guide_loading_ = false;
            if (r.error.empty()) {
              if (r.document) {
                live_guide_document_ = r.document;
                live_guide_document_expires_ = now_seconds() + 900;
              }
              live_guide_xml_ = std::move(r);
            } else
              live_guide_refresh_ = now_seconds() + 60;
            live_build();
          });
    }
  } else if (jstr(source, "type") == "xtream" || jstr(source,"type")=="stalker" || jstr(source,"type")=="free-tv") {
    const auto cancel = live_guide_cancel_;
    const auto generation = live_guide_generation_,
               profile = profile_generation_;
    for (int at = std::max(0, focus - 4);
         at < std::min<int>(guide_indices.size(), focus + 6) &&
         live_guide_inflight_ < 2;
         ++at) {
      const auto channel = live_channels_[guide_indices[at]].id;
      const auto country=live_channels_[guide_indices[at]].guide_country;
      auto cached = live_guide_channels_.find(channel);
      if (live_guide_pending_.count(channel) ||
          (cached != live_guide_channels_.end() &&
           cached->second.expires > time))
        continue;
      live_guide_pending_.insert(channel);
      ++live_guide_inflight_;
      bg<live_guide::Result>(
          [source, channel, country, cancel, offset] {
            if(jstr(source,"type")=="free-tv")return free_tv::guide(country.empty()?jstr(source,"country"):country,channel,now_epoch_ms()/1000-offset,cancel.get());
            if(jstr(source,"type")=="stalker")return stalker::guide(stalker::source(source),channel,now_epoch_ms()/1000-offset,cancel.get());
            return live_guide::fetch_xtream(
                jstr(source, "url"), jstr(source, "username"),
                jstr(source, "password"), channel, now_epoch_ms() / 1000-offset,
                cancel.get());
          },
          [this, generation, profile, channel](auto &r) {
            if (generation != live_guide_generation_ ||
                profile != profile_generation_)
              return;
            --live_guide_inflight_;
            live_guide_pending_.erase(channel);
            if (!live_guide_channels_.count(channel) &&
                live_guide_channels_.size() >= 256) {
              auto old = std::min_element(
                  live_guide_channels_.begin(), live_guide_channels_.end(),
                  [](const auto &a, const auto &b) {
                    return a.second.touched < b.second.touched;
                  });
              live_guide_channels_.erase(old);
            }
            auto &entry = live_guide_channels_[channel];
            entry.touched = now_seconds();
            entry.expires = entry.touched + (r.error.empty() ? 300 : 60);
            if (r.error.empty())
              entry.programmes = std::move(r.channels[channel]);
            live_build();
          });
    }
  }
  // Re-evaluate now/next across programme boundaries without restarting video.
  if (time >= live_guide_redraw_) {
    live_guide_redraw_ = time + 30;
    live_build();
  }
}
} // namespace sx
