#include "session.hpp"
#include <algorithm>
namespace sx {
void Session::sports_event(const sports::Event &original) {
  if (!sports_visible || watching || resuming_)
    return;
  const auto event = original;
  const auto p = profile_generation_, g = sports_generation_,
             d = ++sports_dialog_;
  const auto live = live_generation_;
  const auto source = live_source_;
  const auto candidates = sports::matches(event, live_channels_, sports_guide(),
                                          sports_prefs_.region);
  std::vector<std::string> labels;
  for (const auto &m : candidates)
    labels.push_back(live_channels_[m.channel].name + " · " +
                     (m.rank >= 300 ? "guide match" : "broadcaster candidate"));
  labels.push_back("Search all my Live TV channels");
  labels.push_back("Follow / unfollow competition");
  for (const auto &team : event.teams)
    labels.push_back("Follow / unfollow " + team.name);
  const auto count = candidates.size();
  open_dropdown(event.title, labels, 0,
                [this, p, g, d, live, source, event, candidates, count](int i) {
                  if (!sports_valid(p, g, d) || live != live_generation_ ||
                      source != live_source_ || i < 0)
                    return;
                  if (size_t(i) < count) {
                    const auto at = candidates[size_t(i)].channel;
                    if (at < live_channels_.size())
                      live_play(live_channels_[at]);
                    return;
                  }
                  const auto action = size_t(i) - count;
                  if (action == 0) {
                    sports_channel_search(event);
                    return;
                  }
                  if (action == 1) {
                    const auto *l =
                        sports::league(sports_catalogue_, event.league);
                    if (!sports_prefs_.followed_leagues.erase(event.league)) {
                      sports_prefs_.followed_leagues.insert(event.league);
                      sports_prefs_.excluded_leagues.erase(event.league);
                      if (l)
                        sports_prefs_.excluded_sports.erase(l->sport);
                    }
                    sports_save();
                    sports_event(event);
                    return;
                  }
                  if (action - 2 < event.teams.size()) {
                    const auto team = event.teams[action - 2];
                    auto &teams = sports_prefs_.teams;
                    auto it = std::find_if(
                        teams.begin(), teams.end(), [&](const auto &t) {
                          return sports::team_key(t) == sports::team_key(team);
                        });
                    if (it != teams.end())
                      teams.erase(it);
                    else if (teams.size() < 128 && !team.id.empty())
                      teams.push_back(team);
                    sports_save();
                    sports_event(event);
                  }
                });
  dropdown.message =
      sports::event_time(event, sports_prefs_.utc_offset) + " · " +
      sports::event_status(event, sports_prefs_.hide_scores,
                           now_epoch_ms() / 1000) +
      ". Broadcaster matches do not confirm this programme. Choose a channel "
      "to watch; availability depends on your provider.";
}
void Session::sports_channel_search(const sports::Event &event) {
  if (!sports_visible)
    return;
  const auto p = profile_generation_, g = sports_generation_,
             d = ++sports_dialog_, live = live_generation_;
  const auto source = live_source_;
  if (live_channels_.empty()) {
    say("Configure and load your own source in Live TV first. Sports metadata "
        "does not include playable streams.",
        10);
    return;
  }
  ask_text(
      [this, p, g, d, live, source, event](const std::string &raw) {
        if (!sports_valid(p, g, d) || live != live_generation_ ||
            source != live_source_)
          return;
        const auto query = sports::clean(raw, 80);
        if (query.size() < 2) {
          say("Enter two or more characters to search channels.", 6);
          return;
        }
        std::vector<size_t> ids;
        std::vector<std::string> names;
        for (size_t i = 0; i < live_channels_.size() && ids.size() < 200; ++i)
          if (sports::norm(live_channels_[i].name)
                  .find(trim(sports::norm(query))) != std::string::npos) {
            ids.push_back(i);
            names.push_back(live_channels_[i].name);
          }
        if (ids.empty()) {
          say("No channels matched that search.", 6);
          return;
        }
        open_dropdown(
            "Choose a channel · " + event.title, names, 0,
            [this, p, g, d, live, source, ids](int i) {
              if (!sports_valid(p, g, d) || live != live_generation_ ||
                  source != live_source_ || i < 0 || size_t(i) >= ids.size())
                return;
              live_play(live_channels_[ids[size_t(i)]]);
            });
        dropdown.message = "Manual channel selection. The programme is not "
                           "confirmed; consult your provider guide.";
      },
      "Search loaded Live TV channels");
}
} // namespace sx
