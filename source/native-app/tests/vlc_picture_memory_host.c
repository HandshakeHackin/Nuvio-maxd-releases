// Failure injection against the production direct-picture allocator.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/vlc_picture_memory.c"
static size_t available_bytes=(size_t)8<<30;
static int map_fail,unmap_fail,release_fail,alloc_calls,release_calls,unmap_calls;
static int64_t next_offset=0;
int64_t sceKernelGetDirectMemorySize(void){return (int64_t)8<<30;}
int sceKernelAvailableDirectMemorySize(int64_t a,int64_t b,size_t c,int64_t*d,size_t*e){(void)a;(void)b;(void)c;*d=0;*e=available_bytes;return 0;}
int sceKernelAllocateDirectMemory(int64_t a,int64_t b,size_t n,size_t c,int type,int64_t*offset){(void)a;(void)b;(void)c;assert(type==12);*offset=next_offset;next_offset+=(int64_t)n;++alloc_calls;return 0;}
int sceKernelMapDirectMemory(void**p,size_t n,int prot,int flags,int64_t offset,size_t align){(void)n;(void)flags;assert(prot==3 && align==0x4000);if(map_fail)return -1;*p=(void*)(uintptr_t)(offset+0x100000000ull);return 0;}
int sceKernelMunmap(void*p,size_t n){(void)p;(void)n;++unmap_calls;return unmap_fail?-1:0;}
int sceKernelReleaseDirectMemory(int64_t p,size_t n){(void)p;(void)n;++release_calls;return release_fail?-1:0;}
int main(void){
 assert(!vlc_ps5_big_alloc(SIZE_MAX)); assert(!vlc_ps5_big_free((void*)123));
 available_bytes=RESERVE;assert(!vlc_ps5_big_alloc(1));assert(!alloc_calls);
 available_bytes=(size_t)8<<30;
 void*a=vlc_ps5_big_alloc(1);assert(a && used==PAGE);
 assert(vlc_ps5_big_free(a)==1 && used==0);assert(vlc_ps5_big_free(a)==0);
 a=vlc_ps5_big_alloc(POOL_LIMIT);assert(a && !vlc_ps5_big_alloc(1));
 unmap_fail=1;int before=release_calls;assert(vlc_ps5_big_free(a)==1);
 assert(used==POOL_LIMIT && release_calls==before && !vlc_ps5_big_alloc(1));
 unmap_fail=0;release_fail=1;assert(vlc_ps5_big_free(a)==1 && used==POOL_LIMIT);
 before=unmap_calls;assert(!vlc_ps5_big_alloc(1));assert(unmap_calls==before);
 release_fail=0;void*b=vlc_ps5_big_alloc(1);assert(b && used==PAGE);assert(vlc_ps5_big_free(b)==1 && used==0);
 map_fail=1;release_fail=1;assert(!vlc_ps5_big_alloc(1) && used==PAGE);
 map_fail=0;release_fail=0;b=vlc_ps5_big_alloc(1);assert(b && used==PAGE);assert(vlc_ps5_big_free(b)==1 && used==0);
 puts("VLC picture memory passed: reserve, cap, alignment, foreign pointers, map/unmap/release failures and ownership retention");
}
