// Dedicated bounded storage for VLC's large decoded pictures. The hook is
// supplied by the vendored VLC picture patch; ordinary allocations are unchanged.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int64_t sceKernelGetDirectMemorySize(void);
int sceKernelAvailableDirectMemorySize(int64_t, int64_t, size_t, int64_t *, size_t *);
int sceKernelAllocateDirectMemory(int64_t, int64_t, size_t, size_t, int, int64_t *);
int sceKernelMapDirectMemory(void **, size_t, int, int, int64_t, size_t);
int sceKernelMunmap(void *, size_t);
int sceKernelReleaseDirectMemory(int64_t, size_t);

#define PAGE 0x4000u
#define POOL_LIMIT ((size_t)512 << 20)
#define RESERVE ((size_t)128 << 20)
struct allocation { void *address; int64_t offset; size_t bytes; int retired, mapped; };
static struct allocation blocks[64];
static size_t used;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void collect(void) {
    for (unsigned i = 0; i < 64; ++i) {
        struct allocation *b = &blocks[i];
        if (!b->bytes || !b->retired) continue;
        if (b->mapped) {
            const int rc = sceKernelMunmap(b->address, b->bytes);
            if (rc != 0) { fprintf(stderr, "vlc pictures: unmap deferred rc=%08x bytes=%zu\n", (unsigned)rc, b->bytes); continue; }
            b->mapped = 0;
        }
        // Keep ownership and budget until BOTH operations succeed. Never free
        // a direct-memory pointer through VLC's ordinary aligned_free fallback.
        const int rc = sceKernelReleaseDirectMemory(b->offset, b->bytes);
        if (rc != 0) { fprintf(stderr, "vlc pictures: release deferred rc=%08x bytes=%zu\n", (unsigned)rc, b->bytes); continue; }
        used -= b->bytes;
        *b = (struct allocation){0};
    }
}
void *vlc_ps5_big_alloc(size_t bytes) {
    if (!bytes || bytes > POOL_LIMIT || bytes > SIZE_MAX - (PAGE - 1)) return NULL;
    bytes = (bytes + PAGE - 1) & ~(size_t)(PAGE - 1);
    pthread_mutex_lock(&mutex);
    collect();
    struct allocation *b = NULL;
    for (unsigned i = 0; i < 64; ++i) if (!blocks[i].bytes) { b = &blocks[i]; break; }
    int64_t start = 0, limit = sceKernelGetDirectMemorySize();
    size_t available = 0;
    if (!b || bytes > POOL_LIMIT - used || limit <= 0 ||
        sceKernelAvailableDirectMemorySize(0, limit, PAGE, &start, &available) != 0 ||
        available < RESERVE || bytes > available - RESERVE) {
        pthread_mutex_unlock(&mutex); return NULL;
    }
    int64_t offset = -1;
    if (sceKernelAllocateDirectMemory(0, limit, bytes, PAGE, 12, &offset) != 0) {
        pthread_mutex_unlock(&mutex); return NULL;
    }
    *b = (struct allocation){NULL, offset, bytes, 1, 0};
    used += bytes;
    void *address = NULL;
    if (sceKernelMapDirectMemory(&address, bytes, 3, 0, offset, PAGE) != 0 || !address) {
        collect(); pthread_mutex_unlock(&mutex); return NULL;
    }
    b->address = address; b->mapped = 1; b->retired = 0;
    pthread_mutex_unlock(&mutex);
    return address;
}
int vlc_ps5_big_free(void *address) {
    if (!address) return 0;
    pthread_mutex_lock(&mutex);
    int owned = 0;
    for (unsigned i = 0; i < 64; ++i) if (blocks[i].bytes && blocks[i].address == address) {
        blocks[i].retired = 1; owned = 1; break;
    }
    collect();
    pthread_mutex_unlock(&mutex);
    return owned;
}
