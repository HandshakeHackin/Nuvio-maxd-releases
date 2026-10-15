#pragma once
#include <array>
#include <algorithm>
#include <cstdint>

namespace sx {
// UI-thread tracker. A picture is sampled only on its first submitted flip.
// Measures console selection-to-scanout delay, not TV or receiver latency.
class PresentationTiming {
public:
    struct Picture { uint64_t id=0; int64_t selected_us=0; };
    void reset() { *this = PresentationTiming{}; }
    void submit(int64_t marker, Picture picture) {
        if (!picture.id || picture.id == last_id_ || picture.selected_us <= 0) return;
        pending_[cursor_++ % pending_.size()] = {marker, picture};
        last_id_ = picture.id;
    }
    bool complete(int64_t marker, int64_t flip_us, int64_t now_us) {
        if (flip_us <= 0 || flip_us > now_us || now_us-flip_us > 1000000) return false;
        for (auto& entry : pending_) {
            if (!entry.picture.id || entry.marker != marker) continue;
            const auto selected = entry.picture.selected_us;
            entry.picture = {}; // never count a redraw or repeat status twice
            const auto delay = flip_us-selected;
            if (delay < 0 || delay > 100000) return false;
            last_delay_us_ = delay;
            mean_us_ = samples_ ? mean_us_*0.875 + delay*0.125 : double(delay);
            ++samples_; last_sample_us_=now_us;
            return true;
        }
        return false;
    }
    double lead_seconds(int64_t now_us) const {
        if (samples_ < 3 || now_us < last_sample_us_ || now_us-last_sample_us_ > 1000000) return 0;
        return std::clamp(mean_us_,0.0,40000.0)/1000000.0;
    }
    uint64_t samples() const { return samples_; }
    int64_t last_delay_us() const { return last_delay_us_; }
private:
    struct Pending { int64_t marker=0; Picture picture; };
    std::array<Pending,64> pending_{};
    uint64_t cursor_=0, last_id_=0, samples_=0;
    int64_t last_sample_us_=0, last_delay_us_=0;
    double mean_us_=0;
};
// Keep all timestamp arithmetic guarded; VideoOut uses process microseconds.
inline int64_t flip_monotonic_us(uint64_t flip, uint64_t process_now, int64_t monotonic_now) {
    if (!flip || flip>process_now || process_now-flip>1000000 || monotonic_now<=0 ||
        process_now-flip>=uint64_t(monotonic_now)) return 0;
    return monotonic_now-int64_t(process_now-flip);
}
namespace presentation {
void reset();
void arm(PresentationTiming::Picture picture);
void disarm();
double poll();
}
}
