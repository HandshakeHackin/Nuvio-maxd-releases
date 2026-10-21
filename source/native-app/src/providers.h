// Isolated JavaScript provider repositories, separate from Stremio addon
// resources.
#pragma once
#include "stremio.h"
namespace providers {
// Split enabled scripts into independently cancellable worker jobs without
// copying the complete repository manifest into every queued request.
inline std::vector<Addon>
stream_sources(const Addon &a, const std::string &type, const std::string &id) {
  if (!a.provider_repository)
    return a.supports("stream", type, id) ? std::vector<Addon>{a}
                                          : std::vector<Addon>{};
  std::vector<Addon> result;
  for (const auto &script : a.providers) {
    if (!script.enabled)
      continue;
    Addon source;
    source.provider_repository = true;
    source.transport_url = a.transport_url;
    source.id = a.id;
    source.name = a.name + " / " + script.name;
    source.types = a.types;
    source.providers = {script};
    if (source.supports("stream", type, id))
      result.push_back(std::move(source));
  }
  return result;
}
struct Result {
  std::vector<Stream> streams;
  std::string error;
};
#ifdef NUVIO_HAVE_PROVIDERS
struct Identity {
  std::string tmdb, imdb;
  int season = 0, episode = 0;
};
Identity identify(const std::string &video_id, const Meta &meta);
bool initialize(const std::string &runtime_path);
bool configure(const std::string &applications_path);
json trailer_videos(const Meta&,const std::atomic<bool>* cancel=nullptr);
void clear_script_cache();
json settings_layout(const Addon &, const ProviderScript &, std::string &error,
                     const std::atomic<bool> *cancel = nullptr);
Result streams(const Addon &, const std::string &type,
               const std::string &video_id, const Meta &,
               const std::atomic<bool> *cancel);
// Deterministic entry point also used by host compatibility tests.
json execute(const std::string &source, const json &arguments,
             const json &settings, std::string &error,
             const std::atomic<bool> *cancel = nullptr, int timeout_ms = 40000,
             bool describe_settings = false);
#else
inline bool initialize(const std::string &) { return false; }
inline bool configure(const std::string &) { return false; }
inline void clear_script_cache() {}
inline json trailer_videos(const Meta&,const std::atomic<bool>* =nullptr) {return json::object();}
inline json settings_layout(const Addon &, const ProviderScript &, std::string &error,
                            const std::atomic<bool> * = nullptr) {
  error="This build does not include JavaScript providers."; return json::array();
}
inline Result streams(const Addon &, const std::string &, const std::string &,
                      const Meta &, const std::atomic<bool> *) {
  return {{}, "This build does not include JavaScript providers."};
}
#endif
inline Result fetch(const Addon &a, const std::string &type,
                    const std::string &id, const Meta &meta,
                    const std::atomic<bool> *cancel) {
  if (a.provider_repository)
    return streams(a, type, id, meta, cancel);
  Result r;
  json j;
  if (fetch_json(a.resource_url("stream", type, id), j, r.error, 30, cancel))
    r.streams = parse_streams(j, a.name);
  return r;
}
} // namespace providers
