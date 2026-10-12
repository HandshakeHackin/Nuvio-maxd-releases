// PQ video + SDR controls, composed in linear light then packed for VideoOut.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gfx/canvas.hpp"
#include "gfx/renderer.hpp"
#include "video_frame.h"
#include <string>

namespace sx {
class HdrCompositor {
public:
    ~HdrCompositor() { reset(); }
    bool prepare(int width, int height, std::string* error);
    bool present(hui::gfx::Renderer& renderer, GLuint video, VideoRect rect,
                 GLuint target = 0, std::string* error = nullptr, bool bgr = false);
    void reset();
private:
    hui::gfx::Canvas overlay_;
    GLuint program_ = 0, vao_ = 0;
    int width_ = 0, height_ = 0;
};
}
