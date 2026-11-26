// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../third_party/json.hpp"
#include <cstdint>
#include <string>

namespace usb_cache {
using Json = nlohmann::json;
constexpr int protocol = 1;
inline constexpr const char* title = "PPSA99288";
constexpr int64_t max_bytes = int64_t(6) << 30;
constexpr size_t chunk_bytes = 256 * 1024;

inline bool open_ok(const Json& j) {
    if (!j.is_object() || j.size() != 5 || j.value("protocol", 0) != protocol ||
        j.value("title_id", std::string()) != title || j.value("op", std::string()) != "open" ||
        !j.contains("mount") || !j["mount"].is_number_integer() ||
        !j.contains("limit") || !j["limit"].is_number_integer()) return false;
    const auto mount = j["mount"].get<int64_t>(), limit = j["limit"].get<int64_t>();
    return mount >= 0 && mount < 8 && limit >= (int64_t(512) << 20) && limit <= max_bytes;
}
inline bool io_ok(const Json& j, int64_t limit) {
    if (!j.is_object() || j.size() != 3 || !j.contains("op") || !j["op"].is_string() ||
        (j["op"] != "read" && j["op"] != "write") ||
        !j.contains("offset") || !j["offset"].is_number_integer() ||
        !j.contains("bytes") || !j["bytes"].is_number_integer()) return false;
    const auto offset = j["offset"].get<int64_t>(), bytes = j["bytes"].get<int64_t>();
    return limit > 0 && limit <= max_bytes && offset >= 0 && offset <= limit &&
           bytes > 0 && bytes <= int64_t(chunk_bytes) && bytes <= limit - offset;
}
}
