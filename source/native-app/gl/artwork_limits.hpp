#pragma once
#include <cstddef>
namespace sx::artwork {
constexpr size_t kJobs = 64, kReadyCount = 16, kReadyBytes = 32u << 20;
constexpr size_t kEntries = 4096;
constexpr size_t kDecodeBytes = 64u << 20, kFileBytes = 32u << 20;
inline bool image_allowed(int w,int h) {
    return w>0 && h>0 && w<=16384 && h<=16384 && size_t(w)<=kDecodeBytes/4/size_t(h);
}
inline bool can_publish(size_t count,size_t bytes,size_t incoming) {
    return count<kReadyCount && incoming<=kReadyBytes && bytes<=kReadyBytes-incoming;
}
}
