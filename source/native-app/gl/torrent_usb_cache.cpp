// SPDX-License-Identifier: GPL-3.0-or-later
#include "torrent_usb_cache.hpp"
#include "update_channel.hpp"
#include "../cache/protocol.hpp"
#include <algorithm>
#include <atomic>
#include <cstdlib>

namespace sx {
namespace {
const char* helper_path() {
#ifdef NUVIO_USB_CACHE_HOST_TEST
    if (const char* path = std::getenv("NUVIO_USB_CACHE_TEST_HELPER")) return path;
#endif
    return "/app0/nuvio-usb-cache.elf";
}
class UsbCache final : public bt::CacheFile {
    UpdateChannel channel_;
    std::atomic<bool> cancel_{false};
    int64_t limit_ = 0, storage_ = 0;
    bool valid_ = false;
    bool reply(usb_cache::Json& j) {
        std::string line;
        if (!channel_.line(line, cancel_)) return false;
        j = usb_cache::Json::parse(line, nullptr, false);
        return j.is_object();
    }
    bool command(const usb_cache::Json& j) {
        const auto s = j.dump() + "\n";
        return channel_.send(s.data(), s.size(), cancel_);
    }
    bool fail() { valid_ = false; channel_.close(); return false; }
public:
    bool open(int mount, int64_t limit, std::string& error) {
        limit_ = limit;
        const usb_cache::Json request = {{"op", "open"}, {"protocol", usb_cache::protocol},
            {"title_id", usb_cache::title}, {"mount", mount}, {"limit", limit}};
        if (!usb_cache::open_ok(request) ||
            !channel_.open(helper_path(), cancel_, error, 10, 1)) return false;
        usb_cache::Json hello;
        if (!command(request) || !reply(hello) || hello.value("kind", std::string()) != "ready" ||
            hello.value("protocol", 0) != usb_cache::protocol ||
            hello.value("title_id", std::string()) != usb_cache::title || hello.value("limit", int64_t(0)) != limit) {
            error = "USB cache unavailable. Check the selected drive and the payload loader on port 9021.";
            return fail();
        }
        channel_.deadline(0); valid_ = true; return true;
    }
    bool write(int64_t offset, const void* data, size_t bytes) override {
      try {
        if (!valid_ || offset < 0 || offset > limit_ || bytes > uint64_t(limit_ - offset)) return false;
        channel_.deadline(5);
        auto* p = static_cast<const char*>(data);
        while (bytes) {
            const auto count = std::min(bytes, usb_cache::chunk_bytes);
            usb_cache::Json ack;
            if (!command({{"op", "write"}, {"offset", offset}, {"bytes", count}}) ||
                !channel_.send(p, count, cancel_) || !reply(ack) ||
                ack.value("kind", std::string()) != "written" || ack.value("bytes", size_t(0)) != count ||
                !ack.contains("storage") || !ack["storage"].is_number_integer()) return fail();
            const auto allocated = ack["storage"].get<int64_t>();
            if (allocated < 0 || allocated > limit_ + (1 << 20)) return fail();
            storage_ = allocated; p += count; bytes -= count; offset += int64_t(count);
        }
        channel_.deadline(0); return true;
      } catch (...) { return fail(); }
    }
    bool read(int64_t offset, void* data, size_t bytes) override {
      try {
        if (!valid_ || offset < 0 || offset > limit_ || bytes > uint64_t(limit_ - offset)) return false;
        channel_.deadline(5);
        auto* p = static_cast<char*>(data);
        while (bytes) {
            const auto count = std::min(bytes, usb_cache::chunk_bytes);
            usb_cache::Json ack;
            if (!command({{"op", "read"}, {"offset", offset}, {"bytes", count}}) || !reply(ack) ||
                ack.value("kind", std::string()) != "body" || ack.value("bytes", size_t(0)) != count ||
                !channel_.receive(p, count, cancel_)) return fail();
            p += count; bytes -= count; offset += int64_t(count);
        }
        channel_.deadline(0); return true;
      } catch (...) { return fail(); }
    }
    int64_t storage_bytes() const override { return storage_; }
};
}
bt::CacheFactory torrent_usb_cache_factory() {
    return [](int mount, int64_t limit, std::string& error) -> std::unique_ptr<bt::CacheFile> {
        try {
            auto cache = std::make_unique<UsbCache>();
            if (cache->open(mount, limit, error)) return cache;
        } catch (...) { error = "USB cache connection failed."; }
        return {};
    };
}
}
