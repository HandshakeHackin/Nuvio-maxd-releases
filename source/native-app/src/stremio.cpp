#include "stremio.h"

#include <algorithm>

#include "http.h"

const char* kCinemetaUrl = "https://v3-cinemeta.strem.io/manifest.json";
const char* kOpenSubtitlesUrl = "https://opensubtitles-v3.strem.io/manifest.json";


static std::vector<std::string> str_array(const json& j) {
	std::vector<std::string> out;
	if (j.is_array()) {
		for (auto& v : j)
			if (v.is_string()) out.push_back(v.get<std::string>());
	} else if (j.is_string()) {
		out.push_back(j.get<std::string>());
	}
	return out;
}

// ---------------------------------------------------------------------------
// Addons

bool Catalog::has_extra(const std::string& n) const { return extra(n) != nullptr; }

bool Catalog::on_home() const {
	// Nuvio's home rule: only required search and explicit showInHome=false
	// exclude a catalog. Other extras can have defaults in the addon.
	if (!show_in_home) return false;
	for (const auto& e : extras)
		if (lower(trim(e.name)) == "search" && e.required) return false;
	return true;
}

static bool manifest_flag(const json& value) {
	if (value.is_boolean()) return value.get<bool>();
	if (value.is_string()) return lower(trim(value.get<std::string>())) == "true";
	if (value.is_number()) return value.get<double>() != 0;
	return false;
}

const CatalogExtra* Catalog::extra(const std::string& n) const {
	for (auto& e : extras)
		if (e.name == n) return &e;
	return nullptr;
}

bool Catalog::requires_other_than(const std::string& allowed) const {
	for (auto& e : extras)
		if (e.required && e.name != allowed) return true;
	return false;
}

std::string normalize_addon_url(std::string url) {
	url = trim(url);
	if (starts_with(url, "stremio://")) url = "https://" + url.substr(10);
	if (!starts_with(url, "http://") && !starts_with(url, "https://")) url = "https://" + url;
	const size_t fragment = url.find('#');
	if (fragment != std::string::npos) url.resize(fragment);
	const size_t query_start = url.find('?');
	const std::string query = query_start == std::string::npos ? "" : url.substr(query_start);
	std::string path = url.substr(0, query_start);
	while (!path.empty() && path.back() == '/') path.pop_back();
	if (!ends_with(lower(path), "/manifest.json")) path += "/manifest.json";
	return path + query;
}

bool parse_addon(const json& m, const std::string& transport_url, Addon& a) {
	if (!m.is_object()) return false;
    const bool repository = jobj(m,"scrapers").is_array() && !jstr(m,"name").empty();
    if (!repository && jstr(m,"id").empty()) return false;
    a = Addon{};
    a.provider_repository = repository;
	a.manifest = m;
	a.transport_url = transport_url;
	const size_t query_start = transport_url.find('?');
	a.query = query_start == std::string::npos ? "" : transport_url.substr(query_start);
	a.base = transport_url.substr(0, query_start);
	if (ends_with(lower(a.base), "/manifest.json")) a.base.resize(a.base.size() - 14);
	a.id = jstr(m, "id", "provider:" + transport_url);
	a.name = jstr(m, "name", a.id);
	a.version = jstr(m, "version");
	a.description = jstr(m, "description");
	a.types = str_array(jobj(m, "types"));
	a.id_prefixes = str_array(jobj(m, "idPrefixes"));

    if (repository) {
        a.types={"movie","series"};
        for (const auto& entry : m["scrapers"]) {
            ProviderScript p; p.id=jstr(entry,"id");p.name=jstr(entry,"name",p.id);p.filename=jstr(entry,"filename");
            p.version=jstr(entry,"version",a.version);
            p.types=str_array(jobj(entry,"supportedTypes"));
            if(!p.id.empty() && !p.filename.empty() && p.filename.size()<2048 && a.providers.size()<256)
                a.providers.push_back(std::move(p));
        }
        return !a.providers.empty();
    }
	const json& res = jobj(m, "resources");
	if (res.is_array()) {
		for (auto& r : res) {
			AddonResource ar;
			if (r.is_string()) {
				ar.name = r.get<std::string>();
			} else if (r.is_object()) {
				ar.name = jstr(r, "name");
				ar.types = str_array(jobj(r, "types"));
				ar.id_prefixes = str_array(jobj(r, "idPrefixes"));
			}
			if (!ar.name.empty()) a.resources.push_back(ar);
		}
	}

	const json& cats = jobj(m, "catalogs");
	if (cats.is_array()) {
		for (auto& c : cats) {
			Catalog cat;
			cat.type = jstr(c, "type");
			cat.id = jstr(c, "id");
			cat.name = jstr(c, "name");
			const auto& home = jobj(c, "showInHome");
			cat.show_in_home = home.is_null() || (home.is_boolean() && home.get<bool>());
			if (cat.type.empty() || cat.id.empty()) continue;
			const json& extra = jobj(c, "extra");
			if (extra.is_array()) {
				for (auto& e : extra) {
					CatalogExtra ce;
					ce.name = e.is_string() ? e.get<std::string>() : jstr(e, "name");
					ce.required = manifest_flag(jobj(e, "isRequired"));
					ce.options = str_array(jobj(e, "options"));
					if (!ce.name.empty()) cat.extras.push_back(ce);
				}
			}
			// Older manifests: extraSupported / extraRequired.
			for (auto& n : str_array(jobj(c, "extraSupported")))
				if (!cat.has_extra(n)) cat.extras.push_back(CatalogExtra{n, false, {}});
			for (auto& n : str_array(jobj(c, "extraRequired"))) {
				bool found = false;
				for (auto& e : cat.extras)
					if (e.name == n) e.required = found = true;
				if (!found) cat.extras.push_back(CatalogExtra{n, true, {}});
			}
			if (const CatalogExtra* g = cat.extra("genre")) {
				if (g->options.empty()) {
					// Some addons list genres at the top level.
					for (auto& e : cat.extras)
						if (e.name == "genre") e.options = str_array(jobj(c, "genres"));
				}
			}
			a.catalogs.push_back(cat);
		}
	}
	return true;
}

bool Addon::supports(const std::string& resource, const std::string& type, const std::string& id) const {
    if(provider_repository) {
        if(resource!="stream") return false;
        for(const auto& p:providers) if(p.enabled && (p.types.empty() || std::find(p.types.begin(),p.types.end(),type)!=p.types.end() ||
           (type=="series" && std::find(p.types.begin(),p.types.end(),"tv")!=p.types.end()))) return true;
        return false;
    }
	for (auto& r : resources) {
		if (r.name != resource) continue;
		const auto& types_ = r.types.empty() ? types : r.types;
		if (!types_.empty() && std::find(types_.begin(), types_.end(), type) == types_.end()) continue;
		const auto& prefixes = r.id_prefixes.empty() ? id_prefixes : r.id_prefixes;
		if (prefixes.empty() || id.empty()) return true;
		for (auto& p : prefixes)
			if (starts_with(id, p)) return true;
	}
	return false;
}

std::string Addon::resource_url(const std::string& resource, const std::string& type, const std::string& id,
                                const std::string& extra) const {
	std::string u = base + "/" + resource + "/" + url_encode(type) + "/" + url_encode(id);
	if (!extra.empty()) u += "/" + extra;
	return u + ".json" + query;
}

bool fetch_json(const std::string& url, json& out, std::string& err, long timeout_s,
                const std::atomic<bool>* cancel) {
	HttpResponse r = http_get(url, timeout_s, cancel);
	if (!r.ok()) {
		err = r.describe();
		return false;
	}
	out = json::parse(r.body, nullptr, false);
	if (out.is_discarded()) {
		err = "invalid JSON";
		return false;
	}
	return true;
}

bool fetch_addon(const std::string& url, Addon& out, std::string& err, const std::atomic<bool>* cancel, long timeout_s) {
	json m;
	if (!fetch_json(url, m, err, std::clamp(timeout_s, 1L, 15L), cancel)) return false;
	if (!parse_addon(m, url, out)) {
		err = "not an addon manifest";
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Metadata

Meta parse_meta(const json& j) {
	Meta m;
    for(const auto* field:{"trailers","trailerStreams"}) {
        if(jobj(j,field).is_array()) {
            for(const auto& row:j[field])if(row.is_object() && m.trailers.size()<24)m.trailers.push_back(row);
        }
    }
	m.id = jstr(j, "id", jstr(j, "imdb_id"));
	m.imdb_id = jstr(j, "imdb_id", jstr(j, "imdbId"));
    for(const char* key:{"moviedb_id","tmdb_id","tmdbId"}) {
        const auto& v=jobj(j,key);
        auto id=v.is_number_integer()?std::to_string(v.get<long long>()):v.is_string()?v.get<std::string>():"";
        if(!id.empty() && id!="0" && id.find_first_not_of("0123456789")==std::string::npos){m.tmdb_id=id;break;}
    }
	m.type = jstr(j, "type");
	m.name = jstr(j, "name");
	m.poster = jstr(j, "poster");
	m.background = jstr(j, "background");
	m.logo = jstr(j, "logo");
	m.description = jstr(j, "description");
	m.release_info = jstr(j, "releaseInfo", jstr(j, "year"));
	m.year = m.release_info;
	m.runtime = jstr(j, "runtime");
	m.imdb_rating = jstr(j, "imdbRating");
	m.genres = str_array(jobj(j, "genres"));
	if (m.genres.empty()) m.genres = str_array(jobj(j, "genre"));
	m.cast = str_array(jobj(j, "cast"));
	m.directors = str_array(jobj(j, "director"));
	// Newer addons use "links" with categories instead.
	const json& links = jobj(j, "links");
	if (links.is_array()) {
		bool want_genres = m.genres.empty(), want_cast = m.cast.empty(), want_dir = m.directors.empty();
		for (auto& l : links) {
			std::string cat = jstr(l, "category"), name = jstr(l, "name");
			if (name.empty()) continue;
			if (want_genres && cat == "Genres") m.genres.push_back(name);
			else if (want_cast && cat == "Cast") m.cast.push_back(name);
			else if (want_dir && (cat == "Directors" || cat == "Director")) m.directors.push_back(name);
		}
	}
	const json& vids = jobj(j, "videos");
	if (vids.is_array()) {
		for (auto& v : vids) {
			Video vid;
			vid.id = jstr(v, "id");
			if (vid.id.empty()) continue;
			vid.title = jstr(v, "title", jstr(v, "name"));
			vid.thumbnail = jstr(v, "thumbnail");
			vid.overview = jstr(v, "overview", jstr(v, "description"));
			vid.released = jstr(v, "released", jstr(v, "firstAired"));
			vid.season = int(jnum(v, "season", 0));
			vid.episode = int(jnum(v, "episode", jnum(v, "number", 0)));
			vid.imdb_season = int(jnum(v, "imdbSeason", 0));
			vid.imdb_episode = int(jnum(v, "imdbEpisode", 0));
			m.videos.push_back(vid);
		}
		std::stable_sort(m.videos.begin(), m.videos.end(), [](const Video& a, const Video& b) {
			if (a.season != b.season) return a.season < b.season;
			return a.episode < b.episode;
		});
	}
	return m;
}

std::vector<Stream> parse_streams(const json& j, const std::string& addon_name) {
	std::vector<Stream> out;
	const json& arr = jobj(j, "streams");
	if (!arr.is_array()) return out;
	for (auto& s : arr) {
		Stream st;
		st.addon = addon_name;
		st.name = jstr(s, "name", addon_name);
		st.description = jstr(s, "description", jstr(s, "title"));
		st.url = trim(jstr(s, "url"));
		st.info_hash = lower(jstr(s, "infoHash"));
		st.file_idx = int(jnum(s, "fileIdx", -1));
		const json& resolve = jobj(s, "clientResolve");
		if (resolve.is_object() && lower(jstr(resolve, "type")) == "debrid") {
			st.client_resolve = json::object();
			for (const char* field : {"type", "service", "infoHash", "magnetUri", "filename", "torrentName"})
				if (jobj(resolve, field).is_string()) st.client_resolve[field] = jstr(resolve, field).substr(0, 8192);
			st.client_resolve["isCached"] = jbool(resolve, "isCached");
			if (jobj(resolve, "fileIdx").is_number_integer()) st.client_resolve["fileIdx"] = resolve["fileIdx"];
		}
		st.sources = str_array(jobj(s, "sources"));
		const json& bh = jobj(s, "behaviorHints");
		st.filename = jstr(bh, "filename");
		if (st.client_resolve.is_object()) {
			if (st.info_hash.empty()) st.info_hash = lower(jstr(st.client_resolve, "infoHash"));
			if (st.file_idx < 0) st.file_idx = int(jnum(st.client_resolve, "fileIdx", -1));
			if (st.filename.empty()) st.filename = jstr(st.client_resolve, "filename");
		}
		st.video_size = std::max(0.0, jnum(bh, "videoSize"));
		st.binge_group = jstr(bh, "bingeGroup");
		st.not_web_ready = jbool(bh, "notWebReady");
		const json& req = jobj(jobj(bh, "proxyHeaders"), "request");
		if (req.is_object()) {
			for (auto it = req.begin(); it != req.end(); ++it)
				if (it.value().is_string()) st.request_headers.push_back(it.key() + ": " + it.value().get<std::string>());
		}
		if (starts_with(st.url, "magnet:")) {
			// magnet:?xt=urn:btih:<hash>
			size_t p = st.url.find("btih:");
			if (p != std::string::npos) {
				st.info_hash = lower(st.url.substr(p + 5, 40));
				st.url.clear();
			} else continue;
		}
		// A direct URL wins over an accompanying hash; unsupported external
		// protocols must not fall back to the torrent engine.
		if (!st.playable()) continue;
		out.push_back(st);
	}
	return out;
}

std::vector<SubtitleTrack> parse_subtitles(const json& j, const std::string& addon_name) {
	std::vector<SubtitleTrack> out;
	const json& arr = jobj(j, "subtitles");
	if (!arr.is_array()) return out;
	for (auto& s : arr) {
		SubtitleTrack t;
		t.id = jstr(s, "id");
		t.url = jstr(s, "url");
		t.lang = jstr(s, "lang");
		t.addon = addon_name;
		if (t.url.empty()) continue;
		t.label = language_name(t.lang);
		out.push_back(t);
	}
	return out;
}

// ---------------------------------------------------------------------------
// Languages

struct Lang {
	const char* code2;
	const char* code3;
	const char* alt3;
	const char* name;
};

static const Lang kLangs[] = {
	{"en", "eng", "", "English"},     {"el", "gre", "ell", "Greek"},     {"es", "spa", "", "Spanish"},
	{"fr", "fre", "fra", "French"},   {"de", "ger", "deu", "German"},    {"it", "ita", "", "Italian"},
	{"pt", "por", "", "Portuguese"},  {"ro", "rum", "ron", "Romanian"},  {"nl", "dut", "nld", "Dutch"},
	{"pl", "pol", "", "Polish"},      {"ru", "rus", "", "Russian"},      {"tr", "tur", "", "Turkish"},
	{"ar", "ara", "", "Arabic"},      {"he", "heb", "", "Hebrew"},       {"bg", "bul", "", "Bulgarian"},
	{"hr", "hrv", "", "Croatian"},    {"sr", "srp", "scc", "Serbian"},   {"cs", "cze", "ces", "Czech"},
	{"sk", "slo", "slk", "Slovak"},   {"sl", "slv", "", "Slovenian"},    {"hu", "hun", "", "Hungarian"},
	{"sv", "swe", "", "Swedish"},     {"no", "nor", "nob", "Norwegian"}, {"da", "dan", "", "Danish"},
	{"fi", "fin", "", "Finnish"},     {"et", "est", "", "Estonian"},     {"lv", "lav", "", "Latvian"},
	{"lt", "lit", "", "Lithuanian"},  {"uk", "ukr", "", "Ukrainian"},    {"zh", "chi", "zho", "Chinese"},
	{"ja", "jpn", "", "Japanese"},    {"ko", "kor", "", "Korean"},       {"hi", "hin", "", "Hindi"},
	{"th", "tha", "", "Thai"},        {"vi", "vie", "", "Vietnamese"},   {"id", "ind", "", "Indonesian"},
	{"ms", "may", "msa", "Malay"},    {"fa", "per", "fas", "Persian"},   {"sq", "alb", "sqi", "Albanian"},
	{"mk", "mac", "mkd", "Macedonian"}, {"bs", "bos", "", "Bosnian"},    {"ca", "cat", "", "Catalan"},
	{"pb", "pob", "", "Portuguese (BR)"},
};

static const Lang* find_lang(const std::string& code) {
	std::string c = lower(trim(code));
	if (c.empty()) return nullptr;
	for (auto& l : kLangs) {
		if (c == l.code2 || c == l.code3 || (l.alt3[0] && c == l.alt3) || c == lower(l.name)) return &l;
	}
	return nullptr;
}

std::string language_name(const std::string& code) {
	const Lang* l = find_lang(code);
	if (l) return l->name;
	return code.empty() ? "Unknown" : code;
}

std::string language_to_iso639_2(const std::string& code) {
	const Lang* l = find_lang(code);
	return l ? l->code3 : lower(trim(code));
}
