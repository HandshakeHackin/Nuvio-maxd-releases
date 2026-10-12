// Mock the EGL boundary; exercise the production PS5 display lifecycle.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/ps5/display_egl.hpp"
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <ps5_opengl_display_modes.h>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
int width=1920,height=1080,requested=60,accepted=60,initializations=0;
bool initialized=false,set_120_ok=true,set_60_ok=true,readback_ok=true;
std::vector<int> refresh_requests;
int checks=0;
void require(bool ok,const char* why) { if (!ok) throw std::runtime_error(why); }
void reset() {
    width=1920; height=1080; requested=accepted=60; initializations=0;
    initialized=false; set_120_ok=set_60_ok=readback_ok=true; refresh_requests.clear();
}
template<class F> void test(const char* name,F f) {
    reset(); f(); ++checks; std::printf("Display refresh: %s passed\n",name);
}
}
namespace hui::sys { void log(const char*,...) {} }
extern "C" {
EGLDisplay eglGetDisplay(EGLNativeDisplayType) { return reinterpret_cast<EGLDisplay>(1); }
EGLint eglGetError() { return EGL_BAD_PARAMETER; }
EGLBoolean eglSetDisplayModePS5(EGLDisplay,EGLint w,EGLint h) {
    require(!initialized,"resolution changed after EGL initialization"); width=w; height=h; return EGL_TRUE;
}
EGLBoolean eglSetDisplayRefreshPS5(EGLDisplay,EGLint hz) {
    require(!initialized,"refresh changed after EGL initialization");
    refresh_requests.push_back(hz);
    const bool ok=hz==120 ? set_120_ok : hz==60 && set_60_ok;
    if (ok) requested=hz;
    return ok ? EGL_TRUE : EGL_FALSE;
}
EGLBoolean eglGetDisplayModePS5(EGLDisplay,EGLint*,EGLint*,EGLint* hz) {
    if (!readback_ok) return EGL_FALSE;
    *hz=accepted; return EGL_TRUE;
}
EGLBoolean eglInitialize(EGLDisplay,EGLint* major,EGLint* minor) {
    require(!refresh_requests.empty(),"EGL initialized before refresh was selected");
    initialized=true; ++initializations; *major=1; *minor=4; return EGL_TRUE;
}
EGLBoolean eglBindAPI(EGLenum) { return EGL_TRUE; }
EGLBoolean eglChooseConfig(EGLDisplay,const EGLint*,EGLConfig* config,EGLint,EGLint* count) {
    *config=reinterpret_cast<EGLConfig>(1); *count=1; return EGL_TRUE;
}
EGLSurface eglCreateWindowSurface(EGLDisplay,EGLConfig,EGLNativeWindowType,const EGLint*) {
    return reinterpret_cast<EGLSurface>(1);
}
EGLContext eglCreateContext(EGLDisplay,EGLConfig,EGLContext,const EGLint*) {
    return reinterpret_cast<EGLContext>(1);
}
EGLBoolean eglMakeCurrent(EGLDisplay,EGLSurface,EGLSurface,EGLContext) { return EGL_TRUE; }
EGLBoolean eglQuerySurface(EGLDisplay,EGLSurface,EGLint attribute,EGLint* value) {
    *value=attribute==EGL_WIDTH ? width : height; return EGL_TRUE;
}
EGLBoolean eglSwapInterval(EGLDisplay,EGLint interval) {
    require(interval==1,"display lost vsync"); return EGL_TRUE;
}
EGLBoolean eglSwapBuffers(EGLDisplay,EGLSurface) { return EGL_TRUE; }
EGLBoolean eglDestroyContext(EGLDisplay,EGLContext) { return EGL_TRUE; }
EGLBoolean eglDestroySurface(EGLDisplay,EGLSurface) { return EGL_TRUE; }
EGLBoolean eglTerminate(EGLDisplay) { initialized=false; return EGL_TRUE; }
void glGetIntegerv(GLenum name,GLint* value) { *value=name==GL_MAJOR_VERSION ? 4 : 6; }
const GLubyte* glGetString(GLenum) { return reinterpret_cast<const GLubyte*>("host boundary fixture"); }
}
int main() try {
    using hui::ps5::Display;
    test("4K 120 Hz is selected before EGL and confirmed after swap",[] {
        Display d; accepted=120;
        require(d.open(3840,2160,120),"open refused");
        require(requested==120&&initializations==1&&d.width()==3840&&d.height()==2160,"request altered geometry or order");
        require(d.refresh_hz()==0,"unpresented request was reported as accepted");
        require(d.swap()&&d.refresh_hz()==120,"accepted refresh was lost");
    });
    test("rejected refresh request retries 60 Hz at the same 4K resolution",[] {
        Display d; set_120_ok=false;
        require(d.open(3840,2160,120)&&d.swap(),"60 Hz retry failed");
        require(refresh_requests==std::vector<int>({120,60})&&d.width()==3840&&d.height()==2160&&d.refresh_hz()==60,
                "refresh fallback downgraded resolution or claimed 120 Hz");
    });
    test("two refused refresh requests stop before creating EGL resources",[] {
        Display d; set_120_ok=set_60_ok=false;
        require(!d.open(3840,2160,120)&&initializations==0&&d.refresh_hz()==0,"failed request opened EGL anyway");
    });
    test("hardware refusal of 120 Hz is reported as accepted 60 Hz",[] {
        Display d;
        require(d.open(3840,2160,120)&&d.swap(),"open refused");
        require(requested==120&&d.refresh_hz()==60&&d.width()==3840&&d.height()==2160,"request was mistaken for output confirmation");
    });
    test("failed readback does not claim high refresh",[] {
        Display d; readback_ok=false; accepted=120;
        require(d.open(3840,2160,120)&&d.swap()&&d.refresh_hz()==0,"failed readback confirmed a request");
        readback_ok=true;
        require(d.swap()&&d.refresh_hz()==120,"later confirmation was ignored");
        d.close(); require(d.refresh_hz()==0,"close left a stale accepted rate");
    });
    test("default and invalid stored refresh values use 60 Hz",[] {
        Display d;
        require(d.open()&&requested==60&&d.swap()&&d.refresh_hz()==60,"default changed");
        d.close(); refresh_requests.clear();
        require(d.open(3840,2160,30)&&requested==60&&d.swap(),"invalid preference escaped normalization");
    });
    std::printf("Display refresh lifecycle: %d tests, 0 failures\n",checks); return 0;
} catch(const std::exception& e) { std::fprintf(stderr,"Display refresh FAILED: %s\n",e.what()); return 1; }
