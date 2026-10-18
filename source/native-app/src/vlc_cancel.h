// Per-playback cancellation shared by the access module and adaptive demux.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
long nuvio_vlc_stream_begin(void);
void nuvio_vlc_stream_end(long serial);
int nuvio_vlc_stream_cancelled(long serial);
#ifdef __cplusplus
}
#endif
