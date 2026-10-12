#pragma once
#include "../ui/subtitle_style.hpp"
#include "json.hpp"
#include "install_source.hpp"
#include <algorithm>
#include <cmath>
#include <string>
namespace sx {
struct ExperiencePreferences {
    std::string seen_release;
    std::string install_source; // user-selected, local only; empty means not asked yet
    SubtitleStyle subtitle_style;
    int seek_seconds = 10, shoulder_seek_seconds = 60, next_episode_seconds = 10;
    bool surround_audio = true;
    std::string audio_output_mode="pcm";
    bool reduced_motion = false, high_contrast = false;
};
inline int preference_int(const nlohmann::json& j, const char* key, int fallback, int lo, int hi) {
    const auto i=j.find(key);
    if(i==j.end() || !i->is_number()) return fallback;
    const double v=i->get<double>();
    return std::isfinite(v) ? int(std::clamp(v,double(lo),double(hi))) : fallback;
}
inline void load_experience(const nlohmann::json& j, ExperiencePreferences& p) {
    if(j.contains("install_source") && j["install_source"].is_string()) {
        const auto source=j["install_source"].get<std::string>();
        if(valid_install_source(source)) p.install_source=source;
    }
    if(j.contains("seen_release") && j["seen_release"].is_string()) {
        const auto value=j["seen_release"].get<std::string>(); if(value.size()<=40) p.seen_release=value;
    }
    if(j.contains("audio_output_mode") && j["audio_output_mode"].is_string()) {
        const auto mode=j["audio_output_mode"].get<std::string>();
        if(mode=="pcm" || mode=="auto" || mode=="bitstream")p.audio_output_mode=mode;
    }
    if(j.contains("surround_audio") && j["surround_audio"].is_boolean()) p.surround_audio=j["surround_audio"];
    p.subtitle_style.font=preference_int(j,"subtitle_font",1,0,2);
    p.subtitle_style.colour=preference_int(j,"subtitle_colour",0,0,2);
    p.subtitle_style.effect=preference_int(j,"subtitle_effect",1,0,2);
    p.subtitle_style.background=preference_int(j,"subtitle_background",0,0,100);
    p.seek_seconds=preference_int(j,"seek_seconds",10,5,60);
    p.shoulder_seek_seconds=preference_int(j,"shoulder_seek_seconds",60,5,300);
    p.next_episode_seconds=preference_int(j,"next_episode_seconds",10,5,120);
    if(j.contains("reduced_motion") && j["reduced_motion"].is_boolean()) p.reduced_motion=j["reduced_motion"];
    if(j.contains("high_contrast") && j["high_contrast"].is_boolean()) p.high_contrast=j["high_contrast"];
}
inline void save_experience(nlohmann::json& j, const ExperiencePreferences& p) {
    j["surround_audio"]=p.surround_audio;
    j["audio_output_mode"]=p.audio_output_mode;
    j["seen_release"]=p.seen_release;
    j["install_source"]=p.install_source;
    j["subtitle_font"]=p.subtitle_style.font; j["subtitle_colour"]=p.subtitle_style.colour;
    j["subtitle_effect"]=p.subtitle_style.effect; j["subtitle_background"]=p.subtitle_style.background;
    j["seek_seconds"]=p.seek_seconds; j["shoulder_seek_seconds"]=p.shoulder_seek_seconds;
    j["next_episode_seconds"]=p.next_episode_seconds;
    j["reduced_motion"]=p.reduced_motion; j["high_contrast"]=p.high_contrast;
}
}
