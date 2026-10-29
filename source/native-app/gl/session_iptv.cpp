// IPTV VOD is deliberately isolated from addon discovery, history and trackers.
#include "session.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace sx {
namespace {
std::string short_label(const std::string &s) {
  return s.size() > 24 ? s.substr(0, 21) + "..." : s;
}
} // namespace
void Session::iptv_reset() {
  if (iptv_cancel_)
    *iptv_cancel_ = true;
  if (iptv_probe_cancel_)
    *iptv_probe_cancel_ = true;
  ++iptv_generation_;
  ++iptv_probe_generation_;
  iptv_accounts_.clear();
  iptv_probe_accounts_.clear();
  iptv_state_ = json::object();
  iptv_catalog_ = {};
  iptv_hits_.clear();
  iptv_searching_saved_ = false;
  iptv_letter_.clear();
  iptv_indices_.clear();
  iptv_source_.clear();
  iptv_category_.clear();
  iptv_query_.clear();
  iptv_series_.clear();
  iptv_series_name_.clear();
  iptv_series_poster_.clear();
  iptv_status_.clear();
  iptv_page_ = 0;
  iptv_season_ = -1;
  iptv_kind_ = iptv::Kind::movies;
  iptv_loading_ = iptv_probing_ = iptv_visible = false;
  iptv_content = LibraryContent{};
  changed_ |= kIptv | kIptvNew;
}
void Session::iptv_save() {
  if (!profile_ready_ || profile_dir_.empty())
    return;
  iptv_state_["selected"] = iptv_source_;
  iptv_state_["kind"] = iptv::kind_name(iptv_kind_);
  iptv_state_["category"] = iptv_category_;
  if (!iptv::save_private(profile_dir_ + "/iptv-library.json", iptv_state_))
    say("Could not save IPTV Library preferences", 6, true);
}
void Session::iptv_load() {
  iptv_state_ = iptv::load_private(profile_dir_ + "/iptv-library.json");
  iptv_accounts_ = iptv::accounts(jobj(live_state_, "sources"));
  iptv::reconcile(iptv_state_, iptv_accounts_);
  iptv_source_ = jstr(iptv_state_, "selected");
  iptv_category_ = jstr(iptv_state_, "category");
  iptv_kind_ = jstr(iptv_state_, "kind") == "series" ? iptv::Kind::series
                                                     : iptv::Kind::movies;
  iptv_build(true);
  iptv_probe();
}
void Session::iptv_sources_changed() {
  if (iptv_cancel_)
    *iptv_cancel_ = true;
  if (iptv_probe_cancel_)
    *iptv_probe_cancel_ = true;
  ++iptv_generation_;
  ++iptv_probe_generation_;
  iptv_loading_ = iptv_probing_ = false;
  iptv_hits_.clear();
  iptv_searching_saved_ = false;
  iptv_letter_.clear();
  auto next = iptv::accounts(jobj(live_state_, "sources"));
  for (const auto &a : iptv_accounts_)
    if (std::none_of(next.begin(), next.end(),
                     [&](const auto &n) { return n.id == a.id; }))
      iptv::remove_cache(profile_dir_ + "/iptv-cache", a.id);
  iptv_accounts_ = std::move(next);
  iptv::reconcile(iptv_state_, iptv_accounts_);
  iptv_catalog_ = {};
  iptv_series_.clear();
  iptv_query_.clear();
  iptv_status_.clear();
  iptv_build(true);
  iptv_save();
  iptv_probe();
}
void Session::iptv_probe() {
  if (!profile_ready_ || iptv_accounts_.empty() || iptv_probing_)
    return;
  iptv_probe_cancel_ = std::make_shared<std::atomic<bool>>(false);
  iptv_probe_accounts_ = iptv_accounts_;
  // New accounts are checked first; publish each result without waiting for
  // another disconnected provider to time out.
  const auto priority = [&](const auto &account) {
    return !iptv_state_["sources"].contains(account.id) ? 0
           : account.id == iptv_source_                 ? 1
                                                        : 2;
  };
  std::stable_sort(
      iptv_probe_accounts_.begin(), iptv_probe_accounts_.end(),
      [&](const auto &a, const auto &b) { return priority(a) < priority(b); });
  iptv_probing_ = true;
  iptv_probe_source(0, ++iptv_probe_generation_, profile_generation_,
                    iptv_probe_cancel_);
}
void Session::iptv_probe_source(
    size_t index, unsigned generation, unsigned profile,
    const std::shared_ptr<std::atomic<bool>> &cancel) {
  if (generation != iptv_probe_generation_ || profile != profile_generation_ ||
      cancel->load())
    return;
  if (index >= iptv_probe_accounts_.size()) {
    iptv_probing_ = false;
    return;
  }
  const auto account = iptv_probe_accounts_[index];
  g_tasks.run<iptv::Capabilities>(
      [account, cancel] { return iptv::probe(account, cancel.get()); },
      [this, index, generation, profile, cancel,
       id = account.id](iptv::Capabilities &result) {
        if (generation != iptv_probe_generation_ ||
            profile != profile_generation_ || cancel->load())
          return;
        const auto before = iptv_source_ + ":" + iptv::kind_name(iptv_kind_) +
                            ":" + iptv_category_;
        iptv::apply_capabilities(iptv_state_, id, result);
        if (id == iptv_source_ || iptv_source_.empty()) {
          if (!result.error.empty())
            iptv_status_ = "Availability check failed. Saved library retained; "
                           "Options to retry.";
          else
            iptv_status_.clear();
        }
        iptv_build();
        iptv_save();
        const auto after = iptv_source_ + ":" + iptv::kind_name(iptv_kind_) +
                           ":" + iptv_category_;
        if ((before != after || !iptv_visible) && !iptv_searching_saved_) {
          if (iptv_cancel_)
            *iptv_cancel_ = true;
          ++iptv_generation_;
          iptv_loading_ = false;
          iptv_catalog_ = {};
          iptv_series_.clear();
          iptv_build(true);
        }
        if (!iptv_searching_saved_ && iptv_visible && page_ == Page::iptv &&
            !watching && !iptv_loading_ &&
            (before != after || iptv_catalog_.entries.empty()))
          iptv_reload();
        iptv_probe_source(index + 1, generation, profile, cancel);
      });
}
void Session::iptv_build(bool reset) {
  iptv_visible = profile_ready_ && iptv::available(iptv_state_, iptv_accounts_);
  iptv_content.items.clear();
  iptv_content.filters.clear();
  iptv_indices_.clear();
  if (!iptv_visible) {
    iptv_content.status = "Import a VOD-enabled Xtream account under Live TV.";
    changed_ |= kIptv | kIptvNew;
    return;
  }
  auto valid = [&](const std::string &id) {
    return !iptv::categories(iptv_state_, id, iptv::Kind::movies).empty() ||
           !iptv::categories(iptv_state_, id, iptv::Kind::series).empty();
  };
  bool source_changed = false;
  auto account = std::find_if(
      iptv_accounts_.begin(), iptv_accounts_.end(),
      [&](const auto &a) { return a.id == iptv_source_ && valid(a.id); });
  if (account == iptv_accounts_.end()) {
    account = std::find_if(iptv_accounts_.begin(), iptv_accounts_.end(),
                           [&](const auto &a) { return valid(a.id); });
    if (account == iptv_accounts_.end())
      return;
    iptv_source_ = account->id;
    source_changed = true;
    iptv_series_.clear();
    iptv_category_.clear();
    reset = true;
  }
  auto cats = iptv::categories(iptv_state_, iptv_source_, iptv_kind_);
  if (cats.empty()) {
    iptv_kind_ = iptv_kind_ == iptv::Kind::movies ? iptv::Kind::series
                                                  : iptv::Kind::movies;
    cats = iptv::categories(iptv_state_, iptv_source_, iptv_kind_);
    iptv_series_.clear();
    iptv_category_ = cats.empty() ? "" : cats.front().id;
    reset = true;
  }
  if (source_changed && !cats.empty())
    iptv_category_ = cats.front().id;
  if (!iptv_category_.empty() &&
      std::none_of(cats.begin(), cats.end(),
                   [&](const auto &c) { return c.id == iptv_category_; })) {
    iptv_category_ = cats.empty() ? "" : cats.front().id;
    reset = true;
  }
  iptv_content.heading =
      "IPTV Library" +
      (iptv_series_.empty() ? std::string() : " · " + iptv_series_name_);
  iptv_content.notice = iptv_status_.empty()
                            ? "Provider playback only · Movie/TV addon sources "
                              "stay separate · Options: library controls"
                            : iptv_status_;
  if (iptv_loading_)
    iptv_content.notice =
        iptv_catalog_.entries.empty()
            ? "Loading provider catalog… Circle to leave; Options to cancel."
            : "Saved catalog · Refreshing from provider…";
  iptv_content.filters.push_back({"Server", short_label(account->name)});
  iptv_content.filters.push_back(
      {"Type", iptv_kind_ == iptv::Kind::movies ? "Movies" : "Series"});
  std::string group = "All";
  if (!iptv_series_.empty())
    group = iptv_season_ < 0 ? "All seasons"
                             : "Season " + std::to_string(iptv_season_);
  else
    for (const auto &c : cats)
      if (c.id == iptv_category_)
        group = c.name;
  iptv_content.filters.push_back(
      {iptv_series_.empty() ? "Category" : "Season", short_label(group)});
  if (iptv_searching_saved_) {
    iptv_content.heading = "IPTV Library · Saved search";
    iptv_content.notice =
        iptv_loading_
            ? "Searching saved catalogs…"
            : "Saved movies and series across this profile’s providers · " +
                  iptv_status_;
    iptv_content.filters.clear();
    iptv_content.filters.push_back({"Search", iptv_query_});
  }
  if (iptv_series_.empty())
    iptv_content.filters.push_back(
        {"A–Z", iptv_letter_.empty() ? "All" : iptv_letter_});
  std::vector<size_t> matching;
  const auto query = iptv::search_key(iptv_query_);
  for (size_t i = 0; i < iptv_catalog_.entries.size(); ++i) {
    const auto &e = iptv_catalog_.entries[i];
    if (!iptv_series_.empty() && iptv_season_ >= 0 && e.season != iptv_season_)
      continue;
    if (!iptv_searching_saved_ && !query.empty() &&
        iptv::search_key(e.name).find(query) == std::string::npos)
      continue;
    if (iptv_series_.empty() && !iptv_letter_.empty() &&
        iptv::initial(e.name) != iptv_letter_)
      continue;
    matching.push_back(i);
  }
  if (iptv_series_.empty() && !iptv_searching_saved_ && !iptv_letter_.empty())
    std::stable_sort(matching.begin(), matching.end(), [&](size_t a, size_t b) {
      return iptv::search_key(iptv_catalog_.entries[a].name) <
             iptv::search_key(iptv_catalog_.entries[b].name);
    });
  if (reset)
    iptv_page_ = 0;
  iptv_page_ = std::min(
      iptv_page_, matching.empty() ? size_t(0) : (matching.size() - 1) / 100);
  const auto kind = iptv_series_.empty() ? iptv_kind_ : iptv::Kind::episodes;
  const auto &progress = iptv_state_["progress"];
  for (size_t at = iptv_page_ * 100;
       at < matching.size() && at < (iptv_page_ + 1) * 100; ++at) {
    const auto i = matching[at];
    const auto &e = iptv_catalog_.entries[i];
    Title t;
    const auto *hit = iptv_searching_saved_ && i < iptv_hits_.size()
                          ? &iptv_hits_[i]
                          : nullptr;
    t.id = iptv::identity(hit ? hit->source : iptv_source_,
                          hit ? hit->kind : kind, e.id);
    t.type = iptv_kind_ == iptv::Kind::movies ? "iptv-movie" : "iptv-series";
    if (hit)
      t.type = hit->kind == iptv::Kind::movies ? "iptv-movie" : "iptv-series";
    t.name = e.name;
    t.year = e.year;
    t.badge =
        kind == iptv::Kind::episodes
            ? "S" + std::to_string(e.season) + " E" + std::to_string(e.episode)
            : "Provider stream";
    if (hit)
      t.badge = std::string(hit->kind == iptv::Kind::movies ? "Movie · " : "Series · ") + hit->provider;
    if (progress.is_object() && progress.contains(t.id)) {
      const auto &p = progress[t.id];
      double duration = jnum(p, "duration"), time = jnum(p, "time");
      t.watched = jbool(p, "finished");
      if (duration > 0 && !t.watched)
        t.progress = float(std::clamp(time / duration, 0.0, 1.0));
    }
    iptv_content.items.push_back(std::move(t));
    iptv_indices_.push_back(i);
  }
  iptv_content.count =
      std::to_string(matching.size()) + " titles · " +
      std::to_string(iptv_page_ + 1) + "/" +
      std::to_string(std::max(size_t(1), (matching.size() + 99) / 100));
  iptv_content.status =
      iptv_loading_ ? "Loading IPTV library…"
      : iptv_status_.empty()
          ? "No titles match. Use Options to choose a category or refresh."
          : iptv_status_;
  changed_ |= kIptv;
  if (reset)
    changed_ |= kIptvNew;
  art_dirty_ = true;
}
void Session::iptv_reload(bool refresh) {
  if (!iptv_visible || !profile_ready_)
    return;
  iptv_searching_saved_ = false;
  iptv_hits_.clear();
  iptv_letter_.clear();
  auto it = std::find_if(iptv_accounts_.begin(), iptv_accounts_.end(),
                         [&](const auto &a) { return a.id == iptv_source_; });
  if (it == iptv_accounts_.end())
    return;
  if (iptv_cancel_)
    *iptv_cancel_ = true;
  iptv_cancel_ = std::make_shared<std::atomic<bool>>(false);
  const auto cancel = iptv_cancel_;
  const auto account = *it;
  const auto kind = iptv_series_.empty() ? iptv_kind_ : iptv::Kind::episodes;
  const auto category = iptv_series_.empty() ? iptv_category_ : iptv_series_;
  const auto dir = profile_dir_ + "/iptv-cache";
  const auto generation = ++iptv_generation_, profile = profile_generation_;
  iptv_loading_ = true;
  iptv_build();
  struct Result {
    iptv::Catalog catalog;
    bool cached = false;
    bool cache_saved = true;
  };
  g_tasks.run<Result>(
      [account, kind, category, dir, cancel, refresh] {
        Result r;
        if (!refresh &&
            iptv::load_cache(dir, account.id, kind, category, r.catalog)) {
          r.cached = true;
          return r;
        }
        r.catalog = iptv::fetch(account, kind, category, cancel.get());
        if (r.catalog.error.empty())
          r.cache_saved = iptv::save_cache(dir, account.id, kind, category,
                                           r.catalog, cancel.get());
        return r;
      },
      [this, generation, profile, source = account.id, kind, category,
       refresh](Result &r) {
        const auto current_kind =
            iptv_series_.empty() ? iptv_kind_ : iptv::Kind::episodes;
        const auto current_category =
            iptv_series_.empty() ? iptv_category_ : iptv_series_;
        if (generation != iptv_generation_ || profile != profile_generation_ ||
            !iptv_visible || source != iptv_source_ || kind != current_kind ||
            category != current_category)
          return;
        iptv_loading_ = false;
        if (!r.catalog.error.empty()) {
          iptv_status_ = r.catalog.error;
          if (!iptv_catalog_.entries.empty())
            iptv_status_ += " Saved catalog retained; Options to retry.";
          say(iptv_status_, 14.0, true);
        } else {
          iptv_catalog_ = std::move(r.catalog);
          iptv_status_ =
              r.cache_saved
                  ? std::string()
                  : "Catalog loaded; local cache is full or unavailable.";
        }
        iptv_build(!refresh);
        if (r.cached)
          iptv_reload(true);
      });
}
void Session::iptv_filter(int index) {
  if (iptv_searching_saved_) {
    if (index == 0)
      iptv_search();
    else
      iptv_letters();
    return;
  }
  if (index == 3) {
    iptv_letters();
    return;
  }
  const auto profile = profile_generation_;
  const auto generation = iptv_generation_;
  if (index == 0) {
    std::vector<std::string> names, ids;
    for (const auto &a : iptv_accounts_)
      if (!iptv::categories(iptv_state_, a.id, iptv::Kind::movies).empty() ||
          !iptv::categories(iptv_state_, a.id, iptv::Kind::series).empty()) {
        names.push_back(a.name);
        ids.push_back(a.id);
      }
    if (names.empty())
      return;
    open_dropdown("IPTV Library server", names, 0,
                  [this, profile, generation, ids](int i) {
                    if (profile != profile_generation_ ||
                        generation != iptv_generation_ || i < 0 ||
                        size_t(i) >= ids.size())
                      return;
                    iptv_source_ = ids[size_t(i)];
                    iptv_series_.clear();
                    iptv_query_.clear();
                    iptv_catalog_ = {};
                    iptv_category_.clear();
                    iptv_build(true);
                    auto cats =
                        iptv::categories(iptv_state_, iptv_source_, iptv_kind_);
                    if (!cats.empty())
                      iptv_category_ = cats.front().id;
                    iptv_save();
                    iptv_reload();
                  });
    return;
  }
  if (index == 1) {
    std::vector<std::string> labels;
    std::vector<iptv::Kind> kinds;
    for (auto kind : {iptv::Kind::movies, iptv::Kind::series})
      if (!iptv::categories(iptv_state_, iptv_source_, kind).empty()) {
        labels.push_back(kind == iptv::Kind::movies ? "Movies" : "Series");
        kinds.push_back(kind);
      }
    open_dropdown(
        "IPTV content", labels, 0, [this, profile, generation, kinds](int i) {
          if (profile != profile_generation_ ||
              generation != iptv_generation_ || i < 0 ||
              size_t(i) >= kinds.size())
            return;
          iptv_kind_ = kinds[size_t(i)];
          iptv_series_.clear();
          iptv_query_.clear();
          iptv_catalog_ = {};
          auto cats = iptv::categories(iptv_state_, iptv_source_, iptv_kind_);
          iptv_category_ = cats.empty() ? "" : cats.front().id;
          iptv_save();
          iptv_reload();
        });
    return;
  }
  if (index == 2 && !iptv_series_.empty()) {
    std::vector<int> seasons{-1};
    std::vector<std::string> names{"All seasons"};
    std::set<int> seen;
    for (const auto &e : iptv_catalog_.entries)
      if (seen.insert(e.season).second) {
        seasons.push_back(e.season);
        names.push_back("Season " + std::to_string(e.season));
      }
    open_dropdown("IPTV series season", names, 0,
                  [this, profile, generation, seasons](int i) {
                    if (profile != profile_generation_ ||
                        generation != iptv_generation_ || i < 0 ||
                        size_t(i) >= seasons.size())
                      return;
                    iptv_season_ = seasons[size_t(i)];
                    iptv_build(true);
                  });
    return;
  }
  if (index == 2) {
    auto cats = iptv::categories(iptv_state_, iptv_source_, iptv_kind_);
    std::vector<std::string> names{"All categories"}, ids{""};
    for (const auto &c : cats) {
      if (c.id.empty())
        continue;
      names.push_back(c.name);
      ids.push_back(c.id);
    }
    open_dropdown("IPTV category", names, 0,
                  [this, profile, generation, ids](int i) {
                    if (profile != profile_generation_ ||
                        generation != iptv_generation_ || i < 0 ||
                        size_t(i) >= ids.size())
                      return;
                    iptv_category_ = ids[size_t(i)];
                    iptv_catalog_ = {};
                    iptv_query_.clear();
                    iptv_save();
                    iptv_reload();
                  });
  }
}
void Session::iptv_letters() {
  if (!iptv_series_.empty()) {
    say("Browse A–Z from a movie or series catalog; episodes keep their season order.", 6);
    return;
  }
  const auto profile = profile_generation_, generation = iptv_generation_;
  std::set<std::string> available;
  for (const auto &e : iptv_catalog_.entries) {
    if (!iptv_searching_saved_ && !iptv_query_.empty() &&
        iptv::search_key(e.name).find(iptv::search_key(iptv_query_)) ==
            std::string::npos)
      continue;
    available.insert(iptv::initial(e.name));
  }
  std::vector<std::string> labels{"All"};
  labels.insert(labels.end(), available.begin(), available.end());
  int selected = 0;
  for (size_t i = 1; i < labels.size(); ++i)
    if (labels[i] == iptv_letter_)
      selected = int(i);
  open_dropdown("Browse A–Z", labels, selected,
                [this, profile, generation, labels](int i) {
                  if (profile != profile_generation_ ||
                      generation != iptv_generation_ || i < 0 ||
                      size_t(i) >= labels.size())
                    return;
                  iptv_letter_ = i ? labels[size_t(i)] : "";
                  iptv_build(true);
                });
}
void Session::iptv_saved_search(const std::string &query) {
  if (iptv_cancel_)
    *iptv_cancel_ = true;
  const auto generation = ++iptv_generation_, profile = profile_generation_;
  iptv_cancel_ = std::make_shared<std::atomic<bool>>(false);
  auto cancel = iptv_cancel_;
  iptv_query_ = query;
  iptv_letter_.clear();
  iptv_series_.clear();
  iptv_series_name_.clear();
  iptv_series_poster_.clear();
  iptv_season_ = -1;
  iptv_searching_saved_ = true;
  iptv_catalog_ = {};
  iptv_hits_.clear();
  iptv_loading_ = true;
  iptv_status_.clear();
  iptv_build(true);
  g_tasks.run<iptv::SearchResults>(
      [dir = profile_dir_ + "/iptv-cache", accounts = iptv_accounts_, query,
       cancel] {
        return iptv::search_saved(dir, accounts, query, cancel.get());
      },
      [this, generation, profile, cancel](iptv::SearchResults &r) {
        if (profile != profile_generation_ || generation != iptv_generation_ ||
            cancel->load() || !iptv_searching_saved_)
          return;
        iptv_hits_ = std::move(r.hits);
        for (const auto &hit : iptv_hits_)
          iptv_catalog_.entries.push_back(hit.entry);
        iptv_loading_ = false;
        iptv_status_ = r.limited ? "Search limit reached; narrow your search."
                                 : "Search covers saved catalogs only. Open "
                                   "other categories to save more titles.";
        iptv_build(true);
      });
}
void Session::iptv_search() {
  const auto profile = profile_generation_, generation = iptv_generation_;
  ask_text(
      [this, profile, generation](const std::string &text) {
        if (profile != profile_generation_ || generation != iptv_generation_)
          return;
        const auto query = trim(text).substr(0, 128);
        if (query.empty()) {
          iptv_query_.clear();
          iptv_catalog_ = {};
          iptv_reload();
        } else
          iptv_saved_search(query);
      },
      "Search saved IPTV catalogs (empty returns to browsing)");
}
void Session::iptv_menu() {
  const auto profile = profile_generation_;
  open_dropdown("IPTV Library",
                {"Choose server", "Movies / Series", "Category / Season",
                 "Search saved catalogs", "Refresh catalog",
                 "Check library availability", "Previous page", "Next page",
                 "Cancel loading", "Browse A–Z", "Return to browsing"},
                0, [this, profile](int i) {
                  if (profile != profile_generation_)
                    return;
                  if (i <= 2 && i >= 0) {
                    if (iptv_searching_saved_) {
                      if (iptv_cancel_)
                        *iptv_cancel_ = true;
                      ++iptv_generation_;
                      iptv_loading_ = false;
                      iptv_searching_saved_ = false;
                      iptv_hits_.clear();
                      iptv_catalog_ = {};
                      iptv_query_.clear();
                      iptv_letter_.clear();
                      iptv_build(true);
                    }
                    iptv_filter(i);
                  } else if (i == 3)
                    iptv_search();
                  else if (i == 4) {
                    if (iptv_searching_saved_)
                      iptv_saved_search(iptv_query_);
                    else
                      iptv_reload(true);
                  } else if (i == 5)
                    iptv_probe();
                  else if (i == 6 || i == 7) {
                    if (i == 7)
                      ++iptv_page_;
                    else if (iptv_page_)
                      --iptv_page_;
                    iptv_build();
                  } else if (i == 9) {
                    iptv_letters();
                  } else if (i == 10) {
                    iptv_query_.clear();
                    iptv_catalog_ = {};
                    iptv_reload();
                  } else if (i == 8) {
                    if (iptv_cancel_)
                      *iptv_cancel_ = true;
                    if (iptv_probe_cancel_)
                      *iptv_probe_cancel_ = true;
                    ++iptv_generation_;
                    ++iptv_probe_generation_;
                    iptv_loading_ = iptv_probing_ = false;
                    iptv_status_ = "Loading cancelled. Saved catalog retained.";
                    iptv_build();
                  }
                });
  dropdown.message =
      "Movies and episodes play directly from your IPTV subscription. Addon "
      "catalogs, Nuvio history and trackers are separate. Search covers saved "
      "movie and series catalogs across this profile’s providers.";
}
bool Session::iptv_back() {
  if (iptv_searching_saved_) {
    iptv_query_.clear();
    iptv_catalog_ = {};
    iptv_reload();
    return true;
  }
  if (iptv_series_.empty())
    return false;
  if (iptv_cancel_)
    *iptv_cancel_ = true;
  ++iptv_generation_;
  iptv_series_.clear();
  iptv_series_name_.clear();
  iptv_series_poster_.clear();
  iptv_query_.clear();
  iptv_catalog_ = {};
  iptv_season_ = -1;
  iptv_reload();
  return true;
}
void Session::iptv_activate(int index) {
  if (!iptv_visible || watching || resuming_ || index < 0 ||
      size_t(index) >= iptv_indices_.size())
    return;
  const auto at = iptv_indices_[size_t(index)];
  if (at >= iptv_catalog_.entries.size())
    return;
  const auto entry = iptv_catalog_.entries[at];
  if (iptv_searching_saved_) {
    if (at >= iptv_hits_.size())
      return;
    const auto hit = iptv_hits_[at];
    if (std::none_of(iptv_accounts_.begin(), iptv_accounts_.end(),
                     [&](const auto &a) { return a.id == hit.source; }))
      return;
    iptv_source_ = hit.source;
    iptv_kind_ = hit.kind;
    iptv_category_ = entry.category;
    iptv_series_.clear();
    iptv_save();
    if (hit.kind == iptv::Kind::series) {
      iptv_searching_saved_ = false;
      iptv_hits_.clear();
    }
  }
  if (iptv_kind_ == iptv::Kind::series && iptv_series_.empty()) {
    iptv_series_ = entry.id;
    iptv_series_name_ = entry.name;
    iptv_series_poster_ = entry.poster;
    iptv_query_.clear();
    iptv_catalog_ = {};
    iptv_season_ = -1;
    iptv_reload();
    return;
  }
  const auto key = iptv::identity(iptv_source_,
                                  iptv_series_.empty() ? iptv::Kind::movies
                                                       : iptv::Kind::episodes,
                                  entry.id);
  if (iptv_resume(key) > 5) {
    const auto profile = profile_generation_;
    const auto generation = iptv_generation_;
    open_dropdown(entry.name, {"Resume", "Play from beginning", "Cancel"}, 0,
                  [this, profile, generation, entry, key](int i) {
                    if (profile != profile_generation_ ||
                        generation != iptv_generation_ || i < 0 || i > 1)
                      return;
                    if (i == 1) {
                      iptv_state_["progress"].erase(key);
                      iptv_save();
                    }
                    iptv_play(entry);
                  });
  } else
    iptv_play(entry);
}
void Session::iptv_play(const iptv::Entry &entry) {
  auto a = std::find_if(
      iptv_accounts_.begin(), iptv_accounts_.end(),
      [&](const auto &account) { return account.id == iptv_source_; });
  if (a == iptv_accounts_.end())
    return;
  const auto kind =
      iptv_series_.empty() ? iptv::Kind::movies : iptv::Kind::episodes;
  auto url = iptv::playback_url(*a, kind, entry);
  if (url.empty()) {
    say("The IPTV provider supplied an unsupported file format.", 7, true);
    return;
  }
  cancel_autoplay();
  next_preparation_cancel();
  close_detail();
  d_item_ = Item{};
  d_item_.id = iptv::identity(a->id, kind, entry.id);
  d_item_.type = kind == iptv::Kind::movies ? "iptv-movie" : "iptv-series";
  d_item_.name = kind == iptv::Kind::movies
                     ? entry.name
                     : iptv_series_name_ + " · S" +
                           std::to_string(entry.season) + " E" +
                           std::to_string(entry.episode) + " · " + entry.name;
  d_item_.poster = entry.poster.empty() ? iptv_series_poster_ : entry.poster;
  d_meta_ = Meta{};
  d_meta_loaded_ = false;
  d_video_id_ = d_item_.id;
  detail = DetailContent{};
  ::Stream stream;
  stream.url = std::move(url);
  stream.name = d_item_.name;
  stream.addon = "IPTV Library · " + a->name;
  play_stream(stream, false);
}
double Session::iptv_resume(const std::string &id) const {
  const auto &progress = jobj(iptv_state_, "progress");
  if (!progress.is_object() || !progress.contains(id))
    return 0;
  const auto &p = progress[id];
  const double time = jnum(p, "time"), duration = jnum(p, "duration");
  return !jbool(p, "finished") && std::isfinite(time) &&
                 std::isfinite(duration) && time > 0 && duration > 0 &&
                 time < duration * .95
             ? time
             : 0;
}
void Session::iptv_progress(bool ended) {
  if (!profile_ready_ || !iptv::library_type(w_item_.type))
    return;
  const auto duration = player_.duration(), time = player_.position();
  if (!std::isfinite(duration) || !std::isfinite(time) || duration <= 0 ||
      time < 0)
    return;
  auto &progress = iptv_state_["progress"];
  if (!progress.is_object())
    progress = json::object();
  if (progress.size() >= 500 && !progress.contains(w_item_.id)) {
    auto oldest = progress.begin();
    for (auto i = progress.begin(); i != progress.end(); ++i)
      if (jnum(i.value(), "updated") < jnum(oldest.value(), "updated"))
        oldest = i;
    progress.erase(oldest);
  }
  progress[w_item_.id] = {{"time", time},
                          {"duration", duration},
                          {"finished", ended || time >= duration * .95},
                          {"updated", now_epoch_ms()}};
  iptv_save();
  iptv_build();
}
} // namespace sx
