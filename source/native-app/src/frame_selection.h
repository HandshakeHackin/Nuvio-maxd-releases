// Timestamp-ordered, bounded catch-up for fixed-refresh video. Adapted from
// ProsperoLight 01.000.095 pacing principles (91c0cf8): isolated lateness must
// not discard a run of useful pictures. Nuvio retains its audio-master clock.
// GPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <cmath>
template<class Frames>
int select_video_frame(const Frames& frames, double clock, bool have_picture,
                       bool held, double fps) {
    if (frames.empty() || !std::isfinite(clock)) return -1;
    if (!have_picture) return 0;
    if (held || frames.front().pts > clock) return -1;
    // Preserve due frames during ordinary timer jitter. Only discard when a
    // successor is due and this picture missed an entire source interval.
    const double interval = fps > 0 && std::isfinite(fps) ? 1.0 / fps : 1.0 / 24;
    const double late = std::clamp(interval, 0.008, 0.100);
    int pick = 0;
    while (pick + 1 < int(frames.size()) && frames[pick + 1].pts <= clock &&
           clock - frames[pick].pts > late)
        ++pick;
    return pick;
}
