// Bounded background logging: storage and kernel-log stalls never run on a
// playback/UI producer. Inspired by ProsperoEden's log-flusher architecture
// (8ff2c6a); implementation specific to Nuvio. GPL-3.0-or-later.
#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class BufferedLog {
public:
    using Sink = std::function<void(const std::vector<std::string>&)>;
    explicit BufferedLog(Sink sink, size_t capacity = 1024)
        : sink_(std::move(sink)), capacity_(capacity), worker_([this] { run(); }) {}
    ~BufferedLog() { close(); }
    bool push(std::string line) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return false;
        if (queue_.size() >= capacity_) { ++lost_; cv_.notify_one(); return false; }
        queue_.push_back(std::move(line)); cv_.notify_one(); return true;
    }
    void close() {
        { std::lock_guard<std::mutex> lock(mutex_); stopped_ = true; cv_.notify_one(); }
        if (worker_.joinable()) worker_.join();
    }
private:
    void run() {
        for (;;) {
            std::vector<std::string> batch;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stopped_ || !queue_.empty() || lost_; });
                if (queue_.empty() && !lost_ && stopped_) return;
                if (lost_) {
                    batch.push_back("log: " + std::to_string(lost_) + " messages omitted while the log sink was busy");
                    lost_ = 0;
                }
                while (!queue_.empty() && batch.size() < 64) {
                    batch.push_back(std::move(queue_.front())); queue_.pop_front();
                }
            }
            sink_(batch); // never hold the producer lock during I/O
        }
    }
    Sink sink_;
    const size_t capacity_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    size_t lost_ = 0;
    bool stopped_ = false;
    std::thread worker_;
};
