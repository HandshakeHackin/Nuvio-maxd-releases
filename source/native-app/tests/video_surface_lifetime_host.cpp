#include <string>
#include "video_frame.h"
#include <GL/glcorearb.h>
#define private public
#include "video_surface.hpp"
#undef private
#include <cassert>
#include <cstdio>
static GLenum wait_result=GL_TIMEOUT_EXPIRED;
static int waits=0, deletes=0, finishes=0;
static bool fence_fail=false;
static GLint bound_draw=77;static bool detached=false;static int freed_textures=0;
extern "C" {
GLsync glFenceSync(GLenum condition,GLbitfield flags) {
    assert(condition==GL_SYNC_GPU_COMMANDS_COMPLETE&&!flags);
    return fence_fail ? nullptr : reinterpret_cast<GLsync>(uintptr_t(1234));
}
GLenum glClientWaitSync(GLsync fence,GLbitfield flags,GLuint64 timeout) {
    assert(fence&&flags==GL_SYNC_FLUSH_COMMANDS_BIT&&timeout==100000000); ++waits; return wait_result;
}
void glDeleteSync(GLsync sync) { assert(sync); ++deletes; }
void glFinish() { ++finishes; }
void glGetIntegerv(GLenum what,GLint* value) {assert(what==GL_DRAW_FRAMEBUFFER_BINDING);*value=bound_draw;}
void glBindFramebuffer(GLenum target,GLuint value) {assert(target==GL_DRAW_FRAMEBUFFER);bound_draw=GLint(value);}
void glFramebufferTexture2D(GLenum target,GLenum attachment,GLenum,GLuint texture,GLint) {
 assert(target==GL_DRAW_FRAMEBUFFER&&attachment==GL_COLOR_ATTACHMENT0&&bound_draw==88&&!texture);detached=true;
}
void glDeleteTextures(GLsizei n,const GLuint* ids) {for(int i=0;i<n;++i)if(ids[i]){assert(detached);++freed_textures;}}
void glDeleteFramebuffers(GLsizei,const GLuint*) {}
void glDeleteVertexArrays(GLsizei,const GLuint*) {}
void glDeleteProgram(GLuint) {}
}
int main() {
    sx::VideoSurface s; std::string error;
    assert(s.retire(&error)&&waits==0);
    s.submitted(); assert(s.in_flight_);
    assert(!s.retire(&error)&&s.in_flight_&&deletes==0); // timeout retains storage
    wait_result=GL_WAIT_FAILED; assert(!s.retire(&error)&&s.in_flight_);
    wait_result=GL_CONDITION_SATISFIED; assert(s.retire(&error)&&!s.in_flight_&&deletes==1);
    s.submitted(); s.submitted(); assert(deletes==2&&s.in_flight_); // newer fence covers repeats
    fence_fail=true; s.submitted(); assert(finishes==1&&!s.in_flight_&&deletes==3);
    s.framebuffer_=88;s.rgba_=1;s.y_=2;s.uv_=3;s.w_=3840;s.h_=2160;
    s.release_textures();assert(detached&&freed_textures==3&&bound_draw==77);
    assert(!s.texture()&&!s.y_&&!s.uv_&&!s.w_&&!s.h_);
    s.reset();assert(bound_draw==77);
    std::puts("Video lifetime: timeout/failure retain texture fence, completion releases, repeats and fallback passed");
}
