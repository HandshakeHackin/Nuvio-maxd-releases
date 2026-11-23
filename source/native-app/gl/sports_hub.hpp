// Optional, profile-local sports browsing; public metadata never contains
// playlist credentials. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "live_guide.h"
#include "live_tv.h"
#include "util.h"
#include <map>
#include <set>
namespace sx::sports {
constexpr size_t max_feed_bytes = 16u << 20;
constexpr int64_t day = 86400, max_age = 7 * day;
inline constexpr const char *feed_url =
    "https://abbaskip.github.io/Nuvio-ReSeq/sports-base.json";
struct Sport {
  std::string id, name;
};
struct League {
  std::string id, sport, name, path;
  std::vector<std::string> words, regions;
};
struct Team {
  std::string league, id, name, abbr;
};
struct Event {
  std::string id, league, title, state = "pre", status, score, provider;
  int64_t start = 0, end = 0, updated = 0;
  std::vector<Team> teams;
  std::vector<std::string> broadcasts;
  bool fresh = false, has_time = true;
};
struct Catalogue {
  std::vector<Sport> sports;
  std::vector<League> leagues;
};
struct Snapshot {
  int64_t generated = 0;
  std::vector<Event> events;
  std::string error;
};
struct Preferences {
  bool enabled = false, hide_scores = false, only_channels = false;
  std::string region = "ALL";
  int utc_offset = 0;
  std::set<std::string> excluded_sports, excluded_leagues, followed_leagues;
  std::vector<Team> teams;
  std::vector<std::string> words;
};
std::string clean(const std::string &, size_t max = 160);
std::string norm(const std::string &);
bool phrase(const std::string &, const std::string &);
bool safe_id(const std::string &);
Preferences load(const json &);
void save(json &, const Preferences &);
Catalogue catalogue(const std::string &);
const League *league(const Catalogue &, const std::string &);
bool included(const League &, const Preferences &);
bool follows(const Event &, const Preferences &);
std::string team_key(const Team &);
Snapshot parse_feed(const std::string &, const Catalogue &, int64_t now,
                    int64_t previous = 0);
std::vector<Event> scoreboard(const json &, const League &, int64_t fetched,
                              bool fresh, const std::string &region = "ALL");
std::vector<Team> teams(const json &, const League &);
std::string espn_url(const League &, bool teams = false, int64_t date = 0);
std::string clock(int64_t epoch, int offset = 0);
std::string event_time(const Event &, int offset = 0);
std::string event_status(const Event &, bool hide_scores, int64_t now);
struct Guide {
  size_t channel = 0;
  live_guide::Programme programme;
  std::string normalized{};
};
struct Match {
  size_t channel = 0;
  int rank = 0;
  std::string reason;
};
struct ChannelIndex {
  std::map<std::string, std::vector<size_t>> networks;
};
ChannelIndex index_channels(const std::vector<live_tv::Channel> &);
std::vector<Match> matches(const Event &, const std::vector<live_tv::Channel> &,
                           const std::vector<Guide> &,
                           const std::string &region = "ALL",
                           const ChannelIndex *index = nullptr);
std::vector<Event> guide_events(const Catalogue &, const std::vector<Guide> &,
                                int64_t now,
                                const std::vector<std::string> &names = {});
std::vector<Event> visible(const Snapshot &, const Catalogue &,
                           const Preferences &, int view, int64_t now,
                           const std::string &query = "");
} // namespace sx::sports
