// Portable Nuvio Watch Progress synchronization. Stream URLs stay on device.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "http.h"
#include "util.h"
#include <functional>

class WatchSync {
public:
    using Rpc = std::function<HttpResponse(const std::string&, const json&)>;
    WatchSync(const std::string& dir, const std::string& owner);
    const std::string& owner() const { return owner_; }
    json pull(const Rpc& rpc, double now);
    bool write(const json& changes, const Rpc& rpc, double now, int64_t updated, std::string& error);
    bool synced() const;
private:
    bool save();
    void synchronize(const Rpc& rpc, double now);
    std::string path_, owner_, client_;
    json state_;
    double retry_at_ = 0;
};

// Translate portable progress into the native Continue Watching model.
void apply_watch_progress(json& library, const json& progress);
