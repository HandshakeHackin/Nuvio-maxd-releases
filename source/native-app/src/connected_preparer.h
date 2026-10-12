// Short-lived original links and cancellable, bounded background preparation.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "connected_services.h"
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>

namespace debrid {
class PreparedLinks {
public:
    using Epoch = uint64_t;
    using Scope = uint64_t;
    using Resolver = std::function<Result(const ::Stream&, const Settings&, int, int, const std::atomic<bool>*)>;
    using Clock = std::function<double()>;
    enum class State { missing, preparing, ready, cooldown };
    explicit PreparedLinks(Resolver resolver = {}, Clock clock = now_seconds);
    // Changing credentials/preferences invalidates links and cancels old requests.
    void configure(const Settings& settings);
    Epoch epoch() const;
    Scope open_scope();
    void close_scope(Scope scope);
    void promote(const ::Stream& stream, int season, int episode);
    void cancel(const ::Stream& stream, int season, int episode);
    State state(const ::Stream& stream, int season, int episode) const;
    Result prepare(const ::Stream& stream, int season, int episode, Scope scope, Epoch epoch);
    Result resolve(const ::Stream& stream, int season, int episode, Epoch epoch,
                   const std::shared_ptr<std::atomic<bool>>& cancel = {});
    void shutdown();
    static constexpr double ttl_seconds = 300;
    static constexpr size_t max_links = 100;
private:
    struct Link { std::string url, filename; double size = 0, expires = 0, touched = 0; };
    struct Flight {
        Scope scope = 0;
        Epoch epoch = 0;
        bool foreground = false, done = false;
        std::shared_ptr<std::atomic<bool>> cancel;
        Result result;
        std::condition_variable cv;
    };
    std::string key(const ::Stream&, int, int) const;
    Result execute(const std::shared_ptr<Flight>&, const std::string&, const ::Stream&, const Settings&, int, int);
    Result cached(const ::Stream&, const Link&) const;
    void prune(double now);
    mutable std::mutex mutex_;
    Settings settings_;
    Epoch epoch_ = 0;
    Scope scope_ = 0;
    bool stopped_ = false;
    size_t background_active_ = 0;
    Resolver resolver_;
    Clock clock_;
    std::set<Scope> scopes_;
    std::map<std::string, Link> links_;
    std::map<std::string, double> failures_;
    std::map<std::string, std::shared_ptr<Flight>> flights_;
    std::deque<double> minute_starts_, hour_starts_;
};

// Focus first, then addon order. Never replace a candidate with a lower-quality file.
std::vector<::Stream> preparation_candidates(const std::vector<::Stream>& streams, const Settings& settings,
                                           int focused, const std::string& addon, int limit);
} // namespace debrid
