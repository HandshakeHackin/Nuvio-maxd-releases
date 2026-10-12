// Native-resolution YUV presentation; owns its GL resources on the UI thread.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "video_frame.h"
#include <GL/glcorearb.h>
#include <string>

namespace sx {
class VideoSurface {
public:
    ~VideoSurface() { reset(); }
    VideoSurface() = default;
    VideoSurface(const VideoSurface&) = delete;
    VideoSurface& operator=(const VideoSurface&) = delete;
    bool upload(const VideoPicture& picture, std::string* error, bool hdr_output = false);
    void reset();
    GLuint texture() const { return rgba_; }
private:
    GLuint rgba_ = 0, y_ = 0, uv_ = 0, framebuffer_ = 0, program_ = 0, vao_ = 0;
    int w_ = 0, h_ = 0;
    VideoFormat format_ = VideoFormat::Rgba;
};
} // namespace sx
