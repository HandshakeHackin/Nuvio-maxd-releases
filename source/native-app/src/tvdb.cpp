#include "tvdb.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>
namespace tvdb {
namespace {
using J = nlohmann::json;
struct Failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};
std::string field(const J &j, const char *k) {
  if (!j.is_object() || !j.contains(k))
    return {};
  const auto &v = j[k];
  return v.is_string()           ? v.get<std::string>()
         : v.is_number_integer() ? v.dump()
                                 : std::string();
}
bool identifier(const std::string &s) {
  return !s.empty() && s.size() <= 64 &&
         s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVW"
                             "XYZ0123456789_-") == std::string::npos;
}
int number(const J &j, const char *k) {
  auto s = field(j, k);
  if (s.empty() || s.size() > 6 ||
      s.find_first_not_of("0123456789") != std::string::npos)
    return -1;
  return std::stoi(s);
}
std::string signature(const std::string &s) {
  std::string out;
  for (unsigned char c : s)
    if (std::isalnum(c) || c >= 128)
      out += char(std::tolower(c));
  return out;
}
bool date(const std::string &s) {
  return s.size() >= 10 && s[4] == '-' && s[7] == '-' &&
         s.substr(0, 10).find_first_not_of("0123456789-") == std::string::npos;
}
std::string anchor(const std::string &title, const std::string &aired) {
  return !date(aired) || signature(title).empty()
             ? std::string()
             : signature(title) + ":" + aired.substr(0, 10);
}
class Client {
  const std::atomic<bool> *cancel;
  feature_http::Request request;
  std::string token;

public:
  Client(const Credentials &c, const std::atomic<bool> *stop,
         const feature_http::Request &r)
      : cancel(stop), request(r ? r : feature_http::perform) {
    if (c.key.empty() || c.key.size() > 4096 || c.pin.size() > 256)
      throw Failure("Enter your TVDB API key in Settings first.");
    J body{{"apikey", c.key}};
    if (!c.pin.empty())
      body["pin"] = c.pin;
    auto j = call("login", body.dump());
    token = field(j["data"], "token");
    if (token.empty() || token.size() > 8192 ||
        token.find_first_of("\r\n") != std::string::npos)
      throw Failure("TVDB authentication failed.");
  }
  J call(const std::string &path, const std::string &body = {}) {
    if (cancel && *cancel)
      throw Failure("Cancelled");
    feature_http::Query q;
    q.url = "https://api4.thetvdb.com/v4/" + path;
    q.limit = 8u << 20;
    q.headers = {"Accept: application/json"};
    if (!token.empty())
      q.headers.push_back("Authorization: Bearer " + token);
    if (!body.empty()) {
      q.method = "POST";
      q.body = body;
      q.headers.push_back("Content-Type: application/json");
    }
    auto r = request(q, cancel);
    if (cancel && *cancel)
      throw Failure("Cancelled");
    if (!r.ok())
      throw Failure(r.status == 401 || r.status == 403
                        ? "TVDB rejected access. Check your API key, "
                          "subscription and PIN."
                    : r.status == 429
                        ? "TVDB request limit reached. Try again later."
                        : "TVDB request failed. Provider order is retained.");
    bool deep = false;
    auto j = J::parse(
        r.body,
        [&](int d, J::parse_event_t, J &) {
          if (d > 40) {
            deep = true;
            return false;
          }
          return true;
        },
        false);
    if (deep || !j.is_object() || field(j, "status") != "success" ||
        !j.contains("data"))
      throw Failure("TVDB returned an invalid response.");
    return j;
  }
  std::string series(const Meta &m) {
    auto remote = m.imdb_id;
    if (remote.empty() && m.id.rfind("tt", 0) == 0)
      remote = m.id;
    if (remote.empty() && !m.tmdb_id.empty())
      remote = m.tmdb_id;
    if (!identifier(remote))
      throw Failure("This series has no supported remote ID for TVDB.");
    auto j = call("search/remoteid/" + remote);
    if (!j["data"].is_array())
      throw Failure("TVDB series lookup failed.");
    std::set<std::string> ids;
    for (const auto &row : j["data"])
      if (row.is_object() && row.contains("series")) {
        auto id = field(row["series"], "id");
        if (identifier(id))
          ids.insert(id);
      }
    if (ids.size() != 1)
      throw Failure("TVDB could not identify this series unambiguously.");
    return *ids.begin();
  }
  std::vector<Order> orders(const std::string &id) {
    auto j = call("series/" + id + "/extended")["data"];
    std::vector<Order> out;
    std::set<std::string> ids;
    if (!j.is_object() || !j.contains("seasons") || !j["seasons"].is_array())
      throw Failure("TVDB returned no episode orders.");
    for (const auto &s : j["seasons"])
      if (s.is_object() && s.contains("type")) {
        auto type = field(s["type"], "type"), name = field(s["type"], "name");
        if (identifier(type) && ids.insert(type).second && out.size() < 32)
          out.push_back({type, name.empty() ? type : name.substr(0, 128)});
      }
    return out;
  }
  std::vector<Episode> episodes(const std::string &series,
                                const std::string &order) {
    std::vector<Episode> out;
    std::set<std::string> ids;
    std::set<std::pair<int, int>> coords;
    size_t bytes = 0;
    for (int page = 0; page < 256; ++page) {
      auto j = call("series/" + series + "/episodes/" + order +
                    "/eng?page=" + std::to_string(page));
      bytes += j.dump().size();
      if (bytes > (32u << 20) || !j["data"].is_object() ||
          !j["data"].contains("episodes") || !j["data"]["episodes"].is_array())
        throw Failure(
            "TVDB episode order is invalid or exceeds its size limit.");
      const auto &rows = j["data"]["episodes"];
      for (const auto &row : rows) {
        Episode e{field(row, "id"), field(row, "name"), field(row, "aired"),
                  number(row, "seasonNumber"), number(row, "number")};
        if (!identifier(e.id) || e.season < 0 || e.episode <= 0 ||
            !ids.insert(e.id).second ||
            !coords.insert({e.season, e.episode}).second || out.size() >= 20000)
          throw Failure("TVDB returned ambiguous or repeated episode data.");
        out.push_back(std::move(e));
      }
      auto links = j.find("links");
      if (links == j.end() || !links->is_object() || !links->contains("next") ||
          (*links)["next"].is_null() || field(*links, "next").empty())
        return out;
      // The server's next URL is an indicator only, never a request target.
      if (rows.empty())
        throw Failure("TVDB returned an incomplete episode order.");
    }
    throw Failure("TVDB episode pagination did not finish.");
  }
};
} // namespace
bool reorder(Meta &meta, const std::vector<Episode> &aired,
             const std::vector<Episode> &selected, std::string &error) {
  error.clear();
  auto fail = [&] {
    error = "This order cannot be matched one-to-one with the provider "
            "episodes. Provider order is retained.";
    return false;
  };
  if (meta.videos.empty() || meta.videos.size() > 20000 || aired.empty() ||
      selected.empty() || aired.size() > 20000 || selected.size() > 20000)
    return fail();
  std::map<std::pair<int, int>, const Episode *> by_coord;
  std::map<std::string, const Episode *> by_id;
  std::map<std::string, std::vector<const Episode *>> by_anchor;
  for (const auto &e : aired) {
    if (e.id.empty() ||
        !by_coord.emplace(std::make_pair(e.season, e.episode), &e).second ||
        !by_id.emplace(e.id, &e).second)
      return fail();
    auto a = anchor(e.title, e.aired);
    if (!a.empty())
      by_anchor[a].push_back(&e);
  }
  std::map<std::string, const Video *> canonical;
  std::set<std::string> video_ids;
  for (const auto &v : meta.videos) {
    if (v.id.empty() || !video_ids.insert(v.id).second)
      return fail();
    auto p = by_coord.find({v.season, v.episode});
    if (p == by_coord.end())
      return fail();
    // Numbering alone is never evidence that two records describe the same
    // file.
    auto a = anchor(v.title, v.released);
    if (a.empty() || a != anchor(p->second->title, p->second->aired) ||
        !canonical.emplace(p->second->id, &v).second)
      return fail();
  }
  std::vector<Video> mapped;
  std::set<std::string> seen, selected_ids;
  std::set<std::pair<int, int>> coordinates;
  for (const auto &e : selected) {
    if (e.id.empty() || e.season < 0 || e.episode <= 0 ||
        !selected_ids.insert(e.id).second ||
        !coordinates.insert({e.season, e.episode}).second)
      return fail();
    const Episode *original = nullptr;
    auto id = by_id.find(e.id);
    if (id != by_id.end())
      original = id->second;
    else {
      auto a = anchor(e.title, e.aired);
      auto match = by_anchor.find(a);
      if (a.empty() || match == by_anchor.end() || match->second.size() != 1)
        return fail();
      original = match->second.front();
    }
    auto v = canonical.find(original->id);
    if (v == canonical.end()) {
      // TVDB may know episodes that the provider has not supplied, including
      // specials.
      continue;
    }
    if (!seen.insert(v->second->id).second)
      return fail();
    Video copy = *v->second;
    copy.order_season = e.season;
    copy.order_episode = e.episode;
    auto base = meta.imdb_id;
    if (base.empty() && meta.id.rfind("tt", 0) == 0)
      base = meta.id;
    if (base.empty() && !meta.tmdb_id.empty())
      base = "tmdb:" + meta.tmdb_id;
    if (base.empty())
      return fail();
    copy.order_id =
        base + ":" + std::to_string(e.season) + ":" + std::to_string(e.episode);
    mapped.push_back(std::move(copy));
  }
  if (mapped.size() != meta.videos.size())
    return fail();
  std::sort(mapped.begin(), mapped.end(), [](const auto &a, const auto &b) {
    return std::pair(display_season(a), display_episode(a)) <
           std::pair(display_season(b), display_episode(b));
  });
  meta.videos = std::move(mapped);
  return true;
}
Result load(const Meta &meta, const Credentials &credentials,
            const std::string &order, const std::atomic<bool> *cancel,
            const feature_http::Request &request) {
  Result out;
  out.meta = meta;
  try {
    if (meta.type != "series")
      throw Failure("Episode ordering is available for series only.");
    if (!order.empty() && !identifier(order))
      throw Failure("Invalid TVDB order.");
    Client c(credentials, cancel, request);
    out.series_id = c.series(meta);
    out.orders = c.orders(out.series_id);
    if (order.empty())
      return out;
    if (std::none_of(out.orders.begin(), out.orders.end(),
                     [&](const auto &o) { return o.id == order; }))
      throw Failure("TVDB no longer lists this episode order. Provider order "
                    "is retained.");
    const auto aired = c.episodes(out.series_id, "official"),
               selected = order == "official"
                              ? aired
                              : c.episodes(out.series_id, order);
    reorder(out.meta, aired, selected, out.error);
  } catch (const Failure &e) {
    out.error = e.what();
  } catch (...) {
    out.error =
        "TVDB episode order could not be read. Provider order is retained.";
  }
  return out;
}
bool test(const Credentials &c, std::string &error,
          const std::atomic<bool> *cancel,
          const feature_http::Request &request) {
  try {
    Client client(c, cancel, request);
    error.clear();
    return true;
  } catch (const Failure &e) {
    error = e.what();
  } catch (...) {
    error = "TVDB connection failed.";
  }
  return false;
}
} // namespace tvdb
