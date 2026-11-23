#include "../ui/i18n.hpp"
#include "session.hpp"
#include <algorithm>
namespace sx {
namespace {
void flip(std::set<std::string> &values, const std::string &id) {
  if (!values.erase(id))
    values.insert(id);
}
} // namespace
void Session::sports_menu(int selected) {
  if (!sports_visible || !profile_ready_)
    return;
  const auto p = profile_generation_, g = sports_generation_,
             d = ++sports_dialog_;
  open_dropdown(
      sx::i18n::tr("Sports Hub"),
      {sx::i18n::tr("Choose sports and competitions"), sx::i18n::tr("Follow teams, leagues and names"),
       sx::i18n::tr("Coverage region"),
       sx::i18n::tr("Hide scores: ") + std::string(sports_prefs_.hide_scores ? "on" : "off"),
       sx::i18n::tr("Only channel matches: ") +
           std::string(sports_prefs_.only_channels ? "on" : "off"),
       sx::i18n::tr("Time display / UTC offset"), sx::i18n::tr("Open My Sports"), sx::i18n::tr("Public data sources")},
      selected, [this, p, g, d](int i) {
        if (!sports_valid(p, g, d))
          return;
        if (i == 0)
          sports_browse();
        else if (i == 1)
          sports_following();
        else if (i == 2)
          sports_region();
        else if (i == 3) {
          sports_scores();
          sports_menu(i);
        } else if (i == 4) {
          sports_channel_filter();
          sports_menu(i);
        } else if (i == 5)
          sports_clock();
        else if (i == 6) {
          sports_view_ = 1;
          sports_page_ = 0;
          sports_build();
          goto_ = Page::sports;
        } else if (i == 7)
          say(sx::i18n::tr("Public schedule: Nuvio-ReSeq, ESPN, TheSportsDB and sporting-events.org. Coverage varies. Playlists and credentials stay on this console. Public APIs receive the requested competition. Your Live TV sources supply playback."),
              15);
      });
}
void Session::sports_region() {
  if (!sports_visible)
    return;
  const auto p = profile_generation_, g = sports_generation_,
             d = ++sports_dialog_;
  const std::vector<std::string> labels = {
      sx::i18n::tr("All regions"), sx::i18n::tr("United States"), sx::i18n::tr("United Kingdom"), sx::i18n::tr("Ireland"),
      sx::i18n::tr("Canada"),      sx::i18n::tr("Australia"),     sx::i18n::tr("Other")};
  const char *codes[] = {"ALL", "US", "UK", "IE", "CA", "AU", "OTHER"};
  int selected = 0;
  for (int i = 0; i < 7; ++i)
    if (sports_prefs_.region == codes[i])
      selected = i;
  open_dropdown(
      sx::i18n::tr("Sports coverage region"), labels, selected, [this, p, g, d](int i) {
        if (!sports_valid(p, g, d) || i < 0 || i > 6)
          return;
        const char *values[] = {"ALL", "US", "UK", "IE", "CA", "AU", "OTHER"};
        sports_prefs_.region = values[i];
        sports_save();
        say(sx::i18n::tr("Coverage region saved. Times use the separate UTC offset setting."),
            7);
      });
}
void Session::sports_clock() {
  if (!sports_visible)
    return;
  const auto p = profile_generation_, g = sports_generation_,
             d = ++sports_dialog_;
  std::vector<std::string> labels;
  for (int n = -720; n <= 840; n += 15) {
    char b[32];
    std::snprintf(b, sizeof(b), "UTC %c%02d:%02d", n < 0 ? '-' : '+',
                  std::abs(n) / 60, std::abs(n) % 60);
    labels.emplace_back(b);
  }
  open_dropdown(sx::i18n::tr("Sports time display"), labels,
                (sports_prefs_.utc_offset + 720) / 15, [this, p, g, d](int i) {
                  if (!sports_valid(p, g, d) || i < 0 || i > 104)
                    return;
                  sports_prefs_.utc_offset = -720 + i * 15;
                  sports_save();
                });
  dropdown.message = sx::i18n::tr("Fixed UTC offset. Update this setting when daylight-saving time changes.");
}
void Session::sports_browse(const std::string &sport) {
  if (!sports_visible)
    return;
  const auto p = profile_generation_, g = sports_generation_,
             d = ++sports_dialog_;
  if (sport.empty()) {
    std::vector<std::string> rows;
    for (const auto &s : sports_catalogue_.sports)
      rows.push_back(std::string(sports_prefs_.excluded_sports.count(s.id)
                                     ? sx::i18n::tr("Off · ")
                                     : "On · ") +
                     s.name + " >");
    open_dropdown(sx::i18n::tr("Choose sports"), rows, 0, [this, p, g, d](int i) {
      if (!sports_valid(p, g, d) || i < 0 ||
          size_t(i) >= sports_catalogue_.sports.size())
        return;
      sports_browse(sports_catalogue_.sports[size_t(i)].id);
    });
    dropdown.message = sx::i18n::tr("Open a sport to include/exclude it and select its competitions. All are included by default.");
    return;
  }
  std::vector<std::string> rows = {
      std::string(sx::i18n::tr("Include this sport: ")) +
          (sports_prefs_.excluded_sports.count(sport) ? "off" : "on"),
      sx::i18n::tr("Back to sports")};
  std::vector<std::string> ids;
  for (const auto &l : sports_catalogue_.leagues)
    if (l.sport == sport)
      ids.push_back(l.id);
  std::stable_sort(
      ids.begin(), ids.end(), [this](const auto &a, const auto &b) {
        const auto *x = sports::league(sports_catalogue_, a);
        const auto *y = sports::league(sports_catalogue_, b);
        const auto preferred = [this](const auto *l) {
          return std::find(l->regions.begin(), l->regions.end(),
                           sports_prefs_.region) != l->regions.end();
        };
        return preferred(x) != preferred(y) ? preferred(x) : x->name < y->name;
      });
  for (const auto &id : ids) {
    const auto *l = sports::league(sports_catalogue_, id);
    rows.push_back(std::string(sports_prefs_.excluded_leagues.count(id)
                                   ? sx::i18n::tr("Off · ")
                                   : "On · ") +
                   l->name + " >");
  }
  std::string title = sx::i18n::tr("Competitions");
  for (const auto &s : sports_catalogue_.sports)
    if (s.id == sport)
      title = s.name;
  open_dropdown(title, rows, 0, [this, p, g, d, sport, ids](int i) {
    if (!sports_valid(p, g, d) || i < 0)
      return;
    if (i == 0) {
      flip(sports_prefs_.excluded_sports, sport);
      sports_save();
      sports_browse(sport);
    } else if (i == 1)
      sports_browse();
    else if (size_t(i - 2) < ids.size())
      sports_league(ids[size_t(i - 2)]);
  });
  if (sports_prefs_.excluded_sports.count(sport))
    dropdown.message =
        sx::i18n::tr("This sport is off. Its competition and team selections are retained.");
}
void Session::sports_league(const std::string &id) {
  if (!sports_visible)
    return;
  const auto *found = sports::league(sports_catalogue_, id);
  if (!found)
    return;
  const auto league = *found;
  const auto p = profile_generation_, g = sports_generation_,
             d = ++sports_dialog_;
  open_dropdown(league.name,
                {std::string(sx::i18n::tr("Include competition: ")) +
                     (sports_prefs_.excluded_leagues.count(id) ? "off" : "on"),
                 std::string(sx::i18n::tr("Follow whole competition: ")) +
                     (sports_prefs_.followed_leagues.count(id) ? "on" : "off"),
                 sx::i18n::tr("Choose teams"), sx::i18n::tr("Back to sport")},
                0, [this, p, g, d, league](int i) {
                  if (!sports_valid(p, g, d))
                    return;
                  if (i == 0) {
                    flip(sports_prefs_.excluded_leagues, league.id);
                    sports_save();
                    sports_league(league.id);
                  } else if (i == 1) {
                    flip(sports_prefs_.followed_leagues, league.id);
                    if (sports_prefs_.followed_leagues.count(league.id)) {
                      sports_prefs_.excluded_sports.erase(league.sport);
                      sports_prefs_.excluded_leagues.erase(league.id);
                    }
                    sports_save();
                    sports_league(league.id);
                  } else if (i == 2)
                    sports_teams(league.id);
                  else if (i == 3)
                    sports_browse(league.sport);
                });
}
void Session::sports_teams(const std::string &id, const std::string &query,
                           int selected) {
  if (!sports_visible)
    return;
  const auto *found = sports::league(sports_catalogue_, id);
  if (!found)
    return;
  const auto league = *found;
  const auto p = profile_generation_, g = sports_generation_,
             d = ++sports_dialog_;
  if (league.path.empty()) {
    say(sx::i18n::tr("This competition has no supported team directory. Follow the whole competition or add a team/athlete name in My Sports."),
        10);
    return;
  }
  auto cached = sports_teams_.find(id);
  if (cached == sports_teams_.end() &&
      now_seconds() < sports_team_expiry_[id]) {
    say(sx::i18n::tr("Please wait a minute before retrying this team directory."), 6);
    return;
  }
  if (cached == sports_teams_.end() ||
      now_seconds() >= sports_team_expiry_[id]) {
    if (sports_team_loading_) {
      say(sx::i18n::tr("A team list is loading. Try again in a moment."), 5);
      return;
    }
    sports_team_loading_ = true;
    sports_team_expiry_[id] = now_seconds() + 60;
    const auto cancel = sports_cancel_;
    say(sx::i18n::tr("Loading teams for ") + league.name + "...", 5);
    bg<std::vector<sports::Team>>(
        [league, cancel] {
          const auto r = live_tv::download(sports::espn_url(league, true),
                                           cancel.get(), true, 8u << 20);
          return r.ok() ? sports::teams(json::parse(r.body, nullptr, false),
                                        league)
                        : std::vector<sports::Team>{};
        },
        [this, p, g, d, id, query, selected, cancel](auto &rows) {
          if (g != sports_generation_ || *cancel)
            return;
          sports_team_loading_ = false;
          if (!sports_valid(p, g, d))
            return;
          if (rows.empty()) {
            say(sx::i18n::tr("No team directory returned. Try again later, or follow a team by name."),
                9, true);
            return;
          }
          sports_teams_[id] = std::move(rows);
          sports_team_expiry_[id] = now_seconds() + 86400;
          sports_teams(id, query, selected);
        });
    return;
  }
  std::vector<sports::Team> rows;
  for (const auto &team : cached->second)
    if (query.empty() || sports::norm(team.name).find(
                             trim(sports::norm(query))) != std::string::npos)
      rows.push_back(team);
  std::vector<std::string> labels = {sx::i18n::tr("Search teams"), sx::i18n::tr("Back to competition")};
  for (const auto &team : rows) {
    const bool following =
        std::any_of(sports_prefs_.teams.begin(), sports_prefs_.teams.end(),
                    [&](const auto &t) {
                      return sports::team_key(t) == sports::team_key(team);
                    });
    labels.push_back(std::string(following ? sx::i18n::tr("Following · ") : sx::i18n::tr("Follow · ")) +
                     team.name);
  }
  open_dropdown(league.name + " · teams", labels,
                std::clamp(selected, 0, int(labels.size()) - 1),
                [this, p, g, d, league, rows, query](int i) {
                  if (!sports_valid(p, g, d) || i < 0)
                    return;
                  if (i == 0) {
                    ask_text(
                        [this, p, g, d, league](const std::string &q) {
                          if (sports_valid(p, g, d))
                            sports_teams(league.id, sports::clean(q, 80));
                        },
                        sx::i18n::tr("Find a team (empty shows all)"));
                    return;
                  }
                  if (i == 1) {
                    sports_league(league.id);
                    return;
                  }
                  if (size_t(i - 2) >= rows.size())
                    return;
                  const auto &team = rows[size_t(i - 2)];
                  auto &teams = sports_prefs_.teams;
                  const auto at = std::find_if(
                      teams.begin(), teams.end(), [&](const auto &t) {
                        return sports::team_key(t) == sports::team_key(team);
                      });
                  if (at != teams.end())
                    teams.erase(at);
                  else if (teams.size() < 128) {
                    teams.push_back(team);
                    sports_prefs_.excluded_sports.erase(league.sport);
                    sports_prefs_.excluded_leagues.erase(league.id);
                  } else {
                    say(sx::i18n::tr("You can follow up to 128 teams."), 6, true);
                    return;
                  }
                  sports_save();
                  sports_teams(league.id, query, i);
                });
  dropdown.message =
      sx::i18n::tr("X follows or unfollows. Soccer teams follow across supported competitions; other teams stay scoped to their league.");
}
void Session::sports_following() {
  if (!sports_visible)
    return;
  const auto p = profile_generation_, g = sports_generation_,
             d = ++sports_dialog_;
  std::vector<std::string> labels = {
      sx::i18n::tr("Choose teams or leagues"), sx::i18n::tr("Add a team / athlete name"), sx::i18n::tr("Open My Sports")};
  struct Follow {
    int type;
    std::string id;
  };
  std::vector<Follow> values;
  for (const auto &t : sports_prefs_.teams) {
    labels.push_back(sx::i18n::tr("Unfollow · ") + t.name);
    values.push_back({0, sports::team_key(t)});
  }
  for (const auto &id : sports_prefs_.followed_leagues) {
    const auto *l = sports::league(sports_catalogue_, id);
    labels.push_back(sx::i18n::tr("Unfollow · ") + (l ? l->name : id));
    values.push_back({1, id});
  }
  for (const auto &word : sports_prefs_.words) {
    labels.push_back(sx::i18n::tr("Remove name · ") + word);
    values.push_back({2, word});
  }
  open_dropdown(
      sx::i18n::tr("My Sports selections"), labels, 0, [this, p, g, d, values](int i) {
        if (!sports_valid(p, g, d) || i < 0)
          return;
        if (i == 0) {
          sports_browse();
          return;
        }
        if (i == 1) {
          ask_text(
              [this, p, g, d](const std::string &raw) {
                if (!sports_valid(p, g, d))
                  return;
                const auto word = sports::clean(raw, 80);
                if (sports::norm(word).size() < 5) {
                  say(sx::i18n::tr("Enter at least three characters for a name."), 6);
                  return;
                }
                if (sports_prefs_.words.size() >= 64) {
                  say(sx::i18n::tr("You can follow up to 64 names."), 6, true);
                  return;
                }
                if (std::none_of(sports_prefs_.words.begin(),
                                 sports_prefs_.words.end(), [&](const auto &x) {
                                   return sports::norm(x) == sports::norm(word);
                                 }))
                  sports_prefs_.words.push_back(word);
                sports_save();
                sports_following();
              },
              sx::i18n::tr("Team or athlete name"));
          return;
        }
        if (i == 2) {
          sports_view_ = 1;
          sports_page_ = 0;
          sports_build();
          goto_ = Page::sports;
          return;
        }
        if (size_t(i - 3) >= values.size())
          return;
        const auto value = values[size_t(i - 3)];
        if (value.type == 0) {
          auto &teams = sports_prefs_.teams;
          teams.erase(std::remove_if(teams.begin(), teams.end(),
                                     [&](const auto &t) {
                                       return sports::team_key(t) == value.id;
                                     }),
                      teams.end());
        } else if (value.type == 1)
          sports_prefs_.followed_leagues.erase(value.id);
        else {
          auto &words = sports_prefs_.words;
          words.erase(std::remove(words.begin(), words.end(), value.id),
                      words.end());
        }
        sports_save();
        sports_following();
      });
  dropdown.message =
      sx::i18n::tr("Selections belong to this profile. Names match whole words in event and guide titles. Turned-off sports retain their follows.");
}
} // namespace sx
