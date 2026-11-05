// Fallback changes picture conversion only after SDR scanout is restored.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace sx {
template<class Restore, class Convert>
bool try_hdr_fallback(bool allowed, Restore restore, Convert convert) {
    if (!allowed || !restore()) return false;
    convert();
    return true;
}
}
