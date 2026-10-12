// SPDX-License-Identifier: GPL-3.0-or-later
#include "connected_preparer.h"
#include <algorithm>
#include <chrono>

namespace debrid {
namespace {
Result skipped(const char* reason) { Result r; r.error = reason; return r; }
std::string identity(const ::Stream& s, const Settings& settings, int season, int episode) {
    // Keys deliberately exclude credentials and playable URLs; configure() scopes them to an account.
    return json::array({s.client_resolve.is_object() ? provider_id(jstr(s.client_resolve, "service")) : settings.active_provider(),
        lower(jstr(s.client_resolve, "infoHash", s.info_hash)), jstr(s.client_resolve, "magnetUri"),
        jnum(s.client_resolve, "fileIdx", s.file_idx), jstr(s.client_resolve, "filename", s.filename), season, episode}).dump();
}
} // namespace
PreparedLinks::PreparedLinks(Resolver resolver, Clock clock) : resolver_(std::move(resolver)), clock_(std::move(clock)) {
    if (!resolver_) resolver_ = [](const auto& s, const auto& cfg, int season, int episode, const auto* cancel) {
        return Client().resolve(s, cfg, season, episode, cancel);
    };
}
void PreparedLinks::configure(const Settings& settings) {
    std::lock_guard lock(mutex_);
    ++epoch_; settings_ = settings;
    for (auto& [id, f] : flights_) { (void)id; *f->cancel = true; f->cv.notify_all(); }
    flights_.clear(); links_.clear(); failures_.clear(); scopes_.clear();
}
PreparedLinks::Epoch PreparedLinks::epoch() const { std::lock_guard lock(mutex_); return epoch_; }
PreparedLinks::Scope PreparedLinks::open_scope() {
    std::lock_guard lock(mutex_);
    if (stopped_) return 0;
    scopes_.insert(++scope_); return scope_;
}
void PreparedLinks::close_scope(Scope scope) {
    std::lock_guard lock(mutex_);
    scopes_.erase(scope);
    for (auto& [id, f] : flights_) if (f->scope == scope && !f->foreground) { (void)id; *f->cancel = true; f->cv.notify_all(); }
}
std::string PreparedLinks::key(const ::Stream& s, int season, int episode) const { return identity(s, settings_, season, episode); }
void PreparedLinks::promote(const ::Stream& s, int season, int episode) {
    std::lock_guard lock(mutex_);
    const auto f = flights_.find(key(s, season, episode));
    if (f != flights_.end() && !f->second->cancel->load()) f->second->foreground = true;
}
void PreparedLinks::cancel(const ::Stream& s, int season, int episode) {
    std::lock_guard lock(mutex_);
    const auto f = flights_.find(key(s, season, episode));
    if (f != flights_.end()) { *f->second->cancel = true; f->second->cv.notify_all(); }
}
PreparedLinks::State PreparedLinks::state(const ::Stream& s, int season, int episode) const {
    std::lock_guard lock(mutex_);
    if (stopped_ || !settings_.can_resolve(s)) return State::missing;
    const auto id = key(s, season, episode); const double now = clock_();
    const auto l = links_.find(id);
    if (l != links_.end() && l->second.expires > now) return State::ready;
    const auto f = flights_.find(id);
    if (f != flights_.end() && !f->second->cancel->load()) return State::preparing;
    const auto failed = failures_.find(id);
    return failed != failures_.end() && failed->second > now ? State::cooldown : State::missing;
}
Result PreparedLinks::cached(const ::Stream& original, const Link& link) const {
    Result r; r.ok = true; r.stream = original;
    r.stream.url = link.url; r.stream.filename = link.filename; r.stream.video_size = link.size;
    r.stream.request_headers.clear(); return r;
}
void PreparedLinks::prune(double now) {
    for (auto it = links_.begin(); it != links_.end();) it = it->second.expires <= now ? links_.erase(it) : std::next(it);
    for (auto it = failures_.begin(); it != failures_.end();) it = it->second <= now ? failures_.erase(it) : std::next(it);
    while (!minute_starts_.empty() && minute_starts_.front() <= now - 60) minute_starts_.pop_front();
    while (!hour_starts_.empty() && hour_starts_.front() <= now - 3600) hour_starts_.pop_front();
}
Result PreparedLinks::execute(const std::shared_ptr<Flight>& f, const std::string& id, const ::Stream& stream,
                              const Settings& cfg, int season, int episode) {
    Result r;
    try { r = resolver_(stream, cfg, season, episode, f->cancel.get()); }
    catch (...) { r = skipped("The service could not prepare this stream. Select it to retry."); }
    if (r.ok && !r.stream.direct_http()) r = skipped("The service did not return a playable original-file link.");
    std::lock_guard lock(mutex_);
    if (f->scope) --background_active_;
    const auto current = flights_.find(id);
    if (current != flights_.end() && current->second == f) flights_.erase(current);
    const bool valid = !stopped_ && f->epoch == epoch_ && !f->cancel->load() &&
                       (f->foreground || scopes_.count(f->scope));
    if (!valid) r = skipped("cancelled");
    const double now = clock_(); prune(now);
    if (valid && r.ok && r.stream.direct_http()) {
        while (links_.size() >= max_links) {
            const auto oldest = std::min_element(links_.begin(), links_.end(), [](const auto& a, const auto& b) { return a.second.touched < b.second.touched; });
            links_.erase(oldest);
        }
        links_[id] = {r.stream.url, r.stream.filename, r.stream.video_size, now + ttl_seconds, now};
        failures_.erase(id);
    } else if (valid && !r.ok && r.error != "cancelled") {
        while (failures_.size() >= max_links) failures_.erase(failures_.begin());
        failures_[id] = now + 30;
    }
    f->result = r; f->done = true; f->cv.notify_all(); return r;
}
Result PreparedLinks::prepare(const ::Stream& stream, int season, int episode, Scope scope, Epoch epoch) {
    std::unique_lock lock(mutex_);
    if (stopped_ || epoch != epoch_ || !scopes_.count(scope) || settings_.instant_limit <= 0 || !settings_.can_resolve(stream)) return skipped("cancelled");
    const double now = clock_(); prune(now); const auto id = key(stream, season, episode);
    if (auto l = links_.find(id); l != links_.end()) { l->second.touched = now; return cached(stream, l->second); }
    if (flights_.count(id) || background_active_ || failures_.count(id)) return skipped("preparation busy");
    if (minute_starts_.size() >= 6 || hour_starts_.size() >= 30) return skipped("preparation budget reached");
    minute_starts_.push_back(now); hour_starts_.push_back(now);
    auto f = std::make_shared<Flight>(); f->scope = scope; f->epoch = epoch;
    f->cancel = std::make_shared<std::atomic<bool>>(false); flights_[id] = f; ++background_active_;
    const auto cfg = settings_; lock.unlock();
    return execute(f, id, stream, cfg, season, episode);
}
Result PreparedLinks::resolve(const ::Stream& stream, int season, int episode, Epoch epoch,
                             const std::shared_ptr<std::atomic<bool>>& cancel) {
    std::unique_lock lock(mutex_);
    if (stopped_ || epoch != epoch_ || (cancel && cancel->load())) return skipped("cancelled");
    if (stream.direct_http()) { Result r; r.ok = true; r.stream = stream; return r; }
    if (!settings_.can_resolve(stream)) return skipped("Connect the stream's service in Connected Services.");
    const double now = clock_(); prune(now); const auto id = key(stream, season, episode);
    if (auto l = links_.find(id); l != links_.end()) { l->second.touched = now; return cached(stream, l->second); }
    if (auto found = flights_.find(id); found != flights_.end() && !found->second->cancel->load()) {
        const auto f = found->second; f->foreground = true;
        while (!f->done) {
            if (stopped_ || epoch != epoch_ || (cancel && cancel->load())) { *f->cancel = true; return skipped("cancelled"); }
            f->cv.wait_for(lock, std::chrono::milliseconds(50));
        }
        if (epoch != epoch_ || (cancel && cancel->load())) return skipped("cancelled");
        if (!f->result.ok) return f->result;
        const auto& result = f->result.stream;
        return cached(stream, {result.url, result.filename, result.video_size, 0, 0});
    }
    auto f = std::make_shared<Flight>(); f->epoch = epoch; f->foreground = true;
    f->cancel = cancel ? cancel : std::make_shared<std::atomic<bool>>(false); flights_[id] = f;
    const auto cfg = settings_; lock.unlock();
    return execute(f, id, stream, cfg, season, episode);
}
void PreparedLinks::shutdown() {
    std::lock_guard lock(mutex_); stopped_ = true; ++epoch_;
    for (auto& [id, f] : flights_) { (void)id; *f->cancel = true; f->cv.notify_all(); }
    flights_.clear(); links_.clear(); failures_.clear(); scopes_.clear(); settings_ = {};
}
std::vector<::Stream> preparation_candidates(const std::vector<::Stream>& streams, const Settings& settings,
                                           int focused, const std::string& addon, int limit) {
    std::vector<::Stream> out; std::set<std::string> seen;
    const auto add = [&](int i) {
        if (i < 0 || static_cast<size_t>(i) >= streams.size() || static_cast<int>(out.size()) >= std::clamp(limit, 0, 5)) return;
        const auto& s = streams[i];
        if ((!addon.empty() && s.addon != addon) || !settings.can_resolve(s)) return;
        if (seen.insert(identity(s, settings, -1, -1)).second) out.push_back(s);
    };
    add(focused);
    for (size_t i = 0; i < streams.size(); ++i) add(static_cast<int>(i));
    return out;
}
} // namespace debrid
