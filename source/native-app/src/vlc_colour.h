// Small C boundary for the PS5 port's private vmem color callback.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct nuvio_vlc_colour {
    int transfer, matrix, full, compatible;
    float peak;
    unsigned sar_num, sar_den, visible_width, visible_height;
};
typedef void (*nuvio_vlc_colour_cb)(void *, const struct nuvio_vlc_colour *);
void nuvio_vlc_colour_install(void *player, nuvio_vlc_colour_cb cb);
#ifdef __cplusplus
}
#endif
