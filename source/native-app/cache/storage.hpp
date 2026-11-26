// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "protocol.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace usb_cache {
// A newly created anonymous file: never open/truncate an existing USB file.
// Unlink before buffering anything so crashes/connection loss leave no named
// cache to accumulate, expose another profile's data, or require broad cleanup.
class Storage {
    int fd_ = -1;
public:
    ~Storage() { if (fd_ >= 0) ::close(fd_); }
    bool open(const std::string& root) {
        // Hold directories through libc descriptors, so a path/symlink swap
        // cannot redirect creation or cleanup. This helper owns its cwd.
        int dir = ::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
        if (dir < 0) return false;
        bool entered = fchdir(dir) == 0;
        ::close(dir);
        if (!entered) return false;
        if (mkdir(".nuvio-maxd-cache", 0700) != 0 && errno != EEXIST) return false;
        dir = ::open(".nuvio-maxd-cache", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
        if (dir < 0) return false;
        entered = fchdir(dir) == 0;
        ::close(dir);
        if (!entered) return false;
        std::string pattern = "PPSA99288-XXXXXXXXXXXX";
        fd_ = mkstemp(pattern.data());
        if (fd_ < 0) return false;
        struct stat file{};
        const bool valid = fstat(fd_, &file) == 0 && S_ISREG(file.st_mode) && file.st_nlink == 1;
        if (!valid || unlink(pattern.c_str()) != 0) {
            ::close(fd_); fd_ = -1; return false;
        }
        return true;
    }
    bool write(int64_t offset, const void* data, size_t bytes) {
        if (lseek(fd_, off_t(offset), SEEK_SET) != off_t(offset)) return false;
        auto* p = static_cast<const char*>(data);
        while (bytes) {
            const auto n = ::write(fd_, p, bytes);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return false;
            p += n; bytes -= size_t(n);
        }
        return true;
    }
    bool read(int64_t offset, void* data, size_t bytes) {
        if (lseek(fd_, off_t(offset), SEEK_SET) != off_t(offset)) return false;
        auto* p = static_cast<char*>(data);
        while (bytes) {
            const auto n = ::read(fd_, p, bytes);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return false;
            p += n; bytes -= size_t(n);
        }
        return true;
    }
    int64_t storage_bytes() const {
        struct stat st{};
        return fstat(fd_, &st) == 0 ? int64_t(st.st_blocks) * 512 : 0;
    }
};
}
