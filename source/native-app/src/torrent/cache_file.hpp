// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace bt {
// The engine owns one cache per torrent. External implementations must bound
// operations and return false on removal/failure; the engine then uses RAM.
class CacheFile {
public:
    virtual ~CacheFile() = default;
    virtual bool write(int64_t offset, const void* data, size_t bytes) = 0;
    virtual bool read(int64_t offset, void* data, size_t bytes) = 0;
    virtual int64_t storage_bytes() const = 0;
};
using CacheFactory = std::function<std::unique_ptr<CacheFile>(int, int64_t, std::string&)>;

class LocalCacheFile final : public CacheFile {
    FILE* file_;
public:
    explicit LocalCacheFile(FILE* file) : file_(file) {}
    ~LocalCacheFile() override { if (file_) fclose(file_); }
    bool write(int64_t offset, const void* data, size_t bytes) override {
        return offset >= 0 && fseeko(file_, off_t(offset), SEEK_SET) == 0 &&
               fwrite(data, 1, bytes, file_) == bytes && fflush(file_) == 0;
    }
    bool read(int64_t offset, void* data, size_t bytes) override {
        return offset >= 0 && fseeko(file_, off_t(offset), SEEK_SET) == 0 &&
               fread(data, 1, bytes, file_) == bytes;
    }
    int64_t storage_bytes() const override {
        struct stat st{};
        return fstat(fileno(file_), &st) == 0 ? int64_t(st.st_blocks) * 512 : 0;
    }
};
}
