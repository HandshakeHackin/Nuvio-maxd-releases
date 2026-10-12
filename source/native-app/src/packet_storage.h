// Bounded reusable compressed-packet storage. Refcounted codec buffers survive seeks.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
struct AVPacket;
class PacketStorage {
public:
    explicit PacketStorage(size_t capacity=256u<<20);
    ~PacketStorage();
    bool store(AVPacket* packet); // retains the original packet if the slab is full
    uint64_t pooled() const;
    uint64_t fallbacks() const;
    size_t live_bytes() const;
private:
    struct State;
    std::shared_ptr<State> state_;
};
