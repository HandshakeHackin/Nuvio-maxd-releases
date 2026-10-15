#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace sx {
// ICC channels are blue/white/amber, not RGB. No kernel patch or payload.
class ConsoleLights {
public:
    enum class Status { idle, active, unavailable, system_owned };
    ConsoleLights();
    ~ConsoleLights();
    void update(bool enabled, uint8_t red,uint8_t green,uint8_t blue);
    void stop();
    Status status() const { return status_.load(); }
private:
    void run();
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool quit_=false, enabled_=false;
    uint64_t generation_=0;
    std::array<uint8_t,3> requested_{};
    std::atomic<Status> status_{Status::idle};
};
}
