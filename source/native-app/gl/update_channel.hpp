// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstddef>
#include <string>
namespace sx {
class UpdateChannel {
public:
    ~UpdateChannel();
    bool open(const std::string& helper,const std::atomic<bool>& cancel,std::string& error);
    bool send(const void* data,std::size_t size,const std::atomic<bool>& cancel);
    bool line(std::string& result,const std::atomic<bool>& cancel);
    void close();
private: int socket_=-1;
};
}
