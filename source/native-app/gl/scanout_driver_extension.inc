/* App experimental extension compiled inside the pinned OpenGL runtime.
 * Registration lifetime and framebuffer descriptors remain driver-owned.
 * SPDX-License-Identifier: GPL-3.0-or-later */
extern int sceVideoOutSubmitChangeBufferAttribute2(int, int, const void*, const void*);
static uint64_t runtime_scanout_format = VIDEO_OUT_PIXEL_FORMAT;
int ps5_opengl_set_scanout_format(uint64_t format, int32_t results[4])
{
    for (unsigned i = 0; i < 4; ++i) results[i] = INT32_C(0x7fffffff);
    if (!runtime_video_registered || runtime_video_handle < 0 ||
        !runtime_video_framebuffer || !runtime_video_api.set_attribute2 ||
        !runtime_video_api.is_flip_pending ||
        runtime_video_api.is_flip_pending(runtime_video_handle) != 0)
        return -1;
    video_attribute_t target = {{0}};
    runtime_video_api.set_attribute2(&target, format, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0, 0);
    /* ProsperoTube/EVO: change the displayed set in place, at the next flip.
     * Never unregister an on-screen set after a refusal: doing so can leave
     * scanout without valid buffers. A rejected change preserves SDR/HDR. */
    results[0] = sceVideoOutSubmitChangeBufferAttribute2(runtime_video_handle, 0, &target, NULL);
    if (!results[0]) runtime_scanout_format = format;
    return results[0];
}
