// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "json.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <sys/stat.h>
namespace nuvio_update {
using Json = nlohmann::json;
inline constexpr const char* repository = "HandshakeHackin/Nuvio-maxd-releases";
inline constexpr const char* feed = "https://api.github.com/repos/HandshakeHackin/Nuvio-maxd-releases/releases?per_page=20";
inline constexpr const char* title_id = "PPSA99288";
inline constexpr const char* current_version = "0.5.32";
inline constexpr std::uint64_t max_image_bytes = 512ull << 20;
struct Release { std::string version, tag, url, sha256, notes; std::uint64_t size = 0; };
bool newer(const std::string& a, const std::string& b);
bool valid_release(const Release& release);
Release select_release(const Json& releases, const std::string& current);
std::string installed_image(const Json& info);
bool mount_released(const Json& images, const std::string& path);
bool managed_image_path(const std::string& path);
std::string sha256_file(int fd);
// All writes are relative to a directory descriptor. Each directory and the
// original image must be real, not a symlink. The old file remains installed
// until one rename commits the verified replacement; the backup is retained.
class Transaction {
public:
    ~Transaction();
    bool begin(const std::string& path, std::string& error);
    bool append(const void* bytes, std::size_t size, std::string& error);
    bool verify(const Release& release, std::string& error);
    bool commit(const std::function<bool()>& released, std::string& error);
    std::uint64_t original_size() const { return original_.st_size; }
    std::uint64_t received() const { return received_; }
    const std::string& backup() const { return backup_; }
private:
    bool unchanged() const;
    int dir_ = -1, old_ = -1, stage_fd_ = -1;
    struct stat original_{};
    std::string name_, stage_, backup_;
    std::uint64_t received_ = 0;
    bool verified_ = false, committed_ = false;
    Release verified_release_;
};
}
