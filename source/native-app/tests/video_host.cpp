// Reproduce the console's cropped, low-aligned Main10 output and render it
// with the production GPU shader. No PS5 hardware behavior is simulated here.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "video_surface.hpp"
#include "frame_pacer.hpp"
#include "hdr_compositor.hpp"
#include "gfx/renderer.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <iterator>
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
int checks = 0;
template<class F> void test(const char* name, F f) {
    f(); ++checks; std::printf("video: %s passed\n", name);
}

struct Source {
    int w, h, stride;
    bool wide;
    std::vector<uint8_t> y, uv;
    Source(int width, int height, bool ten, int padding = 0)
        : w(width), h(height), stride(((width + 1) & ~1) * (ten ? 2 : 1) + padding), wide(ten),
          y(size_t(stride) * (height + 2), 0xee), uv(size_t(stride) * ((height + 1) / 2), 0xee) {}
    void fill(int yy, int u, int v, int shift = 0) {
        auto put = [&](uint8_t* p, int value) {
            if (wide) { uint16_t word = uint16_t(value << shift); std::memcpy(p, &word, 2); }
            else *p = uint8_t(value);
        };
        const int bytes = wide ? 2 : 1;
        for (int row = 0; row < h; ++row)
            for (int x = 0; x < w; ++x) put(y.data() + row * stride + x * bytes, yy);
        for (int row = 0; row < (h + 1) / 2; ++row)
            for (int x = 0; x < (w + 1) / 2; ++x) {
                put(uv.data() + row * stride + x * 2 * bytes, u);
                put(uv.data() + row * stride + (x * 2 + 1) * bytes, v);
            }
    }
    YuvPicture view(int shift = 0) const {
        YuvPicture p;
        p.y = y.data(); p.u = uv.data(); p.width = w; p.height = h;
        p.y_stride = p.c_stride = stride; p.interleaved = true; p.wide = wide; p.shift = shift;
        return p;
    }
};

std::vector<uint8_t> render(sx::VideoSurface& surface, const VideoFrame& frame) {
    std::string error;
    require(surface.upload(frame.picture(), &error), error.c_str());
    GLint width, height;
    glBindTexture(GL_TEXTURE_2D, surface.texture());
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    require(width == frame.w && height == frame.h, "GPU texture lost native dimensions");
    std::vector<uint8_t> rgba(size_t(width) * height * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    require(glGetError() == GL_NO_ERROR, "GPU readback failed");
    return rgba;
}
void compare_reference(Source& src, int shift, YuvColors colors, int tolerance) {
    VideoFrame frame; frame.colors = colors;
    require(pack_video_yuv(src.view(shift), frame), "packing failed");
    sx::VideoSurface surface;
    auto actual = render(surface, frame);
    std::vector<uint8_t> expected(actual.size());
    yuv_to_rgba(src.view(shift), colors, 1, expected.data(), src.w, 0, src.h);
    for (size_t i = 0; i < actual.size(); ++i)
        require(std::abs(int(actual[i]) - int(expected[i])) <= tolerance, "GPU disagrees with color reference");
}
void gray(const std::vector<uint8_t>& p, size_t pixel, int low, int high) {
    pixel *= 4;
    require(p[pixel] >= low && p[pixel] <= high &&
        std::abs(int(p[pixel]) - int(p[pixel + 1])) <= 1 &&
        std::abs(int(p[pixel]) - int(p[pixel + 2])) <= 1, "green or incorrect gray picture");
}
} // namespace

#include "hdr_gpu_tests.hpp"
#include "sdr_gpu_tests.hpp"

int main(int argc, char** argv) try {
    require(argc == 2, "H.264 fixture required");
    auto get_display = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    require(get_display != nullptr, "no surfaceless EGL");
    EGLDisplay display = get_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    require(eglInitialize(display, nullptr, nullptr) && eglBindAPI(EGL_OPENGL_API), "EGL setup failed");
    const EGLint attrib[]{EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attrib);
    require(context != EGL_NO_CONTEXT && eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context), "no GL context");

    test("10-bit alignment including a black high-bit intro", [] {
        Source s(64, 64, true);
        s.fill(0, 512, 512); require(video_sample_shift(s.view()) == 0, "low bits misidentified");
        s.fill(0, 512, 512, 6); require(video_sample_shift(s.view()) == 6, "black high bits misidentified");
    });
    test("3840x1606 cropped low-bit picture survives decoder reuse", [] {
        Source s(3840, 1606, true, 128); s.fill(512, 512, 512);
        VideoFrame frame;
        require(pack_video_yuv(s.view(), frame), "cropped frame rejected");
        require(frame.pixels.size() == size_t(3840) * 1606 * 3, "crop or plane size incorrect");
        std::fill(s.y.begin(), s.y.end(), 0); std::fill(s.uv.begin(), s.uv.end(), 0);
        sx::VideoSurface surface; auto rgba = render(surface, frame);
        gray(rgba, 0, 128, 132); gray(rgba, size_t(3839) + 1605 * 3840, 128, 132);
    });
    test("odd planar frames with negative and unequal strides", [] {
        std::vector<uint8_t> y(24), u(8), v(10);
        for (int row = 0; row < 3; ++row) for (int x = 0; x < 5; ++x) y[(2-row)*8+x] = uint8_t(30+row);
        for (int row = 0; row < 2; ++row) for (int x = 0; x < 3; ++x) { u[(1-row)*4+x]=uint8_t(90+row); v[(1-row)*5+x]=uint8_t(140+row); }
        YuvPicture p; p.y=y.data()+16; p.u=u.data()+4; p.v=v.data()+5;
        p.width=5; p.height=3; p.y_stride=-8; p.c_stride=-4; p.v_stride=-5;
        VideoFrame f; require(pack_video_yuv(p, f), "negative strides rejected");
        require(f.pixels.size()==27 && f.pixels[0]==30 && f.pixels[14]==32 &&
            f.pixels[15]==90 && f.pixels[16]==140 && f.pixels[25]==91 && f.pixels[26]==141, "wrong planar copy");
    });
    test("invalid dimensions, planes and strides are rejected", [] {
        Source s(16,16,true); s.fill(512,512,512); VideoFrame f;
        auto p=s.view(); p.y_stride=16; require(!pack_video_yuv(p,f),"short stride accepted");
        p=s.view(); p.u=nullptr; require(!pack_video_yuv(p,f),"missing UV accepted");
        p=s.view(); p.shift=2; require(!pack_video_yuv(p,f),"invalid shift accepted");
        require(!video_dimensions_valid(0,2160) && !video_dimensions_valid(8192,4320),"invalid size accepted");
        sx::VideoSurface surface; std::string error;
        require(!surface.upload(f.picture(),&error) && !error.empty(),"empty GPU frame accepted");
    });
    test("SDR BT.601 and BT.709 in 8-bit and both 10-bit layouts", [] {
        for (bool wide : {false,true}) for (int shift : {0,6}) {
            if (!wide && shift) continue;
            Source s(16,8,wide); const int m=wide?4:1;
            for (YuvColors colors : {YuvColors::Bt601,YuvColors::Bt709}) {
                s.fill(100*m,110*m,155*m,shift); compare_reference(s,shift,colors,3);
            }
        }
    });
    test("10-bit grayscale precision survives GPU conversion", [] {
        Source s(877,2,true);
        sx::VideoSurface surface;
        for (int shift : {0,6}) {
            s.fill(64,512,512,shift);
            for (int y=0;y<2;++y) for (int x=0;x<877;++x) {
                const uint16_t word=uint16_t((64+x)<<shift);
                std::memcpy(s.y.data()+y*s.stride+x*2,&word,2);
            }
            VideoFrame f;
            require(pack_video_yuv(s.view(shift),f),"precision fixture packing failed");
            std::string error;
            require(surface.upload(f.picture(),&error),error.c_str());
            glBindTexture(GL_TEXTURE_2D,surface.texture());
            GLint bits = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_RED_SIZE,&bits);
            require(bits >= 10,"converted texture lost 10-bit storage");
            std::vector<float> values(877*2*4);
            glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,values.data());
            require(glGetError()==GL_NO_ERROR,"precision readback failed");
            for (int x=0;x<877;++x) {
                // UNORM conversion can truncate instead of round. Allow one
                // 10-bit step, then require all 877 levels to remain distinct.
                require(std::abs(values[x*4]-float(x)/876)<1.00001f/1023,"10-bit values quantized early");
                if (x) require(values[x*4]>values[(x-1)*4],"10-bit gradient collapsed to 8-bit");
            }
        }
    });
    test("frame submission pacing respects accepted refresh and recovers from hitches", [] {
        for (int hz : {60,120}) {
            sx::FramePacer p; p.configure(hz,1000000);
            std::int64_t now=1000000;
            for (int frame=0;frame<hz*60;++frame) {
                now+=p.delay_us(now);
                require(std::abs(double(now)-(1000000.0+frame*1000000.0/hz))<2.0,"pacing accumulated drift");
                p.started(now);
                now+=1000; // rendering takes less than one refresh
            }
            now+=2000000;
            require(p.delay_us(now)==0,"late frame slept unnecessarily");
            p.started(now);
            require(p.delay_us(now)>=8000,"hitch caused an unbounded catch-up burst");
            p.configure(0,now); require(p.refresh_hz()==60,"unknown rate must use conservative 60 Hz");
            p.configure(120,now); require(p.refresh_hz()==120,"accepted rate change ignored");
        }
    });
    test("full-range black and white", [] {
        Source s(8,8,true); VideoFrame f; f.full_range=true;
        sx::VideoSurface surface;
        s.fill(0,512,512); require(pack_video_yuv(s.view(),f),"packing failed");
        gray(render(surface,f),0,0,0);
        s.fill(1023,512,512); require(pack_video_yuv(s.view(),f),"packing failed");
        gray(render(surface,f),0,255,255);
    });
    test("GPU upload restores display, texture and pixel-store state", [] {
        GLuint fbo[2], tex[2], rect[2], pbo, vao;
        glGenFramebuffers(2,fbo); glGenTextures(2,tex); glGenTextures(2,rect); glGenBuffers(1,&pbo); glGenVertexArrays(1,&vao);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo[0]); glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo[1]);
        glBindVertexArray(vao); glViewport(7,9,11,13);
        for(int i=0;i<2;++i) { glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,tex[i]); glBindTexture(GL_TEXTURE_RECTANGLE,rect[i]); }
        glActiveTexture(GL_TEXTURE5); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);
        glPixelStorei(GL_UNPACK_ALIGNMENT,8); glPixelStorei(GL_UNPACK_ROW_LENGTH,77);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,3); glPixelStorei(GL_UNPACK_SKIP_PIXELS,5); glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_TRUE);
        glEnable(GL_BLEND); glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glEnable(GL_SCISSOR_TEST);
        glEnable(GL_FRAMEBUFFER_SRGB); glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);
        Source s(8,8,true); s.fill(512,512,512); VideoFrame f;
        require(pack_video_yuv(s.view(),f),"packing failed");
        sx::VideoSurface surface; std::string error;
        require(surface.upload(f.picture(),&error),error.c_str());
        auto equal=[](GLenum name,GLint expected) { GLint value; glGetIntegerv(name,&value); require(value==expected,"GL state changed"); };
        equal(GL_DRAW_FRAMEBUFFER_BINDING,fbo[0]); equal(GL_READ_FRAMEBUFFER_BINDING,fbo[1]);
        equal(GL_ACTIVE_TEXTURE,GL_TEXTURE5); equal(GL_VERTEX_ARRAY_BINDING,vao);
        equal(GL_PIXEL_UNPACK_BUFFER_BINDING,pbo); equal(GL_UNPACK_ALIGNMENT,8);
        equal(GL_UNPACK_ROW_LENGTH,77); equal(GL_UNPACK_SKIP_ROWS,3); equal(GL_UNPACK_SKIP_PIXELS,5); equal(GL_UNPACK_SWAP_BYTES,GL_TRUE);
        GLint viewport[4]; glGetIntegerv(GL_VIEWPORT,viewport);
        require(viewport[0]==7&&viewport[1]==9&&viewport[2]==11&&viewport[3]==13,"viewport changed");
        GLboolean mask[4]; glGetBooleanv(GL_COLOR_WRITEMASK,mask);
        require(!mask[0]&&mask[1]&&!mask[2]&&mask[3]&&glIsEnabled(GL_BLEND)&&glIsEnabled(GL_DEPTH_TEST)&&
            glIsEnabled(GL_CULL_FACE)&&glIsEnabled(GL_SCISSOR_TEST)&&glIsEnabled(GL_FRAMEBUFFER_SRGB),"raster state changed");
        for(int i=0;i<2;++i) { glActiveTexture(GL_TEXTURE0+i); equal(GL_TEXTURE_BINDING_2D,tex[i]); equal(GL_TEXTURE_BINDING_RECTANGLE,rect[i]); }
        glBindFramebuffer(GL_FRAMEBUFFER,0); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0); glBindVertexArray(0);
        glPixelStorei(GL_UNPACK_ALIGNMENT,4); glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0); glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_FALSE);
        glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glDeleteFramebuffers(2,fbo); glDeleteTextures(2,tex); glDeleteTextures(2,rect); glDeleteBuffers(1,&pbo); glDeleteVertexArrays(1,&vao);
    });
    test("format and size changes including RGBA software fallback", [] {
        sx::VideoSurface surface; VideoFrame f;
        f.w=3; f.h=5; f.pixels.resize(60,255);
        auto pixels=render(surface,f); require(pixels[0]==255&&pixels.back()==255,"RGBA upload incorrect");
        Source s(8,6,true); s.fill(512,512,512); require(pack_video_yuv(s.view(),f),"packing failed");
        gray(render(surface,f),0,128,132);
        Source s8(10,4,false); s8.fill(16,128,128); require(pack_video_yuv(s8.view(),f),"packing failed");
        gray(render(surface,f),0,0,0);
    });
    test("4K one-pixel detail survives the actual UI renderer", [] {
        Source s(3840,2160,true); s.fill(64,512,512);
        for(int row=0;row<s.h;++row) for(int x=0;x<s.w;++x) {
            uint16_t y=((row+x)&1)?940:64; std::memcpy(s.y.data()+row*s.stride+x*2,&y,2);
        }
        VideoFrame f; require(pack_video_yuv(s.view(),f),"packing failed");
        sx::VideoSurface surface; std::string error;
        require(surface.upload(f.picture(),&error),error.c_str());
        hui::gfx::Renderer renderer; require(renderer.init(),"UI renderer setup failed");
        GLuint framebuffer, color;
        glGenFramebuffers(1,&framebuffer); glGenTextures(1,&color);
        glBindTexture(GL_TEXTURE_2D,color); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,3840,2160,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"4K display framebuffer incomplete");
        hui::gfx::DrawList list; auto fit=video_fit(f.w,f.h,1,1920,1080);
        list.image(surface.texture(),{fit.x,fit.y,fit.w,fit.h},hui::gfx::kFullUv,hui::gfx::Color::rgb(0xffffff));
        renderer.begin(); renderer.draw(list); renderer.present(framebuffer,3840,2160);
        std::vector<uint8_t> rgba(size_t(3840)*2160*4); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
        glReadPixels(0,0,3840,2160,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        for(int row : {0,17,1079,2159}) for(int x : {0,1,31,1024,3838,3839}) {
            // UI top-down coordinates appear bottom-up in framebuffer readback.
            const bool white=((2159-row+x)&1)!=0;
            gray(rgba,size_t(row)*3840+x,white?254:0,white?255:1);
        }
        require(glGetError()==GL_NO_ERROR,"4K presentation GL error");
        glBindFramebuffer(GL_FRAMEBUFFER,0); glDeleteFramebuffers(1,&framebuffer); glDeleteTextures(1,&color);
        // Do not leave a deleted renderer program/VAO current for the next
        // isolated test. In the app the renderer outlives every video surface.
        glUseProgram(0); glBindVertexArray(0);
    });
    test("display fit preserves cinema crops and sample aspect ratio", [] {
        auto a=video_fit(3840,1606,1,1920,1080);
        require(a.w==1920&&std::abs(a.h-803)<0.01&&std::abs(a.y-138.5)<0.01,"cinema crop distorted");
        auto b=video_fit(720,576,16.0/15.0,1920,1080);
        require(std::abs(b.w-1440)<0.01&&b.h==1080&&b.x==240,"sample aspect ratio lost");
        auto c=video_fit(0,2160,1,1920,1080); require(c.w==0&&c.h==0,"invalid aspect accepted");
    });
    test("odd-sized low-bit HDR retains tone mapping and last row", [] {
        Source s(17,9,true,6); s.fill(512,512,512); VideoFrame f; f.colors=YuvColors::Hdr10;
        require(pack_video_yuv(s.view(),f),"odd HDR rejected");
        sx::VideoSurface surface; auto pixels=render(surface,f);
        gray(pixels,0,160,240); gray(pixels,152,160,240);
    });
    test("real H.264 High with B-frames decodes and renders all pictures", [&] {
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<uint8_t> input((std::istreambuf_iterator<char>(file)), {});
        require(!input.empty(), "missing H.264 clip");
        const size_t length = input.size(); input.resize(length + AV_INPUT_BUFFER_PADDING_SIZE, 0);
        const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        AVCodecContext* decoder = avcodec_alloc_context3(codec);
        AVCodecParserContext* parser = av_parser_init(AV_CODEC_ID_H264);
        AVFrame* picture = av_frame_alloc(); AVPacket* packet = av_packet_alloc();
        require(decoder && parser && picture && packet, "H.264 allocation failed");
        decoder->thread_count = 1;
        require(avcodec_open2(decoder, codec, nullptr) == 0, "H.264 decoder failed");
        sx::VideoSurface surface; int pictures = 0;
        auto receive = [&] {
            int rc;
            while ((rc = avcodec_receive_frame(decoder, picture)) == 0) {
                require(picture->width == 192 && picture->height == 108 &&
                    picture->format == AV_PIX_FMT_YUV420P, "unexpected decoded H.264 format");
                YuvPicture p; p.y = picture->data[0]; p.u = picture->data[1]; p.v = picture->data[2];
                p.width = picture->width; p.height = picture->height;
                p.y_stride = picture->linesize[0]; p.c_stride = picture->linesize[1]; p.v_stride = picture->linesize[2];
                VideoFrame frame; frame.colors = YuvColors::Bt709;
                require(pack_video_yuv(p, frame), "H.264 planes rejected");
                auto rgba = render(surface, frame); std::vector<uint8_t> reference(rgba.size());
                yuv_to_rgba(p, YuvColors::Bt709, 1, reference.data(), frame.w, 0, frame.h);
                int bright = 0;
                for (int channel = 0; channel < 3; ++channel) {
                    int64_t sum = 0;
                    for (size_t i = channel; i < rgba.size(); i += 4) sum += int(rgba[i]) - reference[i];
                    require(std::abs(double(sum) / (frame.w * frame.h)) < 3, "H.264 colors incorrect");
                }
                for (size_t i = 0; i < rgba.size(); i += 4)
                    if (rgba[i] > 32 || rgba[i + 1] > 32 || rgba[i + 2] > 32) ++bright;
                require(bright > frame.w * frame.h / 2, "H.264 rendered a black picture");
                ++pictures; av_frame_unref(picture);
            }
            require(rc == AVERROR(EAGAIN) || rc == AVERROR_EOF, "H.264 receive failed");
        };
        size_t offset = 0;
        while (offset < length) {
            uint8_t* data = nullptr; int size = 0;
            int used = av_parser_parse2(parser, decoder, &data, &size, input.data() + offset,
                int(length - offset), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            require(used >= 0 && (used || size), "H.264 parser stalled"); offset += used;
            if (size) { packet->data = data; packet->size = size;
                require(avcodec_send_packet(decoder, packet) == 0, "H.264 send failed"); receive(); }
        }
        uint8_t* data = nullptr; int size = 0;
        av_parser_parse2(parser, decoder, &data, &size, nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (size) { packet->data = data; packet->size = size;
            require(avcodec_send_packet(decoder, packet) == 0, "H.264 last packet failed"); receive(); }
        require(avcodec_send_packet(decoder, nullptr) == 0, "H.264 flush failed"); receive();
        require(pictures == 12 && decoder->has_b_frames > 0, "H.264 delayed pictures lost");
        av_packet_free(&packet); av_frame_free(&picture); av_parser_close(parser); avcodec_free_context(&decoder);
    });
    hdr_gpu_tests();
    sdr_gpu_tests();
    std::printf("Video packing and GPU presentation: %d tests, 0 failures\n",checks);
    return 0;
} catch(const std::exception& e) {
    std::fprintf(stderr,"Video failure after %d checks: %s\n",checks,e.what()); return 1;
}
