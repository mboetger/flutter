// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_gl_impeller.h"

#include <utility>

#include "flutter/fml/logging.h"

namespace flutter {

AndroidSurfaceGLImpeller::AndroidSurfaceGLImpeller(
    const std::shared_ptr<AndroidContextGLImpeller>& android_context)
    : android_context_(android_context) {
  offscreen_surface_ = android_context_->CreateOffscreenSurface();
  raster_pbuffer_surface_ = android_context_->CreateOffscreenSurface();

  if (!offscreen_surface_ || !raster_pbuffer_surface_) {
    FML_DLOG(ERROR) << "Could not create offscreen surfaces.";
    return;
  }

  is_valid_ = true;
}

AndroidSurfaceGLImpeller::~AndroidSurfaceGLImpeller() = default;

bool AndroidSurfaceGLImpeller::IsValid() const {
  return is_valid_;
}

void AndroidSurfaceGLImpeller::TeardownOnScreenContext() {
  GLContextClearCurrent();
  onscreen_surface_.reset();
}

bool AndroidSurfaceGLImpeller::OnScreenSurfaceResize(const DlISize& size) {
  if (!native_window_) {
    return false;
  }
  onscreen_surface_.reset();
  onscreen_surface_ =
      android_context_->CreateOnscreenSurface(native_window_->handle());
  if (!onscreen_surface_) {
    FML_DLOG(ERROR) << "Could not create onscreen surface.";
    return false;
  }
  return true;
}

bool AndroidSurfaceGLImpeller::ResourceContextMakeCurrent() {
  if (!offscreen_surface_) {
    FML_LOG(ERROR) << "AndroidSurfaceGLImpeller::ResourceContextMakeCurrent: "
                      "offscreen_surface_ is null.";
    return false;
  }
  bool success =
      android_context_->ResourceContextMakeCurrent(offscreen_surface_.get());
  if (!success) {
    FML_LOG(ERROR) << "AndroidSurfaceGLImpeller::ResourceContextMakeCurrent: "
                      "android_context_->ResourceContextMakeCurrent failed.";
  }
  return success;
}

bool AndroidSurfaceGLImpeller::ResourceContextClearCurrent() {
  return android_context_->ResourceContextClearCurrent();
}

bool AndroidSurfaceGLImpeller::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> window,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) {
  native_window_ = std::move(window);
  if (!native_window_) {
    onscreen_surface_.reset();
    return true;
  }
  onscreen_surface_.reset();
  onscreen_surface_ =
      android_context_->CreateOnscreenSurface(native_window_->handle());
  if (!onscreen_surface_) {
    FML_DLOG(ERROR) << "Could not create onscreen surface.";
    return false;
  }
  return true;
}

bool AndroidSurfaceGLImpeller::PresentOnscreenSurface() {
  if (!onscreen_surface_) {
    return false;
  }
  return onscreen_surface_->Present();
}

std::shared_ptr<impeller::Context>
AndroidSurfaceGLImpeller::GetImpellerContext() {
  return android_context_->GetImpellerContext();
}

bool AndroidSurfaceGLImpeller::OnGLContextMakeCurrent() {
  if (onscreen_surface_ && onscreen_surface_->IsValid()) {
    return android_context_->OnscreenContextMakeCurrent(
        onscreen_surface_.get());
  }
  if (raster_pbuffer_surface_ && raster_pbuffer_surface_->IsValid()) {
    return android_context_->RasterPbufferContextMakeCurrent(
        raster_pbuffer_surface_.get());
  }
  return false;
}

bool AndroidSurfaceGLImpeller::GLContextClearCurrent() {
  if (onscreen_surface_ && onscreen_surface_->IsValid()) {
    return android_context_->OnscreenContextClearCurrent();
  }
  if (raster_pbuffer_surface_ && raster_pbuffer_surface_->IsValid()) {
    return android_context_->RasterPbufferContextClearCurrent();
  }
  return false;
}

}  // namespace flutter
