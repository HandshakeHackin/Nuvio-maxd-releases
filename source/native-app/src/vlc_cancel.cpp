// One active player; old segment requests stay cancelled after a new open.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "vlc_cancel.h"
#include <atomic>
#include <limits>
namespace {
std::atomic<long> active{0};
std::atomic<unsigned long> sequence{0};
}
extern "C" long nuvio_vlc_stream_begin() {
    const auto n = sequence.fetch_add(1) % static_cast<unsigned long>(std::numeric_limits<long>::max()) + 1;
    active.store(static_cast<long>(n));
    return static_cast<long>(n);
}
extern "C" void nuvio_vlc_stream_end(long serial) {
    if (serial > 0) active.compare_exchange_strong(serial, 0);
}
extern "C" int nuvio_vlc_stream_cancelled(long serial) {
    // Zero is allowed for callers outside our player integration.
    return serial > 0 && active.load() != serial;
}
