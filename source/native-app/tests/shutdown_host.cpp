// Production Session::shutdown body, with fake lifecycle boundaries only.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace sx {
std::vector<std::string> events;
struct Boundary {
    std::string name;
    void shutdown() { events.push_back(name); }
    void close() { events.push_back(name); }
    void stop() { events.push_back(name); }
};
namespace bt {
struct Engine : Boundary {
    Engine() : Boundary{"torrent"} {}
    static Engine& get() { static Engine engine; return engine; }
};
}
Boundary g_tasks{"tasks"};
struct Updater {
    bool closing = false, handed_off = false;
    void shutdown() { events.push_back("updater joined"); }
    void finish_exit() {
        if (closing) {
            assert(!events.empty() && events.back() == "progress saved");
            handed_off = true;
            events.push_back("exit handed off");
        }
    }
};
struct Session {
    #include "shutdown_fields.hpp"
    std::shared_ptr<Boundary> prepared_ = std::make_shared<Boundary>(Boundary{"prepared"});
    Updater updater_;
    bool watching = false;
    Boundary player_{"player"}, provider_tasks_{"providers"}, tex_{"textures"};
    void watch_stop(bool) { events.push_back("playback stopped"); watching = false; }
    void save_progress() { events.push_back("progress saved"); }
    void shutdown();
};
#include "shutdown_body.hpp"
}

int main() {
    for (bool updating : {false, true}) {
        sx::events.clear();
        sx::Session session;
        // Same lazy cancellation defaults as a real fresh-launch Session.
        assert(!session.w_episode_cancel_);
        session.updater_.closing = updating;
        session.shutdown();
        assert(sx::events.back() == "progress saved");
        assert(!session.updater_.handed_off);
        session.updater_.finish_exit();
        assert(session.updater_.handed_off == updating);
    }
    sx::events.clear();
    sx::Session session;
    session.device_cancel_ = std::make_shared<std::atomic<bool>>(false);
    session.w_episode_cancel_ = std::make_shared<std::atomic<bool>>(false);
    const auto episode_cancel = session.w_episode_cancel_;
    session.watching = true;
    session.updater_.closing = true;
    session.shutdown();
    assert(*episode_cancel && *session.device_cancel_ && *session.d_cancel_ && *session.d_stream_cancel_);
    assert(!session.watching);
    session.updater_.finish_exit();
    assert(session.updater_.handed_off);
    std::cout << "Shutdown: fresh-launch exit and update handoff, active-task cancellation and cleanup ordering passed\n";
}
