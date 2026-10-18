#pragma once

#include <atomic>
#include <map>
#include <string>
#include <vector>

#include "util.h"

// ---------------------------------------------------------------------------
// Addons (https://github.com/Stremio/stremio-addon-sdk/blob/master/docs/protocol.md)

struct CatalogExtra {
	std::string name;
	bool required = false;
	std::vector<std::string> options;
};

struct Catalog {
	std::string type, id, name;
	std::vector<CatalogExtra> extras;
	bool show_in_home = true;  // explicit showInHome=false hides this row
	bool has_extra(const std::string& n) const;
	bool on_home() const;
	bool requires_other_than(const std::string& allowed) const;  // a required extra other than `allowed`
	const CatalogExtra* extra(const std::string& n) const;
};

struct AddonResource {
	std::string name;
	std::vector<std::string> types;        // empty: the manifest's types
	std::vector<std::string> id_prefixes;  // empty: the manifest's idPrefixes (or any)
};

struct ProviderScript {
 std::string id, name, filename; std::vector<std::string> types; bool enabled=false;
};

struct Addon {
 bool provider_repository=false;
 std::vector<ProviderScript> providers;
	std::string transport_url;  // .../manifest.json
	std::string base;           // transport_url without /manifest.json
	std::string query;          // configured addon parameters, retained on resources
	std::string id, name, version, description;
	std::vector<std::string> types;
	std::vector<std::string> id_prefixes;
	std::vector<AddonResource> resources;
	std::vector<Catalog> catalogs;
	bool from_account = false;
	json manifest;

	bool supports(const std::string& resource, const std::string& type, const std::string& id) const;
	std::string resource_url(const std::string& resource, const std::string& type, const std::string& id,
	                         const std::string& extra = "") const;
};

bool parse_addon(const json& manifest, const std::string& transport_url, Addon& out);
std::string normalize_addon_url(std::string url);

// ---------------------------------------------------------------------------
// Metadata

struct Video {
	std::string id, title, thumbnail, overview, released;
	int season = 0, episode = 0;
};

struct Meta {
	std::string id, type, name, imdb_id, tmdb_id;
	std::string poster, background, logo;
	std::string description, release_info, runtime, imdb_rating, year;
	std::vector<std::string> genres, cast, directors;
	std::vector<Video> videos;
};

Meta parse_meta(const json& j);

struct Stream {
	std::string name, description;  // display
	std::string addon;              // addon name
	std::string url;                // direct HTTP(S) URL
	std::string info_hash;          // torrent
	int file_idx = -1;
	std::vector<std::string> sources;
	std::string filename;
	double video_size = 0;          // behaviorHints.videoSize, bytes when supplied
	std::string binge_group;
	bool not_web_ready = false;
	std::vector<std::string> request_headers;  // behaviorHints.proxyHeaders.request
	json client_resolve;           // AIOStreams client-side debrid instructions
	bool debrid_resolvable = false; // session-derived; never implies local P2P
	std::string debrid_service;
	bool direct_http() const {
		const std::string prefix = lower(url.substr(0, 8));
		return starts_with(prefix, "https://") || starts_with(prefix, "http://");
	}
	bool torrent() const { return url.empty() && !info_hash.empty() && !client_resolve.is_object(); }
	bool playable() const { return direct_http() || torrent() || client_resolve.is_object(); }
	bool allowed(bool allow_torrents) const { return direct_http() || (allow_torrents && torrent()); }
};

std::vector<Stream> parse_streams(const json& j, const std::string& addon_name);

struct SubtitleTrack {
	std::string id, url, lang, label, addon;
};

std::vector<SubtitleTrack> parse_subtitles(const json& j, const std::string& addon_name);

// ---------------------------------------------------------------------------
// Nuvio account adapter. All network calls block; use worker threads.

struct ApiResult {
	bool ok = false;
	json result;
	std::string error;
};

ApiResult api_get_user(const std::string& auth_key);
ApiResult api_addon_collection(const std::string& auth_key);
ApiResult api_library_get(const std::string& auth_key);
ApiResult api_library_put(const std::string& auth_key, const json& items);
ApiResult api_logout(const std::string& auth_key);
ApiResult api_connected_services_pull(const std::string& key = ""); // explicit import from selected Nuvio profile

// Nuvio device pairing; the complete verification URL is encoded locally.
struct LinkCode {
	bool ok = false;
	std::string code, link, error;
	std::string display_code, verification_uri;
	int poll_interval = 3, expires_in = 600;
};
LinkCode link_create();
// Returns ok with result.authKey once approved; ok=false, error="" while pending.
ApiResult link_read(const std::string& code);

// Fetches a manifest and parses it.
bool fetch_addon(const std::string& url, Addon& out, std::string& err, const std::atomic<bool>* cancel = nullptr);

// GET an addon resource as JSON.
bool fetch_json(const std::string& url, json& out, std::string& err, long timeout_s = 20,
                const std::atomic<bool>* cancel = nullptr);

extern const char* kCinemetaUrl;
extern const char* kOpenSubtitlesUrl;

// Language codes: "eng" -> "English", "en" -> "eng".
std::string language_name(const std::string& code);
std::string language_to_iso639_2(const std::string& code);
