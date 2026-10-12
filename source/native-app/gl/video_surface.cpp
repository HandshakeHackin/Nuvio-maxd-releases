#include "video_surface.hpp"
#include "gfx/gl_program.hpp"
#include "hdr_tone_map.h"

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
layout(location = 0) uniform sampler2D luma;
layout(location = 1) uniform sampler2D chroma;
layout(location = 2) uniform float sampleScale;
layout(location = 3) uniform int wide;
layout(location = 4) uniform int colors;
layout(location = 5) uniform int fullRange;
layout(location = 6) uniform int hdrOutput;
layout(location = 7) uniform float peakNits;

vec3 pq(vec3 x) {
    const float m1 = 0.1593017578125, m2 = 78.84375;
    const float c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    vec3 p = pow(clamp(x, 0.0, 1.0), vec3(1.0 / m2));
    return pow(max(p - c1, 0.0) / (c2 - c3 * p), vec3(1.0 / m1)) * 100.0;
}
vec3 toSdr(vec3 light) {
    // PQ is display light, so encode the SDR display response (BT.1886,
    // ideal black), rather than applying a camera BT.709 OETF a second time.
    // Extended Reinhard rolls highlights to 100-nit SDR white. A common
    // luminance scale preserves RGB balance instead of compressing channels
    // separately; no exposure gain tuned to a particular film is applied.
    float luminance = dot(light, vec3(0.2627, 0.6780, 0.0593));
    float peak = peakNits / 100.0;
    float mapped = clamp(luminance * (1.0 + luminance / (peak * peak)) /
                         (1.0 + luminance), 0.0, 1.0);
    light *= mapped / max(luminance, 0.000001);
    vec3 rgb = vec3(dot(light, vec3(1.660491, -0.587641, -0.072850)),
                    dot(light, vec3(-0.124550, 1.132900, -0.008350)),
                    dot(light, vec3(-0.018151, -0.100579, 1.118730)));
    // Fit wide-gamut colours towards neutral at the same luminance. Hard
    // channel clipping can change colour balance, especially in highlights.
    float low = min(min(rgb.r, rgb.g), rgb.b);
    float high = max(max(rgb.r, rgb.g), rgb.b);
    float saturation = 1.0;
    if (low < 0.0) saturation = min(saturation, mapped / (mapped - low));
    if (high > 1.0) saturation = min(saturation, (1.0 - mapped) / (high - mapped));
    rgb = vec3(mapped) + (rgb - vec3(mapped)) * saturation;
    return pow(clamp(rgb, 0.0, 1.0), vec3(1.0 / 2.4));
}
void main() {
    // R16/RG16 are normalized storage. The hardware can return low-aligned
    // 10-bit words; P010 software output is high-aligned. Keep both explicit.
    float rawY = texture(luma, uv).r * sampleScale;
    vec2 rawC = texture(chroma, uv).rg * sampleScale;
    float unit = wide != 0 ? 4.0 : 1.0;
    float maximum = wide != 0 ? 1023.0 : 255.0;
    float y = fullRange != 0 ? rawY / maximum : (rawY - 16.0 * unit) / (219.0 * unit);
    vec2 c = fullRange != 0 ? (rawC - 128.0 * unit) / maximum
                            : (rawC - 128.0 * unit) / (224.0 * unit);
    vec3 rgb;
    if (colors == 2) {
        vec3 pqSignal = vec3(y + 1.4746 * c.y, y - 0.16455 * c.x - 0.57135 * c.y,
                            y + 1.8814 * c.x);
        if (hdrOutput != 0) {
            color = vec4(clamp(pqSignal,0.0,1.0),1.0);
            return; // retain original BT.2020 / PQ values; no SDR tone map
        }
        rgb = toSdr(pq(pqSignal));
    } else if (colors == 0) {
        rgb = vec3(y + 1.402 * c.y, y - 0.344136 * c.x - 0.714136 * c.y, y + 1.772 * c.x);
    } else {
        rgb = vec3(y + 1.5748 * c.y, y - 0.187324 * c.x - 0.468124 * c.y, y + 1.8556 * c.x);
    }
    color = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)GLSL";

// Upload happens before the UI draws. Restore caller state so fonts, artwork,
// the display framebuffer and even skipped UI frames remain independent.
struct State {
    GLint draw, read, viewport[4], program, vao, active, textures[2], alignment, row_length;
    GLint skip_rows, skip_pixels, unpack_buffer, swap_bytes;
    GLboolean blend, depth, cull, scissor, srgb, dither, mask[4];
    State() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        for (int i = 0; i < 2; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[i]);
        }
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buffer);
        glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swap_bytes);
        blend = glIsEnabled(GL_BLEND); depth = glIsEnabled(GL_DEPTH_TEST);
        cull = glIsEnabled(GL_CULL_FACE); scissor = glIsEnabled(GL_SCISSOR_TEST);
        srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB); dither = glIsEnabled(GL_DITHER);
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
    }
    ~State() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glUseProgram(program); glBindVertexArray(vao);
        for (int i = 0; i < 2; ++i) {
            glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, textures[i]);
        }
        glActiveTexture(active);
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
        glPixelStorei(GL_UNPACK_SWAP_BYTES, swap_bytes);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
        auto restore = [](GLenum cap, GLboolean on) { if (on) glEnable(cap); else glDisable(cap); };
        restore(GL_BLEND, blend); restore(GL_DEPTH_TEST, depth); restore(GL_CULL_FACE, cull);
        restore(GL_SCISSOR_TEST, scissor); restore(GL_FRAMEBUFFER_SRGB, srgb); restore(GL_DITHER, dither);
        glColorMask(mask[0], mask[1], mask[2], mask[3]);
    }
};

void texture_parameters(GLint filter) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
} // namespace

bool VideoSurface::upload(const VideoPicture& p, std::string* error, bool hdr_output) {
    auto fail = [&](const char* message) { if (error) *error = message; return false; };
    if (!video_dimensions_valid(p.w, p.h) ||
        (p.format == VideoFormat::Rgba ? !p.rgba : !p.y || !p.uv) ||
        (p.format == VideoFormat::Nv12_10 && p.shift != 0 && p.shift != 6))
        return fail("Invalid decoded video planes");
    if (hdr_output && (!p.hdr10_compatible || p.colors != YuvColors::Hdr10 || p.format == VideoFormat::Rgba))
        return fail("This picture cannot preserve HDR10 through the native YUV path");
    State state;
    const bool resize = !rgba_ || p.w != w_ || p.h != h_ || p.format != format_;
    glActiveTexture(GL_TEXTURE0);
    if (!rgba_) glGenTextures(1, &rgba_);
    glBindTexture(GL_TEXTURE_2D, rgba_);
    if (resize) {
        // Keep the converted 10-bit picture above 8-bit precision until final
        // composition. This does not itself make the HDMI scanout HDR.
        glTexImage2D(GL_TEXTURE_2D, 0, p.format == VideoFormat::Rgba ? GL_RGBA8 : GL_RGB10_A2,
                     p.w, p.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        texture_parameters(GL_LINEAR);
    }
    if (p.format == VideoFormat::Rgba) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, p.w, p.h, GL_RGBA, GL_UNSIGNED_BYTE, p.rgba);
    } else {
        if (!program_) {
            program_ = hui::gfx::build_program("native video YUV", kVertex, kFragment);
            if (!program_) return fail("Cannot compile the video color shader");
        }
        if (!vao_) glGenVertexArrays(1, &vao_);
        if (!y_) glGenTextures(1, &y_);
        if (!uv_) glGenTextures(1, &uv_);
        const bool wide = p.format == VideoFormat::Nv12_10;
        const GLenum type = wide ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;
        glBindTexture(GL_TEXTURE_2D, y_);
        if (resize) {
            glTexImage2D(GL_TEXTURE_2D, 0, wide ? GL_R16 : GL_R8, p.w, p.h, 0, GL_RED, type, nullptr);
            texture_parameters(GL_NEAREST);
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, p.w, p.h, GL_RED, type, p.y);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, uv_);
        const int cw = (p.w + 1) / 2, ch = (p.h + 1) / 2;
        if (resize) {
            glTexImage2D(GL_TEXTURE_2D, 0, wide ? GL_RG16 : GL_RG8, cw, ch, 0, GL_RG, type, nullptr);
            texture_parameters(GL_LINEAR);
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cw, ch, GL_RG, type, p.uv);
        if (!framebuffer_) glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rgba_, 0);
        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            return fail("Cannot allocate the native-resolution video surface");
        glViewport(0, 0, p.w, p.h);
        glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_DITHER);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glUseProgram(program_); glBindVertexArray(vao_);
        glUniform1i(0, 0); glUniform1i(1, 1);
        glUniform1f(2, wide ? 65535.0f / float(1 << p.shift) : 255.0f);
        glUniform1i(3, wide); glUniform1i(4, int(p.colors)); glUniform1i(5, p.full_range);
        glUniform1i(6, hdr_output);
        glUniform1f(7, hdr_peak_nits(p.hdr_peak_nits, 0));
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    if (glGetError() != GL_NO_ERROR) return fail("The GPU rejected the video picture");
    w_ = p.w; h_ = p.h; format_ = p.format;
    return true;
}

void VideoSurface::reset() {
    if (!rgba_ && !y_ && !uv_ && !framebuffer_ && !vao_ && !program_) return;
    const GLuint textures[] = {rgba_, y_, uv_};
    glDeleteTextures(3, textures);
    if (framebuffer_) glDeleteFramebuffers(1, &framebuffer_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
    rgba_ = y_ = uv_ = framebuffer_ = program_ = vao_ = 0;
    w_ = h_ = 0;
}
} // namespace sx
