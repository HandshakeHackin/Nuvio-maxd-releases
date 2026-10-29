#pragma once
#include "hevc_colour.hpp"
extern "C" {
#include <libavcodec/codec_par.h>
}
inline bool recover_hevc_colour(AVCodecParameters* p) {
    if(!p || p->codec_id!=AV_CODEC_ID_HEVC || p->extradata_size<=0)return false;
    const auto c=hevc_colour::extradata(p->extradata,size_t(p->extradata_size));
    if(!c)return false;
    bool changed=false;
    if(p->color_trc==AVCOL_TRC_UNSPECIFIED && c->transfer!=2 && c->transfer<AVCOL_TRC_NB) {p->color_trc=AVColorTransferCharacteristic(c->transfer);changed=true;}
    if(p->color_primaries==AVCOL_PRI_UNSPECIFIED && c->primaries!=2 && c->primaries<AVCOL_PRI_NB) {p->color_primaries=AVColorPrimaries(c->primaries);changed=true;}
    if(p->color_space==AVCOL_SPC_UNSPECIFIED && c->matrix!=2 && c->matrix<AVCOL_SPC_NB) {p->color_space=AVColorSpace(c->matrix);changed=true;}
    if(p->color_range==AVCOL_RANGE_UNSPECIFIED) {p->color_range=c->full?AVCOL_RANGE_JPEG:AVCOL_RANGE_MPEG;changed=true;}
    return changed;
}
