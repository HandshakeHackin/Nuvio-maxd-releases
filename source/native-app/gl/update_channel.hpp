// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstddef>
#include <string>
#include <array>
#include <chrono>
namespace sx {
class UpdateChannel {
public:
    ~UpdateChannel();
    bool open(const std::string& helper,const std::atomic<bool>& cancel,std::string& error,unsigned deadline_seconds=0);
    bool send(const void* data,std::size_t size,const std::atomic<bool>& cancel);
    bool line(std::string& result,const std::atomic<bool>& cancel);
    bool receive(void* data,std::size_t size,const std::atomic<bool>& cancel);
    void close();
private:
    bool active(const std::atomic<bool>& cancel) const;
    int socket_=-1;
    std::array<char,4096> input_{};std::size_t at_=0,end_=0;
    std::chrono::steady_clock::time_point deadline_=std::chrono::steady_clock::time_point::max();
};
}
