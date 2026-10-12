// Nuvio host services for EVO's decoder/presentation modules.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "evo_dovi.h"
#include "util.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/dovi_meta.h>

int g_pb_decode_fatal = 0;
int video_decode_done = 0;
void toast(const char* title, const char* text) { dlog("evo: %s: %s", title, text); }
void evo_boot_log(const char* format, ...) {
    char line[2048]; va_list args; va_start(args, format);
    vsnprintf(line, sizeof(line), format, args); va_end(args);
    dlog("evo: %s", line);
}
void pp_stage_bc_checkpoint(const char* stage, const char* text) { dlog("evo: %s %s", stage, text); }
void pp_stage_bc(const char* stage, const char* text) { pp_stage_bc_checkpoint(stage, text); }

// EVO's RPU parser mirrors FFmpeg 7.1.1 private structures; this app uses a
// different FFmpeg ABI. P5 is rejected before open. HDR10-compatible base
// layers keep their existing colour policy without invoking that parser.
int evo_dovi_stream_profile(const void* codecpar) {
    auto* p = static_cast<const AVCodecParameters*>(codecpar);
    if (!p) return 0;
    const auto* side = av_packet_side_data_get(p->coded_side_data, p->nb_coded_side_data, AV_PKT_DATA_DOVI_CONF);
    if (!side || side->size < sizeof(AVDOVIDecoderConfigurationRecord)) return 0;
    AVDOVIDecoderConfigurationRecord r{}; std::memcpy(&r, side->data, sizeof(r));
    return r.dv_profile;
}
int evo_dovi_stream_needs_reshape(const void* p) { return evo_dovi_stream_profile(p) == 5; }
void evo_dovi_params_default(evo_dovi_params* p) { if (p) std::memset(p, 0, sizeof(*p)); }
int evo_dovi_params_from_av(const void*, evo_dovi_params*) { return -1; }
evo_dovi_parser* evo_dovi_parser_open(const void*) { return nullptr; }
void evo_dovi_parser_close(evo_dovi_parser*) {}
void evo_dovi_parser_flush(evo_dovi_parser*) {}
int evo_dovi_parse_annexb(evo_dovi_parser*, const uint8_t*, int, evo_dovi_params*) { return -1; }
}
