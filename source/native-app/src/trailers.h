// On-demand trailer metadata and public YouTube playback responses.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "stremio.h"
#include "http.h"
#include <functional>
namespace trailers {
struct Candidate { std::string name, youtube_id, url; };
struct Playback { std::string url,error;std::vector<std::string> headers; };
using Request=std::function<HttpResponse(const std::string&,const std::string*)>;
std::string preferred_language(const std::string& preferences);
std::string youtube_id(const std::string& input);
bool https_url(const std::string& url);
json tmdb_videos(const Meta& meta,const std::string& key,const std::atomic<bool>* cancel=nullptr,const Request& request={},const std::string& language="en");
std::vector<Candidate> candidates(const Meta& meta,const json& tmdb,const std::string& language="en");
json embedded_player(const std::string& html);
Playback player_source(const json& response);
Playback resolve(const Candidate& candidate,const std::atomic<bool>* cancel=nullptr,const Request& request={});
inline bool records_history(bool trailer,const std::string& type) {return !trailer && type!="tv" && type!="torrent" && !type.starts_with("media-") && type!="iptv-movie" && type!="iptv-series";}
} // namespace trailers
