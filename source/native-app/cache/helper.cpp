// SPDX-License-Identifier: GPL-3.0-or-later
// Persistent USB-only torrent cache over the loader's inherited connection.
// No listener, network requests, path input, account state or mount changes.
#include "storage.hpp"
#include <vector>
#include <chrono>
#include <csignal>
#include <poll.h>

namespace {
using namespace usb_cache;
using Clock = std::chrono::steady_clock;
bool transfer(void* data, size_t bytes, bool sending, Clock::time_point deadline) {
    auto* p = static_cast<char*>(data);
    while (bytes && Clock::now() < deadline) {
        pollfd fd{sending ? STDOUT_FILENO : STDIN_FILENO, short(sending ? POLLOUT : POLLIN), 0};
        if (poll(&fd, 1, 250) < 0) { if (errno == EINTR) continue; return false; }
        if (fd.revents & (POLLERR | POLLNVAL)) return false;
        if (!(fd.revents & (sending ? POLLOUT : (POLLIN | POLLHUP)))) continue;
        const auto n = sending ? ::write(fd.fd, p, bytes) : ::read(fd.fd, p, bytes);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        p += n; bytes -= size_t(n);
    }
    return bytes == 0;
}
bool output(const Json& j) {
    auto s = j.dump() + "\n";
    return transfer(s.data(), s.size(), true, Clock::now() + std::chrono::seconds(5));
}
bool line(std::string& s, unsigned idle_seconds) {
    s.clear(); const auto end = Clock::now() + std::chrono::seconds(idle_seconds);
    while (s.size() < 2048 && Clock::now() < end) {
        char c;
        if (!transfer(&c, 1, false, end)) return false;
        if (c == '\n') return true;
        s += c;
    }
    return false;
}
int run() {
    std::string s;
    if (!line(s, 15)) return 1;
    auto request = Json::parse(s, nullptr, false);
    if (!open_ok(request)) { output({{"kind", "error"}, {"code", "invalid-request"}}); return 1; }
    const int mount = request["mount"].get<int>();
    auto root = "/mnt/usb" + std::to_string(mount);
#ifdef NUVIO_USB_CACHE_HOST_TEST
    if (const char* base = std::getenv("NUVIO_USB_CACHE_TEST_ROOT")) root = std::string(base) + "/usb" + std::to_string(mount);
#endif
    Storage cache;
    if (!cache.open(root)) { output({{"kind", "error"}, {"code", "usb-unavailable"}, {"errno", errno}}); return 1; }
    const int64_t limit = request["limit"].get<int64_t>();
    if (!output({{"kind", "ready"}, {"protocol", protocol}, {"title_id", title}, {"limit", limit}})) return 1;
    std::vector<char> data(chunk_bytes);
    while (line(s, 900)) {
        request = Json::parse(s, nullptr, false);
        if (!io_ok(request, limit)) { output({{"kind", "error"}, {"code", "invalid-io"}}); return 1; }
        const int64_t offset = request["offset"].get<int64_t>();
        const auto bytes = request["bytes"].get<size_t>();
        if (request["op"] == "write") {
            if (!transfer(data.data(), bytes, false, Clock::now() + std::chrono::seconds(5)) ||
                !cache.write(offset, data.data(), bytes)) break;
            if (!output({{"kind", "written"}, {"bytes", bytes}, {"storage", cache.storage_bytes()}})) return 1;
        } else {
            if (!cache.read(offset, data.data(), bytes)) break;
            if (!output({{"kind", "body"}, {"bytes", bytes}}) ||
                !transfer(data.data(), bytes, true, Clock::now() + std::chrono::seconds(5))) return 1;
        }
    }
    return 0; // EOF/idle/failure closes the anonymous cache; app switches to RAM.
}
}
int main() {
    std::signal(SIGPIPE, SIG_IGN);
    try { return run(); }
    catch (...) { output({{"kind", "error"}, {"code", "cache-failed"}}); return 1; }
}
