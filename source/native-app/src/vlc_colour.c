// Adapted to the VLC-PS5 vmem colour hook (GPL-2.0-or-later).
// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "vlc_colour.h"
#include <vlc_common.h>
#include <vlc_es.h>
// vmem's opaque is the application backend; one registered conversion callback
// is used by the one active player. Set only before playback begins.
static nuvio_vlc_colour_cb output;
static void colour(void *opaque, const video_format_t *fmt) {
    if (!output)
        return;
    struct nuvio_vlc_colour c = {0,
                                 1,
                                 fmt->b_color_range_full,
                                 0,
                                 1000,
                                 fmt->i_sar_num,
                                 fmt->i_sar_den,
                                 fmt->i_visible_width,
                                 fmt->i_visible_height};
    c.transfer = fmt->transfer == TRANSFER_FUNC_SMPTE_ST2084 ? 1 : fmt->transfer == TRANSFER_FUNC_HLG ? 2 : 0;
    c.matrix = fmt->space == COLOR_SPACE_BT601 ? 0 : fmt->space == COLOR_SPACE_BT2020 ? 2 : 1;
    c.compatible =
        c.transfer == 1 && fmt->space == COLOR_SPACE_BT2020 && fmt->primaries == COLOR_PRIMARIES_BT2020;
    if (fmt->lighting.MaxCLL >= 100)
        c.peak = fmt->lighting.MaxCLL;
    else if (fmt->mastering.max_luminance >= 1000000)
        c.peak = fmt->mastering.max_luminance / 10000.f;
    if (c.peak > 10000)
        c.peak = 10000;
    output(opaque, &c);
}
void nuvio_vlc_colour_install(void *player, nuvio_vlc_colour_cb cb) {
    output = cb;
    var_Create((vlc_object_t *)player, "vmem-colour", VLC_VAR_ADDRESS);
    var_SetAddress((vlc_object_t *)player, "vmem-colour", colour);
}
