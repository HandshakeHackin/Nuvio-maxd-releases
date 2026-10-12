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
        !runtime_video_api.register_buffers2 || !runtime_video_api.unregister_buffers ||
        !runtime_video_api.is_flip_pending ||
        runtime_video_api.is_flip_pending(runtime_video_handle) != 0)
        return -1;
    video_attribute_t target = {{0}}, previous = {{0}};
    runtime_video_api.set_attribute2(&target, format, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0, 0);
    runtime_video_api.set_attribute2(&previous, runtime_scanout_format, 0,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0, 0);
    results[0] = sceVideoOutSubmitChangeBufferAttribute2(runtime_video_handle, 0, &target, NULL);
    if (!results[0]) {runtime_scanout_format = format;return 0;}
    /* Do not unregister on unrelated API errors. A busy displayed set is left
     * intact, and the original failure is returned to the app. */
    if ((uint32_t)results[0] != UINT32_C(0x80290003)) return results[0];
    results[1] = runtime_video_api.unregister_buffers(runtime_video_handle, 0);
    if (results[1]) return results[0];
    runtime_video_registered = 0;
    video_buffer_t buffers[2] = {
        {runtime_video_framebuffer, NULL, NULL, NULL},
        {runtime_video_framebuffer + (runtime_video_framebuffer_size >= FRAMEBUFFER_POOL_BYTES
                                     ? FRAMEBUFFER_BYTES : 0), NULL, NULL, NULL}};
    results[2] = runtime_video_api.register_buffers2(runtime_video_handle, 0, 0, buffers, 2,
                                                   &target, 0, NULL);
    if (!results[2]) {runtime_video_registered = 1;runtime_scanout_format = format;return 0;}
    results[3] = runtime_video_api.register_buffers2(runtime_video_handle, 0, 0, buffers, 2,
                                                   &previous, 0, NULL);
    if (!results[3]) runtime_video_registered = 1;
    /* If restore fails the driver's registered flag stays false: its next
     * prepare recreates the SDR presentation, instead of using a lost set. */
    return results[3] ? results[3] : results[2];
}
