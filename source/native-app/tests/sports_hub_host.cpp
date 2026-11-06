#include "sports_session_boundary.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace sx::sports;
namespace {
int requests = 0;
json response;
json event(int64_t now, const std::string &id = "10",
           const std::string &state = "in") {
  std::tm t{};
  time_t when = now;
  gmtime_r(&when, &t);
  char date[40];
  std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%SZ", &t);
  return {
      {"id", id},
      {"date", date},
      {"name", "Alpha City at Beta Town"},
      {"competitions",
       json::array(
           {{{"status",
              {{"type", {{"state", state}, {"shortDetail", "3rd quarter"}}}}},
             {"broadcasts", json::array({{{"names", json::array({"ESPN"})}}})},
             {"competitors", json::array({{{"team",
                                            {{"id", "1"},
                                             {"displayName", "Alpha City"},
                                             {"abbreviation", "ALP"}}},
                                           {"score", "15"}},
                                          {{"team",
                                            {{"id", "2"},
                                             {"displayName", "Beta Town"},
                                             {"abbreviation", "BET"}}},
                                           {"score", "20"}}})}}})}};
}
json feed(int64_t now) {
  return {{"schemaVersion", 1},
          {"generatedAt", now * 1000},
          {"entries",
           json::array(
               {{{"leagueId", "basketball/nba"},
                 {"provider", "espn"},
                 {"fetchedAt", now * 1000},
                 {"payload", {{"events", json::array({event(now - 60)})}}}}})}};
}
void scene(json &j, const char *name, const sx::Session &s) {
  j[name] = {{"title", s.dropdown.title},
             {"message", s.dropdown.message},
             {"options", s.choices}};
}
} // namespace
namespace live_tv {
HttpResponse download(const std::string &, const std::atomic<bool> *cancel,
                      bool, size_t) {
  ++requests;
  HttpResponse r;
  r.status = cancel && *cancel ? 0 : 200;
  r.body = response.dump();
  return r;
}
} // namespace live_tv
int main(int argc, char **argv) {
  assert(argc >= 3);
  std::string raw;
  assert(read_file(std::string(argv[1]) + "/sports/catalogue.json", raw));
  const auto c = catalogue(raw);
  assert(c.sports.size() == 26 && c.leagues.size() > 150);
  const int64_t now = now_epoch_ms() / 1000;
  const auto *nba = league(c, "basketball/nba");
  assert(nba);
  auto snapshot = parse_feed(feed(now).dump(), c, now);
  assert(snapshot.error.empty() && snapshot.events.size() == 1);
  assert(snapshot.events[0].state == "pre" && snapshot.events[0].score.empty());
  auto live =
      scoreboard({{"events", json::array({event(now - 60)})}}, *nba, now, true);
  assert(live.size() == 1 && live[0].fresh);
  assert(event_status(live[0], false, now).find("ALP 15") != std::string::npos);
  assert(event_status(live[0], true, now).find("15") == std::string::npos);
  assert(event_status(live[0], false, now + 121).find("Live") ==
         std::string::npos);
  assert(!parse_feed(feed(now - 8 * day).dump(), c, now).error.empty());
  assert(!parse_feed(feed(now + 601).dump(), c, now).error.empty());
  assert(!parse_feed(feed(now).dump(), c, now, now + 1).error.empty());
  assert(!parse_feed("{broken", c, now).error.empty());
  assert(
      !parse_feed(std::string(max_feed_bytes + 1, ' '), c, now).error.empty());
  Preferences p;
  p.enabled = true;
  p.teams.push_back(live[0].teams.front());
  assert(follows(live[0], p));
  assert(team_key({"soccer/eng.1", "9", "Same", ""}) ==
         team_key({"soccer/uefa.champions", "9", "Same", ""}));
  assert(team_key({"football/nfl", "9", "Same", ""}) !=
         team_key({"basketball/nba", "9", "Same", ""}));
  assert(visible(snapshot, c, p, 1, now).size() == 1);
  p.excluded_sports.insert("basketball");
  assert(visible(snapshot, c, p, 0, now).empty());
  p.excluded_sports.clear();
  p.words = {"Alpha"};
  p.teams.clear();
  assert(follows(live[0], p));
  p.words = {"Alp"};
  assert(!follows(live[0], p));
  json saved = {{"unrelated", 42}};
  save(saved, p);
  const auto loaded = load(saved);
  assert(loaded.enabled && loaded.words == p.words && saved["unrelated"] == 42);
  auto bad = load({{"sports_hub",
                    {{"region", "url?token=secret"},
                     {"utc_offset", 1e100},
                     {"teams", json::array({{{"league", "../../bad"},
                                             {"id", "x"},
                                             {"name", "bad"}}})}}}});
  assert(bad.region == "ALL" && bad.utc_offset == 0 && bad.teams.empty());
  assert(!safe_id("../secrets") &&
         espn_url({"", "", "", "../private", {}, {}}, true).empty());
  std::vector<live_tv::Channel> channels(3);
  channels[0].name = "US ESPN HD";
  channels[1].name = "ESPN2";
  channels[2].name = "ESPN+";
  auto candidates = matches(live[0], channels, {});
  assert(candidates.size() == 1 && candidates[0].channel == 0 &&
         candidates[0].rank == 100);
  std::vector<Guide> guide = {
      {1, {now - 60, now + 3600, "NBA: Alpha City at Beta Town"}}};
  candidates = matches(live[0], channels, guide);
  assert(candidates[0].channel == 1 && candidates[0].rank == 300);
  guide[0].programme.title = "NBA: Alpha City at Somebody Else";
  candidates = matches(live[0], channels, guide);
  assert(candidates[0].rank == 100);
  guide[0].programme.title = "NBA: Alpha City at Beta Town";
  auto local = guide_events(c, guide, now);
  assert(local.size() == 1 && local[0].provider == "Guide");
  guide[0].programme.title = "World Cup";
  assert(guide_events(c, guide, now).empty());
  json dir = {
      {"sports",
       json::array(
           {{{"leagues",
              json::array(
                  {{{"teams", json::array({{{"team",
                                             {{"id", "1"},
                                              {"displayName",
                                               "Alpha City"}}}}})}}})}}})}};
  assert(teams(dir, *nba).size() == 1);
  sx::Session s;
  s.base_dir_ = argv[1];
  s.data_dir_ = std::string(argv[2]) + "/data";
  make_dirs(s.data_dir_);
  s.sports_load({});
  assert(!s.sports_visible && s.jobs.empty());
  s.sports_reload();
  assert(s.jobs.empty() && requests == 0);
  s.sports_toggle();
  assert(s.sports_visible && s.saves == 1);
  s.sports_reload();
  assert(s.jobs.size() == 1);
  auto cancelled = s.sports_cancel_;
  s.sports_toggle();
  assert(*cancelled && !s.sports_visible);
  response = feed(now);
  s.finish();
  assert(s.sports_snapshot_.generated == 0 && s.sports_content.items.empty());
  s.sports_last_feed_ = -1000;
  s.sports_toggle();
  s.sports_next_feed_ = 0;
  s.sports_reload();
  s.finish();
  assert(s.sports_snapshot_.events.size() == 1);
  s.sports_menu();
  json scenes;
  scene(scenes, "sports-menu", s);
  s.pick(3);
  assert(s.dropdown.selected == 3);
  s.sports_browse();
  scene(scenes, "sports-picker", s);
  const auto before = s.sports_prefs_.excluded_sports.size();
  s.pick(0);
  s.pick(0);
  assert(s.sports_prefs_.excluded_sports.size() == before + 1);
  s.sports_load(saved);
  assert(s.sports_prefs_.words == p.words);
  s.sports_prefs_.excluded_sports.clear();
  s.sports_snapshot_ = snapshot;
  s.sports_build();
  assert(s.sports_content.items.size() >= 4);
  scenes["sports-hub"] = json::array();
  for (const auto &row : s.sports_content.items)
    scenes["sports-hub"].push_back({{"name", row.name},
                                    {"version", row.version},
                                    {"description", row.description},
                                    {"types", row.types},
                                    {"initial", row.initial}});
  s.sports_region();
  auto stale = s.choice;
  ++s.profile_generation_;
  stale(1);
  assert(s.sports_prefs_.region == "ALL");
  s.sports_region();
  stale = s.choice;
  s.sports_toggle();
  stale(1);
  assert(s.sports_prefs_.region == "ALL" && !s.sports_visible);
  s.sports_toggle();
  s.sports_teams_[nba->id] = teams(dir, *nba);
  s.sports_team_expiry_[nba->id] = now_seconds() + 100;
  s.sports_teams_[nba->id].push_back({nba->id, "2", "Beta Town", "BET"});
  s.sports_teams(nba->id, "Alpha");
  scene(scenes, "sports-teams", s);
  s.pick(2);
  assert(!s.sports_prefs_.teams.empty());
  assert(s.dropdown.selected == 2 && s.choices.size() == 3);
  s.live_channels_ = channels;
  s.sports_event(live[0]);
  scene(scenes, "sports-channels", s);
  stale = s.choice;
  ++s.live_generation_;
  stale(0);
  assert(s.plays == 0);
  s.sports_event(live[0]);
  s.pick(0);
  assert(s.plays == 1 && s.played.name == "US ESPN HD");
  const auto oldrequests = requests;
  s.sports_toggle();
  s.sports_tick(3);
  assert(requests == oldrequests && s.jobs.empty());
  assert(norm("São Paulo") == norm("Sao Paulo"));
  assert(json({{"name", clean("日本語", 4)}}).dump().find("日") !=
         std::string::npos);
  assert(clean("日本語", 2).empty());
  auto dated = event(now);
  dated["date"] = "2026-02-30T12:00:00Z";
  assert(
      scoreboard({{"events", json::array({dated})}}, *nba, now, true).empty());
  dated["date"] = "2026-11-06T01:30:00+05:30";
  const auto offset_event =
      scoreboard({{"events", json::array({dated})}}, *nba, now, true);
  assert(offset_event.size() == 1 &&
         clock(offset_event[0].start).find("20:00") != std::string::npos);
  auto dates = feed(now);
  dates["entries"][0]["leagueId"] = "guide/darts";
  dates["entries"][0]["provider"] = "calendar";
  dates["entries"][0]["payload"]["events"] =
      json::array({{{"fixture", "Darts tournament"},
                    {"date_utc", iso8601_now().substr(0, 10)},
                    {"time_utc", ""}}});
  const auto calendar = parse_feed(dates.dump(), c, now);
  assert(calendar.events.size() == 1 && !calendar.events[0].has_time &&
         event_time(calendar.events[0]).find("time TBD") != std::string::npos);
  guide[0].programme.title = "Luke Littler exhibition";
  assert(guide_events(c, guide, now, {"Luke Littler"}).size() == 1);
  s.sports_load(saved);
  assert(s.sports_prefs_.words == p.words && s.sports_prefs_.teams.empty());
  const auto settings_scene = [&](const char *key) {
    s.sports_settings_snapshot();
    scenes[key] = json::array();
    for (const auto &row : s.settings.rows)
      scenes[key].push_back({{"label", row.label},
                             {"value", row.value},
                             {"help", row.help},
                             {"category", row.category},
                             {"kind", int(row.kind)},
                             {"on", row.on}});
  };
  settings_scene("sports-settings");
  s.sports_toggle();
  settings_scene("sports-settings-off");
  const auto indexed = index_channels(channels);
  assert(matches(live[0], channels, {}, "ALL", &indexed).size() == 1);
  if (argc > 3) {
    std::string real;
    assert(read_file(argv[3], real));
    const auto actual = parse_feed(real, c, now);
    assert(actual.error.empty() && !actual.events.empty());
    std::cout << "Public ReSeq feed parsed: " << actual.events.size()
              << " bounded events\n";
  }
  if (argc > 4) {
    std::string team_raw;
    assert(read_file(argv[4], team_raw));
    const auto directory = teams(json::parse(team_raw), *nba);
    assert(!directory.empty());
    std::cout << "Public NBA team directory parsed: " << directory.size()
              << " teams\n";
  }
  std::ofstream(std::string(argv[2]) + "/scenes.json") << scenes.dump(2);
  std::cout << "Sports Hub: catalogue, bounded parsing, stale scores, follows, "
               "filtering, channel confidence, cancellation and production "
               "selection flows passed\n";
}
