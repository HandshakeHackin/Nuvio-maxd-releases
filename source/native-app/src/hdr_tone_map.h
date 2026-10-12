// Metadata policy for the explicitly selected HDR-to-SDR conversion.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>

inline float hdr_peak_nits(double content_peak, double mastering_peak) {
    const auto valid = [](double x) { return std::isfinite(x) && x >= 100 && x <= 10000; };
    if (valid(content_peak)) return float(content_peak);
    if (valid(mastering_peak)) return float(mastering_peak);
    return 1000.0f; // conservative default when the stream does not describe its peak
}
