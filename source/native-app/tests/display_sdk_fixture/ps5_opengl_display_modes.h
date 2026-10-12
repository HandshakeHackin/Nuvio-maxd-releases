// Host-only boundary declarations; match ps5-opengl SDK v1.0.0.
// The fixture supplies no implementation and cannot confirm HDMI behavior.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <EGL/egl.h>
#define PS5_OPENGL_DYNAMIC_DISPLAY 1
extern "C" {
EGLBoolean eglSetDisplayModePS5(EGLDisplay, EGLint, EGLint);
EGLBoolean eglSetDisplayRefreshPS5(EGLDisplay, EGLint);
EGLBoolean eglGetDisplayModePS5(EGLDisplay, EGLint*, EGLint*, EGLint*);
}
