// Exercise production preparation/cache behavior with controlled provider replies.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "connected_preparer.h"
#include "../gl/stream_json.hpp"
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
namespace {
int tests = 0;
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void check(const char* name, const std::function<void()>& test) { test(); ++tests; std::cout << "PASS " << name << '\n'; }
::Stream stream(int id = 0, const std::string& provider = "torbox") {
    ::Stream s; s.name = "4K HDR10+ HEVC Atmos"; s.addon = "AIOStreams";
    s.client_resolve = {{"type", "debrid"}, {"service", provider}, {"infoHash", std::string(40, 'a')},
                        {"isCached", true}, {"fileIdx", id}};
    s.request_headers = {"Authorization: Bearer addon-only"}; return s;
}
debrid::Settings settings() { debrid::Settings s; s.enabled = true; s.torbox.key = "dummy-tb"; s.premiumize.key = "dummy-pm"; return s; }
debrid::Result resolved(const ::Stream& s) {
    debrid::Result r; r.ok = true; r.stream = s; r.stream.url = "https://cdn.invalid/original.mkv";
    r.stream.filename = "Original.2160p.HEVC.mkv"; r.stream.video_size = 20000000000.0;
    r.stream.request_headers.clear(); return r;
}
struct Fixture {
    double now = 0; int calls = 0;
    debrid::PreparedLinks links{[this](const auto& s, auto&, int, int, auto*) { ++calls; return resolved(s); }, [this] { return now; }};
    Fixture() { links.configure(settings()); }
    auto epoch() { return links.epoch(); }
    auto scope() { return links.open_scope(); }
};
struct Gate {
    std::mutex mutex; std::condition_variable cv; bool entered = false, released = false;
    std::atomic<int> calls{0}; std::atomic<bool> cancelled{false};
    debrid::Result run(const ::Stream& s, const std::atomic<bool>* cancel) {
        ++calls; std::unique_lock lock(mutex); entered = true; cv.notify_all();
        while (!released && !cancel->load()) cv.wait_for(lock, 2ms);
        cancelled = cancel->load(); return resolved(s);
    }
    bool started() { std::unique_lock lock(mutex); return cv.wait_for(lock, 2s, [this] { return entered; }); }
    void release() { std::lock_guard lock(mutex); released = true; cv.notify_all(); }
};
}
int main() {
try {
    check("prepared original link starts playback without another provider call", [] {
        Fixture f; const auto s = stream();
        expect(f.links.prepare(s, -1, -1, f.scope(), f.epoch()).ok, "preparation failed");
        const auto r = f.links.resolve(s, -1, -1, f.epoch());
        expect(r.ok && f.calls == 1 && r.stream.url == "https://cdn.invalid/original.mkv", "cache not reused");
        expect(r.stream.name == s.name && r.stream.filename == "Original.2160p.HEVC.mkv" && r.stream.video_size == 20000000000.0, "original encode changed");
    });
    check("cache keeps caller presentation and removes addon credentials from CDN requests", [] {
        Fixture f; auto s = stream(); f.links.prepare(s, -1, -1, f.scope(), f.epoch());
        s.name = "new addon formatting"; s.addon = "other addon";
        const auto r = f.links.resolve(s, -1, -1, f.epoch());
        expect(r.ok && f.calls == 1 && r.stream.name == s.name && r.stream.addon == s.addon && r.stream.request_headers.empty(), "stale formatting or leaked headers");
    });
    check("links expire at five minutes and manual playback obtains a fresh link", [] {
        Fixture f; const auto s = stream(); f.links.prepare(s, 1, 1, f.scope(), f.epoch()); f.now = 299.9;
        expect(f.links.state(s, 1, 1) == debrid::PreparedLinks::State::ready, "expired early");
        f.now = 300;
        expect(f.links.state(s, 1, 1) == debrid::PreparedLinks::State::missing, "expired link still Ready");
        expect(f.links.resolve(s, 1, 1, f.epoch()).ok && f.calls == 2, "expired link used");
    });
    check("resume preserves resolver instructions and uses the same prepared file", [] {
        Fixture f; const auto s = stream(); f.links.prepare(s, 1, 4, f.scope(), f.epoch());
        const auto saved = stream_to_json(s); const auto restored = stream_from_json(saved);
        expect(saved["url"].get<std::string>().empty(), "ephemeral link persisted");
        expect(f.links.resolve(restored, 1, 4, f.epoch()).ok && f.calls == 1, "resume cannot reuse instructions");
    });
    check("file index and exact filename do not share prepared links", [] {
        Fixture f; auto a = stream(); auto b = stream(1);
        f.links.resolve(a, -1, -1, f.epoch()); f.links.resolve(b, -1, -1, f.epoch());
        b.filename = "other encode.mkv"; f.links.resolve(b, -1, -1, f.epoch());
        expect(f.calls == 3, "different file identities collide");
    });
    check("next episode and season identities cannot reuse the current episode link", [] {
        Fixture f; const auto s = stream(); f.links.resolve(s, 1, 1, f.epoch());
        f.links.resolve(s, 1, 2, f.epoch()); f.links.resolve(s, 2, 1, f.epoch());
        expect(f.calls == 3, "episode keys collide");
    });
    check("provider identities stay separate for the same hash", [] {
        Fixture f; f.links.resolve(stream(), -1, -1, f.epoch()); f.links.resolve(stream(0, "premiumize"), -1, -1, f.epoch());
        expect(f.calls == 2, "different services share links");
    });
    check("credential changes discard cached links and stale queued preparation", [] {
        Fixture f; const auto old = f.epoch(), scope = f.scope(); f.links.prepare(stream(), -1, -1, scope, old);
        auto cfg = settings(); cfg.torbox.key = "other-dummy"; f.links.configure(cfg);
        expect(!f.links.prepare(stream(1), -1, -1, scope, old).ok && f.calls == 1, "stale job made requests");
        expect(!f.links.resolve(stream(), -1, -1, old).ok, "old credentials accepted");
        expect(f.links.resolve(stream(), -1, -1, f.epoch()).ok && f.calls == 2, "old account link reused");
    });
    check("provider preference and disconnected services invalidate eligibility", [] {
        Fixture f; f.links.resolve(stream(), -1, -1, f.epoch());
        auto cfg = settings(); cfg.preferred = "premiumize"; f.links.configure(cfg);
        expect(!f.links.resolve(stream(), -1, -1, f.epoch()).ok && f.calls == 1, "named service redirected");
        cfg.premiumize.key.clear(); f.links.configure(cfg);
        expect(!f.links.resolve(stream(0, "premiumize"), -1, -1, f.epoch()).ok, "disconnected key used");
    });
    check("turning instant playback off stops background work but permits manual resolution", [] {
        Fixture f; auto cfg = settings(); cfg.instant_limit = 0; f.links.configure(cfg);
        expect(!f.links.prepare(stream(), -1, -1, f.scope(), f.epoch()).ok && f.calls == 0, "background off ignored");
        expect(f.links.resolve(stream(), -1, -1, f.epoch()).ok && f.calls == 1, "manual resolution disabled");
    });
    check("direct addon URLs bypass preparation and keep their existing headers", [] {
        Fixture f; auto s = stream(); s.url = "https://cdn.invalid/direct.mp4";
        expect(!f.links.prepare(s, -1, -1, f.scope(), f.epoch()).ok && f.calls == 0, "direct URL prefetched");
        const auto r = f.links.resolve(s, -1, -1, f.epoch());
        expect(r.ok && r.stream.url == s.url && r.stream.request_headers == s.request_headers && f.calls == 0, "direct link changed");
    });
    check("uncached and unsupported instructions never contact providers in the background", [] {
        Fixture f; auto s = stream(); s.client_resolve["isCached"] = false;
        expect(!f.links.prepare(s, -1, -1, f.scope(), f.epoch()).ok, "uncached result accepted");
        s = stream(0, "unsupported"); expect(!f.links.prepare(s, -1, -1, f.scope(), f.epoch()).ok && f.calls == 0, "unsupported provider called");
    });
    check("background budget limits starts to six a minute while cache hits stay free", [] {
        Fixture f; const auto scope = f.scope();
        for (int i = 0; i < 6; ++i) expect(f.links.prepare(stream(i), -1, -1, scope, f.epoch()).ok, "budget too low");
        expect(!f.links.prepare(stream(6), -1, -1, scope, f.epoch()).ok && f.calls == 6, "minute budget exceeded");
        expect(f.links.prepare(stream(), -1, -1, scope, f.epoch()).ok && f.calls == 6, "cache hit spends budget");
        f.now = 60; expect(f.links.prepare(stream(6), -1, -1, scope, f.epoch()).ok, "minute budget did not recover");
    });
    check("hourly background budget recovers and manual play bypasses it", [] {
        Fixture f; const auto scope = f.scope();
        for (int i = 0; i < 30; ++i) { f.now = i * 61; expect(f.links.prepare(stream(i), -1, -1, scope, f.epoch()).ok, "unexpected budget stop"); }
        f.now = 1900; expect(!f.links.prepare(stream(30), -1, -1, scope, f.epoch()).ok && f.calls == 30, "hour budget exceeded");
        expect(f.links.resolve(stream(30), -1, -1, f.epoch()).ok && f.calls == 31, "manual play blocked by budget");
        f.now = 3600; expect(f.links.prepare(stream(31), -1, -1, scope, f.epoch()).ok, "hour budget did not recover");
    });
    check("cache evicts least recently used links at the one hundred link bound", [] {
        Fixture f;
        for (int i = 0; i < 100; ++i) { f.now = i; f.links.resolve(stream(i), -1, -1, f.epoch()); }
        f.now = 100; f.links.resolve(stream(), -1, -1, f.epoch());
        f.links.resolve(stream(100), -1, -1, f.epoch());
        expect(f.links.state(stream(), -1, -1) == debrid::PreparedLinks::State::ready, "hot entry evicted");
        expect(f.links.state(stream(1), -1, -1) == debrid::PreparedLinks::State::missing && f.calls == 101, "cache did not evict oldest");
    });
    check("closing a browsing scope cancels the provider and discards its link", [] {
        Gate gate; debrid::PreparedLinks links([&](const auto& s, auto&, int, int, auto* c) { return gate.run(s,c); }); links.configure(settings());
        const auto scope = links.open_scope(), epoch = links.epoch();
        auto job = std::async(std::launch::async, [&] { return links.prepare(stream(), 1, 1, scope, epoch); });
        const bool started = gate.started(); links.close_scope(scope); const auto r = job.get();
        expect(started && gate.cancelled && !r.ok && links.state(stream(), 1, 1) == debrid::PreparedLinks::State::missing, "cancelled link published");
        expect(!links.prepare(stream(1), 1, 1, scope, epoch).ok && gate.calls == 1, "queued scope survived closure");
    });
    check("one active background resolution bounds concurrent provider requests", [] {
        Gate gate; debrid::PreparedLinks links([&](const auto& s, auto&, int, int, auto* c) { return gate.run(s,c); }); links.configure(settings());
        const auto scope = links.open_scope(), epoch = links.epoch();
        auto job = std::async(std::launch::async, [&] { return links.prepare(stream(), -1, -1, scope, epoch); });
        const bool started = gate.started(); const auto second = links.prepare(stream(1), -1, -1, scope, epoch);
        gate.release(); const auto r = job.get(); expect(started && r.ok && !second.ok && gate.calls == 1, "background concurrency unbounded");
    });
    check("selected stream adopts the pending request across browsing closure", [] {
        Gate gate; debrid::PreparedLinks links([&](const auto& s, auto&, int, int, auto* c) { return gate.run(s,c); }); links.configure(settings());
        const auto scope = links.open_scope(), epoch = links.epoch();
        auto background = std::async(std::launch::async, [&] { return links.prepare(stream(), 1, 2, scope, epoch); });
        const bool started = gate.started(); links.promote(stream(), 1, 2); links.close_scope(scope);
        auto foreground = std::async(std::launch::async, [&] { return links.resolve(stream(), 1, 2, epoch); });
        const bool waiting = foreground.wait_for(20ms) == std::future_status::timeout;
        gate.release(); const auto a = background.get(), b = foreground.get();
        expect(started && waiting && a.ok && b.ok && gate.calls == 1 && !gate.cancelled, "selected request cancelled or duplicated");
    });
    check("cancelling playback wakes a waiter and interrupts adopted provider work", [] {
        Gate gate; debrid::PreparedLinks links([&](const auto& s, auto&, int, int, auto* c) { return gate.run(s,c); }); links.configure(settings());
        const auto scope = links.open_scope(), epoch = links.epoch();
        auto background = std::async(std::launch::async, [&] { return links.prepare(stream(), 1, 1, scope, epoch); });
        const bool started = gate.started(); auto cancel = std::make_shared<std::atomic<bool>>(false);
        auto foreground = std::async(std::launch::async, [&] { return links.resolve(stream(), 1, 1, epoch, cancel); });
        *cancel = true; const auto b = foreground.get(); links.cancel(stream(), 1, 1); const auto a = background.get();
        expect(started && !a.ok && !b.ok && gate.cancelled, "play cancellation lost");
    });
    check("cancellation before the queued play task also stops a promoted request", [] {
        Gate gate; debrid::PreparedLinks links([&](const auto& s, auto&, int, int, auto* c) { return gate.run(s,c); }); links.configure(settings());
        const auto scope = links.open_scope(), epoch = links.epoch();
        auto background = std::async(std::launch::async, [&] { return links.prepare(stream(), 1, 1, scope, epoch); });
        const bool started = gate.started(); links.promote(stream(),1,1); links.close_scope(scope); links.cancel(stream(),1,1);
        const auto a = background.get(); auto cancelled = std::make_shared<std::atomic<bool>>(true);
        expect(started && !a.ok && !links.resolve(stream(),1,1,epoch,cancelled).ok && gate.calls == 1, "queued selection leaked request");
    });
    check("settings changes cancel requests and wake pending playback", [] {
        Gate gate; debrid::PreparedLinks links([&](const auto& s, auto&, int, int, auto* c) { return gate.run(s,c); }); links.configure(settings());
        const auto scope = links.open_scope(), epoch = links.epoch();
        auto background = std::async(std::launch::async, [&] { return links.prepare(stream(),1,1,scope,epoch); });
        const bool started = gate.started();
        auto foreground = std::async(std::launch::async, [&] { return links.resolve(stream(),1,1,epoch); });
        links.configure(settings()); const auto a = background.get(), b = foreground.get();
        expect(started && !a.ok && !b.ok && gate.cancelled, "stale account work survived");
    });
    check("shutdown cancels provider work and rejects queued preparation", [] {
        Gate gate; debrid::PreparedLinks links([&](const auto& s, auto&, int, int, auto* c) { return gate.run(s,c); }); links.configure(settings());
        const auto scope = links.open_scope(), epoch = links.epoch();
        auto background = std::async(std::launch::async, [&] { return links.prepare(stream(),1,1,scope,epoch); });
        const bool started = gate.started(); links.shutdown(); const auto a = background.get();
        expect(started && !a.ok && gate.cancelled && !links.resolve(stream(),1,1,epoch).ok && links.open_scope() == 0, "shutdown allowed work");
    });
    check("provider failures cool down in the background while manual retry stays available", [] {
        double now = 0; int calls = 0;
        debrid::PreparedLinks links([&](const auto& s, auto&, int, int, auto*) { return ++calls == 1 ? debrid::Result{} : resolved(s); }, [&] { return now; });
        links.configure(settings()); const auto scope = links.open_scope(), epoch = links.epoch();
        expect(!links.prepare(stream(),-1,-1,scope,epoch).ok, "failure unexpectedly succeeds");
        expect(!links.prepare(stream(),-1,-1,scope,epoch).ok && calls == 1, "failure retried without cooldown");
        expect(links.resolve(stream(),-1,-1,epoch).ok && calls == 2, "manual retry blocked");
    });
    check("provider exceptions release flights and produce a sanitized retry message", [] {
        int calls = 0; debrid::PreparedLinks links([&](const auto& s, auto&, int, int, auto*) {
            if (++calls == 1) throw std::runtime_error("private credential");
            return resolved(s);
        }); links.configure(settings());
        const auto r = links.prepare(stream(),-1,-1,links.open_scope(),links.epoch());
        expect(!r.ok && r.error.find("private") == std::string::npos && links.resolve(stream(),-1,-1,links.epoch()).ok, "exception leaked or flight stuck");
    });
    check("a non HTTP provider result never becomes Ready or playable", [] {
        debrid::PreparedLinks links([](const auto& s, auto&, int, int, auto*) { auto r = resolved(s); r.stream.url = "magnet:?xt=unsafe"; return r; }); links.configure(settings());
        expect(!links.prepare(stream(),-1,-1,links.open_scope(),links.epoch()).ok && links.state(stream(),-1,-1) != debrid::PreparedLinks::State::ready, "invalid link cached");
    });
    check("focus is prepared first and addon filters, duplicate identities and caps are respected", [] {
        auto other = stream(4); other.addon = "PenguPlay"; auto direct = stream(5); direct.url = "https://cdn.invalid/direct.mp4";
        std::vector<::Stream> list{stream(), stream(1), stream(1), stream(2), other, direct};
        const auto picked = debrid::preparation_candidates(list, settings(), 3, "AIOStreams", 3);
        expect(picked.size() == 3 && jnum(picked[0].client_resolve,"fileIdx") == 2 && jnum(picked[1].client_resolve,"fileIdx") == 0 && jnum(picked[2].client_resolve,"fileIdx") == 1, "focus/order/filter changed");
        expect(debrid::preparation_candidates(list,settings(),3,"",0).empty(), "Off still yields candidates");
        auto cfg = settings(); cfg.preferred = "premiumize";
        expect(debrid::preparation_candidates(list,cfg,3,"",5).empty(), "preferred provider ignored");
    });
    std::cout << tests << " Instant playback tests passed\n"; return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
