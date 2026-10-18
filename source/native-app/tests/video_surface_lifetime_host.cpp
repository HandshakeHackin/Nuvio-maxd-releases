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
void glDeleteTextures(GLsizei,const GLuint*) {}
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
    std::puts("Video lifetime: timeout/failure retain texture fence, completion releases, repeats and fallback passed");
}
