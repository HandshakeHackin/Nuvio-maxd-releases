// HLS language choices must remain in the selected video's rendition group.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
extern "C" {
#include <libavformat/avformat.h>
}
#include <cstring>
namespace playback_streams {
inline bool is_hls(const AVFormatContext* fmt) {
    return fmt && fmt->iformat && fmt->iformat->name && !std::strcmp(fmt->iformat->name,"hls");
}
inline bool compatible_audio(const AVFormatContext* fmt,int video,int audio) {
    if (!is_hls(fmt) || video < 0) return true;
    bool video_program = false;
    for (unsigned i=0;i<fmt->nb_programs;++i) {
        const AVProgram* program=fmt->programs[i]; bool has_video=false,has_audio=false;
        for (unsigned j=0;j<program->nb_stream_indexes;++j) {
            has_video |= int(program->stream_index[j])==video;
            has_audio |= int(program->stream_index[j])==audio;
        }
        if (has_video && has_audio) return true;
        video_program |= has_video;
    }
    // Some demuxers expose no grouping. Preserve their ordinary track choices.
    return !video_program;
}
}
