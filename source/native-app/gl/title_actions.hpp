// Explicit manual watched scopes; never infer unwatched/future episode identities.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "stremio.h"
#include "airtime.hpp"
#include <set>
namespace sx::title_actions {
inline json episodes(const Meta& meta,const std::string& content,int season,bool marking,
                     const std::set<std::string>& watched,int64_t now) {
    json out=json::array();std::set<std::pair<int,int>> seen;
    for(const auto& video:meta.videos) {
        if(video.id.empty()||video.season<0||video.episode<1||(season>=0&&display_season(video)!=season))continue;
        const auto canonical=content+":"+std::to_string(video.season)+":"+std::to_string(video.episode);
        const auto released=airtime::iso(video.released);
        const bool marked=watched.count(canonical)||watched.count(video.id);
        if((!released||*released>now/1000) && (marking||!marked))continue;
        if(!seen.insert({video.season,video.episode}).second)continue;
        out.push_back({{"content_id",content},{"content_type","series"},{"title",meta.name},
            {"season",video.season},{"episode",video.episode},{"video_id",video.id}});
        if(out.size()>5000)return json(); // Reject the whole scope, never silently truncate it.
    }
    return out;
}
inline std::string video_id(const json& row) {
    auto id=jstr(row,"content_id");
    if(jstr(row,"content_type")=="series")id+=":"+std::to_string(int(jnum(row,"season")))+":"+std::to_string(int(jnum(row,"episode")));
    return id;
}
}
