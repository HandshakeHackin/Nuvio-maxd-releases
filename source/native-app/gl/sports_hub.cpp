#include "../ui/i18n.hpp"
#include "sports_hub.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
namespace sx::sports {
namespace {
const json &arr(const json &j, const char *key) {
  static const json empty = json::array();
  const auto &a = jobj(j, key);
  return a.is_array() ? a : empty;
}
std::vector<std::string> strings(const json &a, size_t limit = 64) {
  std::vector<std::string> out;
  if (a.is_array())
    for (const auto &v : a)
      if (v.is_string() && out.size() < limit) {
        auto s = clean(v.get<std::string>());
        if (!s.empty())
          out.push_back(s);
      }
  return out;
}
int64_t stamp(const std::string &value) {
  if (value.size() < 10)
    return 0;
  int y = 0, m = 0, d = 0, h = 0, n = 0, s = 0, used = 0;
  const int fields = std::sscanf(value.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%n", &y,
                                 &m, &d, &h, &n, &s, &used);
  if (fields == 5)
    std::sscanf(value.c_str(), "%4d-%2d-%2dT%2d:%2d%n", &y, &m, &d, &h, &n,
                &used);
  else if (fields == 3 && value.size() == 10)
    used = 10;
  if (fields < 3 || !used || y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 ||
      d > 31 || h < 0 || h > 23 || n < 0 || n > 59 || s < 0 || s > 59)
    return 0;
  const auto suffix = value.substr(size_t(used));
  int offset = 0;
  if (!suffix.empty() && suffix != "Z" && suffix != "z") {
    int th = 0, tm = 0, read = 0;
    if ((suffix[0] != '+' && suffix[0] != '-') ||
        std::sscanf(suffix.c_str() + 1, "%2d:%2d%n", &th, &tm, &read) != 2 ||
        read != 5 || suffix.size() != 6 || th > 14 || tm > 59)
      return 0;
    offset = (th * 60 + tm) * 60 * (suffix[0] == '+' ? 1 : -1);
  }
  std::tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = n;
  t.tm_sec = s;
  const auto result = timegm(&t);
  if (t.tm_year != y - 1900 || t.tm_mon != m - 1 || t.tm_mday != d ||
      t.tm_hour != h || t.tm_min != n)
    return 0;
  return result - offset;
}

Team side(const json &j, const League &l) {
  return {l.id, clean(jstr(j, "id"), 64),
          clean(jstr(j, "displayName", jstr(j, "fullName", jstr(j, "name")))),
          clean(jstr(j, "abbreviation"), 20)};
}
std::string family(const std::string &l) {
  return starts_with(l, "soccer/") ? "soccer" : l;
}
bool same_team(const Team &a, const Team &b) {
  return family(a.league) == family(b.league) && a.id == b.id && !a.id.empty();
}
std::set<std::string> ids(const json &j, const char *k) {
  std::set<std::string> out;
  for (const auto &s : strings(jobj(j, k), 512))
    if (safe_id(s))
      out.insert(s);
  return out;
}
} // namespace
std::string clean(const std::string &s, size_t max) {
  std::string out;
  for (size_t i = 0; i < s.size();) {
    const auto c = static_cast<unsigned char>(s[i]);
    if (c < 128) {
      ++i;
      if (c < 32 && c != '\n' && c != '\r' && c != '\t')
        continue;
      if (c == 127)
        continue;
      if (out.size() == max)
        break;
      out += c < 32 ? ' ' : char(c);
      continue;
    }
    const size_t len = c >= 0xc2 && c <= 0xdf   ? 2
                       : c >= 0xe0 && c <= 0xef ? 3
                       : c >= 0xf0 && c <= 0xf4 ? 4
                                                : 0;
    if (!len || i + len > s.size()) {
      ++i;
      continue;
    }
    unsigned cp = c & (len == 2 ? 31 : len == 3 ? 15 : 7);
    bool valid = true;
    for (size_t n = 1; n < len; ++n) {
      const auto b = static_cast<unsigned char>(s[i + n]);
      if ((b & 0xc0) != 0x80) {
        valid = false;
        break;
      }
      cp = (cp << 6) | (b & 63);
    }
    if (!valid || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) ||
        (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000)) {
      ++i;
      continue;
    }
    if (out.size() + len > max)
      break;
    out.append(s, i, len);
    i += len;
  }
  return trim(out);
}

std::string norm(const std::string &s) {
  const auto text = lower(clean(s, 4096));
  std::string out = " ";
  for (size_t i = 0; i < text.size(); ++i) {
    auto c = static_cast<unsigned char>(text[i]);
    if (c == 0xc3 && i + 1 < text.size()) {
      const unsigned cp = 0xc0 + (static_cast<unsigned char>(text[++i]) - 0x80);
      if ((cp >= 0xc0 && cp <= 0xc5) || (cp >= 0xe0 && cp <= 0xe5))
        out += 'a';
      else if (cp == 0xc7 || cp == 0xe7)
        out += 'c';
      else if ((cp >= 0xc8 && cp <= 0xcb) || (cp >= 0xe8 && cp <= 0xeb))
        out += 'e';
      else if ((cp >= 0xcc && cp <= 0xcf) || (cp >= 0xec && cp <= 0xef))
        out += 'i';
      else if (cp == 0xd1 || cp == 0xf1)
        out += 'n';
      else if ((cp >= 0xd2 && cp <= 0xd6) || (cp >= 0xf2 && cp <= 0xf6) ||
               cp == 0xd8 || cp == 0xf8)
        out += 'o';
      else if ((cp >= 0xd9 && cp <= 0xdc) || (cp >= 0xf9 && cp <= 0xfc))
        out += 'u';
      else if (cp == 0xdd || cp == 0xfd || cp == 0xff)
        out += 'y';
      else if (cp == 0xdf)
        out += "ss";
      else {
        out += char(c);
        out += text[i];
      }
      continue;
    }
    if (c == '+') {
      out += sx::i18n::tr(" plus ");
      continue;
    }
    if (std::isalnum(c) || c >= 128)
      out += char(c);
    else if (out.back() != ' ')
      out += ' ';
  }
  if (out.back() != ' ')
    out += ' ';
  return out;
}

bool phrase(const std::string &text, const std::string &term) {
  const auto t = norm(term);
  return t.size() >= 5 && norm(text).find(t) != std::string::npos;
}
bool safe_id(const std::string &s) {
  return !s.empty() && s.size() <= 100 && s.find("..") == std::string::npos &&
         std::all_of(s.begin(), s.end(), [](unsigned char c) {
           return std::isalnum(c) || c == '/' || c == '_' || c == '.' ||
                  c == '-';
         });
}
Preferences load(const json &j) {
  Preferences p;
  const auto &s = jobj(j, "sports_hub");
  p.enabled = jbool(s, "enabled");
  p.hide_scores = jbool(s, "hide_scores");
  p.only_channels = jbool(s, "only_channels");
  p.region = jstr(s, "region", "ALL");
  if (p.region != "US" && p.region != "UK" && p.region != "IE" &&
      p.region != "CA" && p.region != "AU" && p.region != "OTHER")
    p.region = "ALL";
  const double offset = jnum(s, "utc_offset");
  if (std::isfinite(offset) && offset >= -720 && offset <= 840 &&
      std::fmod(offset, 15) == 0)
    p.utc_offset = int(offset);
  p.excluded_sports = ids(s, "excluded_sports");
  p.excluded_leagues = ids(s, "excluded_leagues");
  p.followed_leagues = ids(s, "followed_leagues");
  for (const auto &t : arr(s, "teams")) {
    Team team{jstr(t, "league"), jstr(t, "id"), clean(jstr(t, "name")),
              clean(jstr(t, "abbr"), 20)};
    if (safe_id(team.league) && safe_id(team.id) && !team.name.empty() &&
        p.teams.size() < 128 &&
        std::none_of(p.teams.begin(), p.teams.end(),
                     [&](const auto &x) { return same_team(x, team); }))
      p.teams.push_back(team);
  }
  for (const auto &word : strings(jobj(s, "words"), 64))
    if (norm(word).size() >= 5 &&
        std::none_of(p.words.begin(), p.words.end(),
                     [&](const auto &w) { return norm(w) == norm(word); }))
      p.words.push_back(word);
  return p;
}
void save(json &j, const Preferences &p) {
  json teams = json::array();
  for (const auto &t : p.teams)
    teams.push_back({{"league", t.league},
                     {"id", t.id},
                     {"name", t.name},
                     {"abbr", t.abbr}});
  j["sports_hub"] = {{"enabled", p.enabled},
                     {"hide_scores", p.hide_scores},
                     {"only_channels", p.only_channels},
                     {"region", p.region},
                     {"utc_offset", p.utc_offset},
                     {"excluded_sports", p.excluded_sports},
                     {"excluded_leagues", p.excluded_leagues},
                     {"followed_leagues", p.followed_leagues},
                     {"teams", teams},
                     {"words", p.words}};
}
Catalogue catalogue(const std::string &raw) {
  Catalogue c;
  if (raw.size() > 512 * 1024)
    return c;
  const auto j = json::parse(raw, nullptr, false);
  if (jnum(j, "schema") != 1)
    return c;
  for (const auto &s : arr(j, "sports"))
    if (safe_id(jstr(s, "id")) && c.sports.size() < 40)
      c.sports.push_back({jstr(s, "id"), clean(jstr(s, "name"))});
  std::set<std::string> seen;
  for (const auto &l : arr(j, "leagues")) {
    League a{jstr(l, "id"),
             jstr(l, "sport"),
             clean(jstr(l, "name")),
             jstr(l, "path"),
             strings(jobj(l, "words")),
             strings(jobj(l, "regions"))};
    if (safe_id(a.id) && !a.name.empty() &&
        (a.path.empty() || safe_id(a.path)) && seen.insert(a.id).second &&
        c.leagues.size() < 512 &&
        std::any_of(c.sports.begin(), c.sports.end(),
                    [&](const auto &s) { return s.id == a.sport; }))
      c.leagues.push_back(std::move(a));
  }
  return c;
}
const League *league(const Catalogue &c, const std::string &id) {
  for (const auto &l : c.leagues)
    if (l.id == id)
      return &l;
  return nullptr;
}
bool included(const League &l, const Preferences &p) {
  return !p.excluded_sports.count(l.sport) && !p.excluded_leagues.count(l.id);
}
std::string team_key(const Team &t) { return family(t.league) + ":" + t.id; }
bool follows(const Event &e, const Preferences &p) {
  if (p.followed_leagues.count(e.league))
    return true;
  for (const auto &t : e.teams)
    for (const auto &f : p.teams)
      if (same_team(t, f))
        return true;
  if (e.provider == "Guide")
    for (const auto &t : p.teams)
      if (phrase(e.title, t.name))
        return true;
  for (const auto &w : p.words)
    if (phrase(e.title, w))
      return true;
  return false;
}
std::vector<Event> scoreboard(const json &j, const League &l, int64_t at,
                              bool fresh, const std::string &region) {
  std::vector<Event> out;
  for (const auto &raw : arr(j, "events")) {
    if (out.size() >= 4000)
      break;
    const auto id = clean(jstr(raw, "id"), 64);
    const auto start = stamp(jstr(raw, "date"));
    if (!safe_id(id) || !start)
      continue;
    Event e;
    e.id = l.id + ":" + id;
    e.league = l.id;
    e.title = clean(jstr(raw, "name"));
    e.provider = "ESPN";
    e.start = start;
    e.end = stamp(jstr(raw, "endDate"));
    if (e.end <= e.start)
      e.end = start + (l.sport == "cricket" ? 12
                       : l.sport == "golf"  ? 8
                                            : 4) *
                          3600;
    e.updated = at;
    e.fresh = fresh;
    const auto &comps = arr(raw, "competitions");
    const auto &comp = comps.empty() ? raw : comps.front();
    const auto &status = jobj(jobj(comp, "status"), "type");
    e.state = jstr(status, "state",
                   jstr(jobj(jobj(raw, "status"), "type"), "state", "pre"));
    if (e.state != "in" && e.state != "post" && e.state != "pre")
      e.state = "off";
    e.status = clean(jstr(status, "shortDetail", jstr(status, "description")));
    std::vector<std::string> scores;
    for (const auto &competitor : arr(comp, "competitors")) {
      auto t = side(jobj(competitor, "team").is_object()
                        ? jobj(competitor, "team")
                        : jobj(competitor, "athlete"),
                    l);
      if (t.name.empty())
        continue;
      e.teams.push_back(t);
      const auto &score = jobj(competitor, "score");
      std::string n = score.is_string()   ? score.get<std::string>()
                      : score.is_number() ? score.dump()
                                          : "";
      if (!n.empty())
        scores.push_back((t.abbr.empty() ? t.name : t.abbr) + " " +
                         clean(n, 12));
    }
    if (e.state == "in" && fresh)
      e.end = std::max(e.end, at + 1800);
    if (e.state == "post" || (e.state == "in" && fresh))
      e.score = join(scores, " · ");
    if (e.state == "in" && !fresh) {
      e.state = "pre";
      e.status = sx::i18n::tr("Score update needed");
      e.score.clear();
    }
    for (const auto &b : arr(comp, "geoBroadcasts")) {
      auto country = lower(jstr(b, "region"));
      const auto wanted = region == "UK" ? "gb" : lower(region);
      if (region == "ALL" || country.empty() || country == wanted) {
        auto name = clean(jstr(jobj(b, "media"), "shortName"));
        if (!name.empty())
          e.broadcasts.push_back(name);
      }
    }
    if (e.broadcasts.empty())
      for (const auto &b : arr(comp, "broadcasts"))
        for (const auto &name : strings(jobj(b, "names"), 16))
          e.broadcasts.push_back(name);
    if (!e.title.empty())
      out.push_back(std::move(e));
  }
  return out;
}
Snapshot parse_feed(const std::string &raw, const Catalogue &c, int64_t now,
                    int64_t previous) {
  Snapshot s;
  const auto fail = [&](const char *message) {
    s.events.clear();
    s.error = message;
    return s;
  };
  if (raw.size() > max_feed_bytes)
    return fail(sx::i18n::tr("Sports catalogue exceeds the size limit"));
  const auto j = json::parse(raw, nullptr, false);
  const auto generated = jnum(j, "generatedAt") / 1000;
  if (jnum(j, "schemaVersion") != 1 || !std::isfinite(generated) ||
      generated < now - max_age || generated > now + 600 ||
      generated < previous || arr(j, "entries").empty() ||
      arr(j, "entries").size() > 512)
    return fail(sx::i18n::tr("Sports catalogue is invalid or expired"));
  s.generated = int64_t(generated);
  std::set<std::string> seen;
  for (const auto &entry : arr(j, "entries")) {
    const auto *l = league(c, jstr(entry, "leagueId"));
    const auto provider = jstr(entry, "provider");
    const double at = jnum(entry, "fetchedAt") / 1000;
    if (!l || !std::isfinite(at) || at < now - max_age ||
        at > s.generated + 600)
      continue;
    if (provider == "espn")
      for (auto &e : scoreboard(jobj(entry, "payload"), *l, int64_t(at), false))
        if (e.start >= now - max_age && e.start <= now + 31 * day &&
            seen.insert(e.id).second && s.events.size() < 20000)
          s.events.push_back(std::move(e));
    if (provider == "tsdb")
      for (const auto &raw : arr(jobj(entry, "payload"), "events")) {
        Event e;
        const auto id = clean(jstr(raw, "idEvent"), 64);
        if (!safe_id(id))
          continue;
        e.id = l->id + ":" + id;
        e.league = l->id;
        e.title = clean(jstr(raw, "strEvent"));
        e.provider = "TheSportsDB";
        e.start = stamp(jstr(raw, "strTimestamp",
                             jstr(raw, "dateEvent") + "T" +
                                 jstr(raw, "strTime", "00:00:00")));
        e.end = e.start + 4 * 3600;
        e.updated = int64_t(at);
        if (!e.start || e.start < now - max_age || e.start > now + 31 * day ||
            e.title.empty())
          continue;
        const auto home = jstr(raw, "strHomeTeam"),
                   away = jstr(raw, "strAwayTeam");
        if (!home.empty())
          e.teams.push_back({l->id, jstr(raw, "idHomeTeam"), clean(home), ""});
        if (!away.empty())
          e.teams.push_back({l->id, jstr(raw, "idAwayTeam"), clean(away), ""});
        const auto &h = jobj(raw, "intHomeScore");
        const auto &a = jobj(raw, "intAwayScore");
        if (e.end < now && !h.is_null() && !a.is_null()) {
          e.state = "post";
          e.score = clean(h.is_string()   ? h.get<std::string>()
                          : h.is_number() ? h.dump()
                                          : "",
                          12) +
                    " - " +
                    clean(a.is_string()   ? a.get<std::string>()
                          : a.is_number() ? a.dump()
                                          : "",
                          12);
        }
        if (seen.insert(e.id).second && s.events.size() < 20000)
          s.events.push_back(std::move(e));
      }

    if (provider == "calendar")
      for (const auto &raw : arr(jobj(entry, "payload"), "events")) {
        Event e;
        e.league = l->id;
        e.title = clean(jstr(raw, "fixture"));
        e.provider = "sporting-events.org";
        const auto time = jstr(raw, "time_utc");
        e.has_time = !time.empty();
        e.start = stamp(jstr(raw, "date_utc") +
                        (time.empty() ? "" : "T" + time + "Z"));
        e.end = e.start + (e.has_time ? 4 * 3600 : day);
        e.updated = int64_t(at);
        if (!e.start || e.start < now - max_age || e.start > now + 31 * day ||
            e.title.empty())
          continue;
        const auto state = lower(jstr(raw, "status"));
        e.state = state == "completed"                           ? "post"
                  : state == "cancelled" || state == "postponed" ? "off"
                                                                 : "pre";
        e.id = l->id + ":calendar:" + std::to_string(e.start) + ":" +
               std::to_string(fnv1a(e.title));
        if (seen.insert(e.id).second && s.events.size() < 20000)
          s.events.push_back(std::move(e));
      }
  }
  return s;
}
std::vector<Team> teams(const json &j, const League &l) {
  std::vector<Team> out;
  std::set<std::string> seen;
  for (const auto &sport : arr(j, "sports"))
    for (const auto &group : arr(sport, "leagues"))
      for (const auto &raw : arr(group, "teams")) {
        auto t = side(jobj(raw, "team"), l);
        if (safe_id(t.id) && !t.name.empty() && seen.insert(t.id).second &&
            out.size() < 2000)
          out.push_back(std::move(t));
      }
  std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
    return lower(a.name) < lower(b.name);
  });
  return out;
}
std::string espn_url(const League &l, bool team, int64_t date) {
  if (!safe_id(l.path) || l.path.empty())
    return {};
  std::string url = "https://site.api.espn.com/apis/site/v2/sports/" + l.path +
                    (team ? "/teams?limit=1000" : "/scoreboard?limit=1000");
  if (!team && date > 0) {
    std::tm t{};
    const time_t when = date;
    gmtime_r(&when, &t);
    char b[16];
    std::strftime(b, sizeof(b), "%Y%m%d", &t);
    url += "&dates=" + std::string(b);
  }
  return url;
}
std::string clock(int64_t epoch, int offset) {
  time_t when = epoch + int64_t(offset) * 60;
  std::tm t{};
  gmtime_r(&when, &t);
  char b[48];
  std::strftime(b, sizeof(b), "%a %d %b %H:%M", &t);
  return b;
}
std::string event_time(const Event &e, int offset) {
  return e.has_time ? clock(e.start, offset)
                    : clock(e.start, 0).substr(0, 10) + sx::i18n::tr(" · time TBD");
}
std::string event_status(const Event &e, bool hidden, int64_t now) {
  if (e.provider == "Guide")
    return e.start <= now && now < e.end ? sx::i18n::tr("On now in your guide")
                                         : sx::i18n::tr("In your guide");
  if (e.state == "post")
    return hidden
               ? sx::i18n::tr("Finished · scores hidden")
               : sx::i18n::tr("Final") + (e.score.empty() ? std::string() : " · " + e.score);
  if (e.state == "in" && e.fresh && now - e.updated <= 120)
    return hidden ? sx::i18n::tr("Live · scores hidden")
                  : sx::i18n::tr("Live · ") + e.status +
                        (e.score.empty() ? std::string() : " · " + e.score);
  if (e.state == "off")
    return sx::i18n::tr("Postponed / unavailable");
  if (!e.has_time)
    return sx::i18n::tr("Time to be confirmed");
  return e.start <= now ? sx::i18n::tr("Started · refresh for score") : sx::i18n::tr("Scheduled");
}
ChannelIndex index_channels(const std::vector<live_tv::Channel> &channels) {
  ChannelIndex index;
  for (size_t i = 0; i < channels.size(); ++i) {
    auto name = norm(channels[i].name);
    for (const char *marker :
         {" us ", " uk ", " gb ", " ie ", " ca ", " au ", " hd ", " fhd ",
          " sd ", " 4k ", " 1080p ", " 720p ", " 2160p "}) {
      const auto at = name.find(marker);
      if (at != std::string::npos)
        name.replace(at, std::char_traits<char>::length(marker), " ");
    }
    index.networks[trim(name)].push_back(i);
  }
  return index;
}
std::vector<Match> matches(const Event &e,
                           const std::vector<live_tv::Channel> &channels,
                           const std::vector<Guide> &guide,
                           const std::string &region,
                           const ChannelIndex *supplied) {
  std::map<size_t, Match> found;
  for (const auto &g : guide) {
    if (g.channel >= channels.size() || g.programme.end < e.start - 1800 ||
        g.programme.start > e.end + 1800)
      continue;
    int sides = 0;
    for (const auto &t : e.teams)
      if (phrase(g.programme.title, t.name) ||
          (t.abbr.size() >= 3 && phrase(g.programme.title, t.abbr)))
        ++sides;
    if ((e.provider == "Guide" && g.programme.title == e.title) ||
        (e.teams.size() >= 2 && sides >= 2))
      found[g.channel] = {g.channel, 300, sx::i18n::tr("Guide title matches event")};
  }
  ChannelIndex local;
  if (!supplied) {
    local = index_channels(channels);
    supplied = &local;
  }
  for (const auto &broadcast : e.broadcasts) {
    const auto network = trim(norm(broadcast));
    if (network.size() < 3)
      continue;
    const auto at = supplied->networks.find(network);
    if (at == supplied->networks.end())
      continue;
    size_t count = 0;
    for (const auto i : at->second) {
      if (i >= channels.size() || ++count > 256)
        break;
      if (found.count(i))
        continue;
      const bool preferred =
          region != "ALL" &&
          norm(channels[i].name).find(" " + lower(region) + " ") !=
              std::string::npos;
      found[i] = {i, preferred ? 120 : 100,
                  sx::i18n::tr("Broadcaster match; programme unconfirmed")};
    }
  }
  std::vector<Match> out;
  for (const auto &pair : found)
    out.push_back(pair.second);
  std::stable_sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
    return a.rank > b.rank;
  });
  if (out.size() > 256)
    out.resize(256);
  return out;
}
std::vector<Event> guide_events(const Catalogue &c,
                                const std::vector<Guide> &guide, int64_t now,
                                const std::vector<std::string> &names) {
  struct Alias {
    const League *league;
    std::string key;
  };
  std::vector<Alias> aliases;
  for (const auto &l : c.leagues)
    for (const auto &word : l.words) {
      const auto simple = trim(norm(word));
      if (simple.size() < 3 || simple == "world cup" ||
          simple == "championship" || simple == "football" ||
          simple == "rugby" || simple == "premier league" ||
          simple == "grand prix" || simple == "champions league" ||
          simple == "super cup" || simple == "league cup")
        continue;
      aliases.push_back({&l, " " + simple + " "});
    }
  std::vector<Event> out;
  std::set<std::string> seen;
  for (const auto &g : guide) {
    if (g.programme.end < now - day || g.programme.start > now + 7 * day)
      continue;
    const League *best = nullptr;
    size_t weight = 0;
    bool ambiguous = false;
    const auto title =
        g.normalized.empty() ? norm(g.programme.title) : g.normalized;
    for (const auto &alias : aliases)
      if (title.find(alias.key) != std::string::npos) {
        if (alias.key.size() > weight) {
          best = alias.league;
          weight = alias.key.size();
          ambiguous = false;
        } else if (alias.key.size() == weight && best &&
                   best->sport != alias.league->sport)
          ambiguous = true;
      }
    if (!best || ambiguous) {
      const bool followed =
          std::any_of(names.begin(), names.end(), [&](const auto &name) {
            return phrase(g.programme.title, name);
          });
      best = followed ? league(c, "guide/other") : nullptr;
    }
    if (!best)
      continue;
    Event e;
    e.id = "guide:" + std::to_string(g.programme.start) + ":" +
           std::to_string(fnv1a(g.programme.title));
    if (!seen.insert(e.id).second)
      continue;
    e.league = best->id;
    e.title = clean(g.programme.title);
    e.start = g.programme.start;
    e.end = g.programme.end;
    e.provider = "Guide";
    e.state = e.end < now ? "post" : e.start <= now ? "in" : "pre";
    e.updated = now;
    e.fresh = true;
    out.push_back(std::move(e));
    if (out.size() >= 2000)
      break;
  }
  return out;
}
std::vector<Event> visible(const Snapshot &s, const Catalogue &c,
                           const Preferences &p, int view, int64_t now,
                           const std::string &query) {
  std::vector<Event> out;
  if (!p.enabled)
    return out;
  for (const auto &e : s.events) {
    const auto *l = league(c, e.league);
    if (!l || !included(*l, p) || e.end < now - day || e.start > now + 7 * day)
      continue;
    if (view == 1 && !follows(e, p))
      continue;
    if (view == 2 && !(e.has_time && e.start <= now && e.end > now &&
                       e.state != "post" && e.state != "off"))
      continue;
    if (view == 3 && (e.has_time ? e.start < now : e.end < now))
      continue;
    if (view == 4 && e.state != "post")
      continue;
    if (!query.empty() &&
        norm(e.title + " " + l->name).find(trim(norm(query))) ==
            std::string::npos)
      continue;
    out.push_back(e);
  }
  std::stable_sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
    return a.start == b.start ? a.id < b.id : a.start < b.start;
  });
  return out;
}
} // namespace sx::sports
