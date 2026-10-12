// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../update/core.hpp"
#include "update_channel.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace sx {
class AppUpdate {
public:
    enum class Phase {idle,checking,current,available,downloading,staging,ready,closing,error,cancelled};
    struct Status {Phase phase=Phase::idle;std::string message;float progress=-1; nuvio_update::Release release;};
    ~AppUpdate();
    Status status() const;
    void check();
    void download(const std::string& base,const std::string& data);
    void install();
    void cancel();
    void shutdown();
    void finish_exit();
private:
    void start_worker(std::function<void()> work);
    void set(Phase phase,const std::string& message,float progress=-1);
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    Status state_;
    std::thread worker_;
    std::atomic<bool> cancel_{false},running_{false};
    bool install_=false;
    UpdateChannel channel_;
};
}
