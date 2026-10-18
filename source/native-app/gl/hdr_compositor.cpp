// SPDX-License-Identifier: GPL-3.0-or-later
#include "hdr_compositor.hpp"
#include "gfx/gl_program.hpp"

namespace sx {
namespace {
constexpr const char* kVertex = R"GLSL(
out vec2 uv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";
constexpr const char* kFragment = R"GLSL(
in vec2 uv;
layout(location = 0) out vec4 color;
layout(location = 0) uniform sampler2D video;
layout(location = 1) uniform sampler2D overlay;
layout(location = 2) uniform vec4 rect; // lower-left normalized display rectangle
layout(location = 3) uniform int haveVideo;
layout(location = 4) uniform int bgrOrder;
vec3 pqToLinear(vec3 x) {
    const float m1 = 0.1593017578125, m2 = 78.84375;
    const float c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    vec3 p = pow(clamp(x,0.0,1.0),vec3(1.0/m2));
    return pow(max(p-c1,0.0)/(c2-c3*p),vec3(1.0/m1)); // 1 = 10,000 nits
}
vec3 linearToPq(vec3 x) {
    const float m1 = 0.1593017578125, m2 = 78.84375;
    const float c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    vec3 p = pow(clamp(x,0.0,1.0),vec3(m1));
    return pow((c1+c2*p)/(1.0+c3*p),vec3(m2));
}
vec3 srgbToLinear(vec3 x) {
    return mix(pow((x+0.055)/1.055,vec3(2.4)),x/12.92,lessThanEqual(x,vec3(0.04045)));
}
void main() {
    vec2 q = (uv-rect.xy)/max(rect.zw,vec2(0.000001));
    vec3 signal = vec3(0.0);
    if (haveVideo != 0 && all(greaterThanEqual(q,vec2(0))) && all(lessThan(q,vec2(1))))
        signal = texture(video,vec2(q.x,1.0-q.y)).rgb;
    vec4 ui = texture(overlay,uv); // accumulated premultiplied sRGB + alpha
    // Keep an unobscured source in PQ: avoids round-trip floating-point drift.
    if (ui.a > 0.0) {
        vec3 sdr = srgbToLinear(clamp(ui.rgb/max(ui.a,0.000001),0.0,1.0));
        vec3 bt2020 = vec3(dot(sdr,vec3(0.627404,0.329283,0.043313)),
                           dot(sdr,vec3(0.069097,0.919540,0.011362)),
                           dot(sdr,vec3(0.016391,0.088013,0.895595)));
        signal = linearToPq(pqToLinear(signal)*(1.0-ui.a)+bt2020*0.01*ui.a);
    }
    uvec3 v = uvec3(floor(clamp(signal,0.0,1.0)*1023.0+0.5));
    // RGB10A2: red bits 0..9, green 10..19, blue 20..29, alpha 30..31.
    // The accepted VideoOut token selects the layout: BGR puts blue low,
    // RGB (0x22000000 order bits) puts red low. Do not force one order.
    // EGL's BGRA8 attachment transports these four bytes unchanged. Each
    // float below is an exact 8-bit byte, not an 8-bit picture component.
    if(bgrOrder!=0) v=v.bgr;
    uint word = v.r | (v.g << 10u) | (v.b << 20u) | 0xc0000000u;
    color = vec4(float((word>>16u)&255u),float((word>>8u)&255u),
                 float(word&255u),float((word>>24u)&255u))/255.0;
}
)GLSL";
}

bool HdrCompositor::prepare(int width, int height, std::string* error) {
    if (program_ && width == width_ && height == height_) return true;
    reset();
    if (width <= 0 || height <= 0 || !overlay_.create(width,height,1)) {
        if (error) *error = "Cannot allocate HDR controls surface";
        reset(); return false;
    }
    program_ = hui::gfx::build_program("HDR10 final composition",kVertex,kFragment);
    if (!program_) {
        if (error) *error = "Cannot compile HDR10 output shader";
        reset(); return false;
    }
    glGenVertexArrays(1,&vao_);
    width_ = width; height_ = height;
    if (glGetError() != GL_NO_ERROR) {
        if (error) *error = "The GPU rejected the HDR output resources";
        reset(); return false;
    }
    return true;
}
bool HdrCompositor::present(hui::gfx::Renderer& renderer, GLuint video, VideoRect r,
                            GLuint target, std::string* error, bool bgr) {
    if (!program_) { if (error) *error = "HDR compositor is not ready"; return false; }
    overlay_.bind();
    glDisable(GL_SCISSOR_TEST); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_DITHER);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
    renderer.present(overlay_.framebuffer(),width_,height_);
    glBindFramebuffer(GL_FRAMEBUFFER,target); glViewport(0,0,width_,height_);
    glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_DITHER);
    glUseProgram(program_); glBindVertexArray(vao_);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,video);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,overlay_.texture());
    glUniform1i(0,0); glUniform1i(1,1);
    glUniform4f(2,r.x/width_,1.0f-(r.y+r.h)/height_,r.w/width_,r.h/height_);
    glUniform1i(3,video ? 1 : 0); glUniform1i(4,bgr?1:0);
    glDrawArrays(GL_TRIANGLES,0,3);
    glBindVertexArray(0); glActiveTexture(GL_TEXTURE0);
    const bool ok = glGetError() == GL_NO_ERROR;
    if (!ok && error) *error = "The GPU rejected HDR10 composition";
    return ok;
}
void HdrCompositor::reset() {
    overlay_.destroy();
    if (vao_) glDeleteVertexArrays(1,&vao_);
    if (program_) glDeleteProgram(program_);
    vao_ = program_ = 0; width_ = height_ = 0;
}
}
