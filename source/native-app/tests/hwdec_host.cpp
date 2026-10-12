// Production decoder setup with simulated PS5 allocation and decoder failures.
// SPDX-License-Identifier: GPL-3.0-or-later
#define PLATFORM_PS5_NATIVE 1
extern "C" {
#include <libavcodec/avcodec.h>
}
// FFmpeg 7 host headers use the older profile names; the target SDK uses 8.
#ifndef AV_PROFILE_UNKNOWN
#define AV_PROFILE_UNKNOWN FF_PROFILE_UNKNOWN
#define AV_PROFILE_HEVC_MAIN FF_PROFILE_HEVC_MAIN
#define AV_PROFILE_HEVC_MAIN_10 FF_PROFILE_HEVC_MAIN_10
#define AV_PROFILE_VP9_2 FF_PROFILE_VP9_2
#endif
// The backend stays in this one test translation unit, including its private
// slot types; GCC's cross-unit subobject linkage warning does not apply here.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#endif
#include "../src/hwdec_ps5.cpp"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace test {
enum Failure { None, DeepQuery, DeepCreate, DeepCpuMap, AllQuery };
Failure failure = None;
std::vector<unsigned> queried;
std::map<void*, size_t> mappings;
std::map<int64_t, size_t> allocations;
int64_t next_offset = 1;
unsigned created_depth = 0;
unsigned current_depth = 0;
bool fail_next_direct_map = false;

void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void clean() {
    for (auto& slot : g_slots) slot_teardown(slot);
    require(mappings.empty() && allocations.empty(), "decoder setup leaked an allocation");
    failure = None; queried.clear(); created_depth = current_depth = 0;
    fail_next_direct_map = false;
}
std::unique_ptr<HwDecoder> open(int codec, int profile, int format) {
    AVCodecParameters par{};
    par.codec_type = AVMEDIA_TYPE_VIDEO;
    par.codec_id = AVCodecID(codec); par.profile = profile; par.format = format;
    par.width = 3840; par.height = 2160;
    std::string reason;
    return std::unique_ptr<HwDecoder>(HwDecoder::open(&par, &reason));
}
}

void dlog(const char*, ...) {}
extern "C" {
int sceSysmoduleLoadModule(uint16_t) { return 0; }
int64_t sceKernelGetDirectMemorySize(void) { return 4ll << 30; }
int sceKernelAllocateDirectMemory(int64_t, int64_t, size_t size, size_t, int, int64_t* start) {
    *start = test::next_offset++;
    test::allocations[*start] = size;
    return 0;
}
int sceKernelMapDirectMemory(void** addr, size_t size, int, int, int64_t, size_t) {
    if (test::fail_next_direct_map) {
        test::fail_next_direct_map = false; *addr = nullptr; return -12;
    }
    *addr = malloc(size);
    if (!*addr) return -1;
    test::mappings[*addr] = size;
    return 0;
}
int sceKernelReleaseDirectMemory(int64_t start, size_t) { test::allocations.erase(start); return 0; }
int sceKernelAvailableDirectMemorySize(int64_t, int64_t, size_t, int64_t* start, size_t* size) {
    *start = 0; *size = 4ull << 30; return 0;
}
int sceKernelMunmap(void* addr, size_t) {
    if (test::mappings.erase(addr)) free(addr);
    return 0;
}
int sceKernelMapNamedFlexibleMemory(void** addr, size_t size, int, int, const char*) {
    if (test::failure == test::DeepCpuMap && test::current_depth > 1) {
        test::fail_next_direct_map = true; *addr = nullptr; return -12;
    }
    *addr = malloc(size);
    if (!*addr) return -1;
    test::mappings[*addr] = size;
    return 0;
}
int sceKernelReleaseFlexibleMemory(void*, size_t) { return 0; }
int sceKernelAvailableFlexibleMemorySize(size_t* size) { *size = 256u << 20; return 0; }
int32_t sceVideodec2QueryComputeMemoryInfo(SceVideodec2ComputeMemoryInfo* mem) {
    mem->cpu_gpu_size = 0x4000; return 0;
}
int32_t sceVideodec2AllocateComputeQueue(const SceVideodec2ComputeConfigInfo*,
                                       const SceVideodec2ComputeMemoryInfo*, void** queue) {
    *queue = reinterpret_cast<void*>(1); return 0;
}
int32_t sceVideodec2ReleaseComputeQueue(void*) { return 0; }
int32_t sceVideodec2QueryDecoderMemoryInfo(const SceVideodec2DecoderConfigInfo* cfg,
                                         SceVideodec2DecoderMemoryInfo* mem) {
    test::queried.push_back(cfg->pipeline_depth);
    test::current_depth = cfg->pipeline_depth;
    // Each attempt has just its new compute allocation at this point.
    // A failed deeper attempt must release its workspace before retrying.
    test::require(test::mappings.size() == 1 && test::allocations.size() == 1,
                  "failed setup memory was not released before retry");
    if (test::failure == test::AllQuery ||
        (test::failure == test::DeepQuery && cfg->pipeline_depth > 1)) return -12;
    mem->cpu_size = mem->gpu_size = mem->cpu_gpu_size = mem->max_frame_size = 0x4000;
    return 0;
}
int32_t sceVideodec2CreateDecoder(const SceVideodec2DecoderConfigInfo* cfg,
                                 const SceVideodec2DecoderMemoryInfo*, void** decoder) {
    if (test::failure == test::DeepCreate && cfg->pipeline_depth > 1) return -12;
    test::created_depth = cfg->pipeline_depth;
    *decoder = reinterpret_cast<void*>(2); return 0;
}
int32_t sceVideodec2DeleteDecoder(void*) { return 0; }
int32_t sceVideodec2Reset(void*) { return 0; }
int32_t sceVideodec2Decode(void*, SceVideodec2InputData*, SceVideodec2FrameBuffer*, SceVideodec2OutputInfo*) { return 0; }
int32_t sceVideodec2Flush(void*, SceVideodec2FrameBuffer*, SceVideodec2OutputInfo*) { return 0; }
}

int main() {
    HwDecoder::load_module();
    int failures = 0;
    auto run = [&](const char* name, auto fn) {
        try { test::clean(); fn(); test::clean(); printf("%s: PASS\n", name); }
        catch (const std::exception& e) { ++failures; printf("%s: FAIL (%s)\n", name, e.what()); test::clean(); }
    };
    auto main10 = [] { return test::open(AV_CODEC_ID_HEVC, AV_PROFILE_HEVC_MAIN_10, AV_PIX_FMT_YUV420P10LE); };
    run("main10_prefers_deep_pipeline", [&] {
        auto d = main10(); test::require(bool(d) && test::created_depth == 4, "deep hardware setup was lost");
    });
    run("main10_retries_query_failure", [&] {
        test::failure = test::DeepQuery;
        auto d = main10(); test::require(bool(d) && test::queried == std::vector<unsigned>{4, 1}, "no smaller-pipeline retry");
    });
    run("main10_retries_create_failure", [&] {
        test::failure = test::DeepCreate;
        auto d = main10(); test::require(bool(d) && test::created_depth == 1, "allocation failure lost hardware decoding");
    });
    run("main10_releases_unmapped_workspace_before_retry", [&] {
        test::failure = test::DeepCpuMap;
        auto d = main10(); test::require(bool(d) && test::created_depth == 1, "workspace mapping failure lost the retry");
    });
    run("main10_all_fail_cleanly", [&] {
        test::failure = test::AllQuery;
        auto d = main10(); test::require(!d && test::queried == std::vector<unsigned>{4, 1}, "failure retry was not bounded");
        test::require(test::mappings.empty() && test::allocations.empty(), "failed retry leaked memory");
    });
    run("main10_successful_retry_can_be_reused", [&] {
        test::failure = test::DeepQuery;
        { auto d = main10(); test::require(bool(d), "retry did not open"); }
        { auto d = main10(); test::require(bool(d), "resident decoder cannot be reopened"); }
        test::require(test::queried == std::vector<unsigned>{4, 1}, "working resident decoder was reallocated");
    });
    run("other_codecs_keep_existing_setup", [&] {
        { auto d = test::open(AV_CODEC_ID_H264, 100, AV_PIX_FMT_YUV420P);
          test::require(bool(d) && test::created_depth == 4, "H.264 setup changed"); }
        test::clean();
        { auto d = test::open(AV_CODEC_ID_HEVC, AV_PROFILE_HEVC_MAIN, AV_PIX_FMT_YUV420P);
          test::require(bool(d) && test::created_depth == 4, "8-bit HEVC setup changed"); }
    });
    run("codec_switch_releases_idle_workspaces", [&] {
        { auto d = main10(); test::require(bool(d), "Main10 setup failed"); }
        auto d = test::open(AV_CODEC_ID_H264, 100, AV_PIX_FMT_YUV420P);
        test::require(bool(d) && !g_slots[HEVC10].ready && test::created_depth == 4,
                      "idle HEVC workspace was retained during H.264 setup");
    });
    run("h264_retries_create_failure", [&] {
        test::failure = test::DeepCreate;
        auto d = test::open(AV_CODEC_ID_H264, 100, AV_PIX_FMT_YUV420P);
        test::require(bool(d) && test::queried == std::vector<unsigned>{4, 1}, "H.264 setup has no bounded retry");
    });
    run("h264_all_fail_cleanly", [&] {
        test::failure = test::AllQuery;
        auto d = test::open(AV_CODEC_ID_H264, 100, AV_PIX_FMT_YUV420P);
        test::require(!d && test::queried == std::vector<unsigned>{4, 1}, "H.264 retry is not bounded");
        test::require(test::mappings.empty() && test::allocations.empty(), "H.264 failure leaked memory");
    });
    printf("Hardware decoder setup: 10 tests, %d failures\n", failures);
    return failures ? 1 : 0;
}
