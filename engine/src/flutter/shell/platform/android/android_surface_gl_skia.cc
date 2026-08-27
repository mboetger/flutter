// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_gl_skia.h"

#include <utility>

#include "flutter/fml/logging.h"
#include "flutter/shell/platform/android/android_egl_surface.h"

namespace flutter {

AndroidSurfaceGLSkia::AndroidSurfaceGLSkia(
    const std::shared_ptr<AndroidContextGLSkia>& android_context)
    : android_context_(android_context) {
  // Create the offscreen surface.
  offscreen_surface_ = android_context_->CreateOffscreenSurface();
}

AndroidSurfaceGLSkia::~AndroidSurfaceGLSkia() {
  TeardownOnScreenContext();
}

void AndroidSurfaceGLSkia::TeardownOnScreenContext() {
  if (IsValid()) {
    android_context_->ClearCurrent();
  }
  onscreen_surface_ = nullptr;
}

bool AndroidSurfaceGLSkia::IsValid() const {
  return offscreen_surface_ && android_context_->IsValid();
}

bool AndroidSurfaceGLSkia::OnScreenSurfaceResize(const DlISize& size) {
  FML_DCHECK(IsValid());
  FML_DCHECK(onscreen_surface_);
  FML_DCHECK(native_window_);

  if (size == onscreen_surface_->GetSize()) {
    return true;
  }

  android_context_->ClearCurrent();

  onscreen_surface_ = nullptr;
  onscreen_surface_ = android_context_->CreateOnscreenSurface(native_window_);
  if (!onscreen_surface_->IsValid()) {
    FML_LOG(ERROR) << "Unable to create EGL window surface on resize.";
    return false;
  }
  onscreen_surface_->MakeCurrent();
  return true;
}

bool AndroidSurfaceGLSkia::ResourceContextMakeCurrent() {
  FML_DCHECK(IsValid());
  auto status = offscreen_surface_->MakeCurrent();
  return status != AndroidEGLSurfaceMakeCurrentStatus::kFailure;
}

bool AndroidSurfaceGLSkia::ResourceContextClearCurrent() {
  FML_DCHECK(IsValid());
  EGLBoolean result = eglMakeCurrent(eglGetCurrentDisplay(), EGL_NO_SURFACE,
                                     EGL_NO_SURFACE, EGL_NO_CONTEXT);
  return result == EGL_TRUE;
}

bool AndroidSurfaceGLSkia::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> window,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) {
  native_window_ = window;

  if (!native_window_ || !native_window_->IsValid()) {
    TeardownOnScreenContext();
    return false;
  }

  auto onscreen_surface =
      android_context_->CreateOnscreenSurface(native_window_);
  if (!onscreen_surface->IsValid()) {
    return false;
  }
  onscreen_surface_ = std::move(onscreen_surface);
  return true;
}

bool AndroidSurfaceGLSkia::PresentOnscreenSurface() {
  if (!onscreen_surface_ || !onscreen_surface_->IsValid()) {
    return false;
  }
  return onscreen_surface_->SwapBuffers(std::nullopt);
}

bool AndroidSurfaceGLSkia::OnGLContextMakeCurrent() {
  FML_DCHECK(IsValid());
  if (onscreen_surface_) {
    auto status = onscreen_surface_->MakeCurrent();
    return status != AndroidEGLSurfaceMakeCurrentStatus::kFailure;
  }
  if (offscreen_surface_) {
    auto status = offscreen_surface_->MakeCurrent();
    return status != AndroidEGLSurfaceMakeCurrentStatus::kFailure;
  }
  return false;
}

bool AndroidSurfaceGLSkia::GLContextClearCurrent() {
  FML_DCHECK(IsValid());
  return android_context_->ClearCurrent();
}

}  // namespace flutter
