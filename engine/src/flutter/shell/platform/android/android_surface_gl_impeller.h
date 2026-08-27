// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_GL_IMPELLER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_GL_IMPELLER_H_

#include <optional>
#include "flutter/fml/macros.h"
#include "flutter/impeller/renderer/context.h"
#include "flutter/impeller/toolkit/egl/surface.h"
#include "flutter/shell/platform/android/android_context_gl_impeller.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/android/surface/android_surface.h"

namespace flutter {

class AndroidSurfaceGLImpeller final : public AndroidSurface {
 public:
  explicit AndroidSurfaceGLImpeller(
      const std::shared_ptr<AndroidContextGLImpeller>& android_context);

  // |AndroidSurface|
  ~AndroidSurfaceGLImpeller() override;

  // |AndroidSurface|
  bool IsValid() const override;

  // |AndroidSurface|
  void TeardownOnScreenContext() override;

  // |AndroidSurface|
  bool OnScreenSurfaceResize(const DlISize& size) override;

  // |AndroidSurface|
  bool ResourceContextMakeCurrent() override;

  // |AndroidSurface|
  bool ResourceContextClearCurrent() override;

  // |AndroidSurface|
  bool OnGLContextMakeCurrent() override;

  // |AndroidSurface|
  bool GLContextClearCurrent() override;

  // |AndroidSurface|
  bool SetNativeWindow(
      fml::RefPtr<AndroidNativeWindow> window,
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) override;

  // |AndroidSurface|
  bool PresentOnscreenSurface() override;

  // |AndroidSurface|
  std::shared_ptr<impeller::Context> GetImpellerContext() override;

 private:
  const std::shared_ptr<AndroidContextGLImpeller> android_context_;
  fml::RefPtr<AndroidNativeWindow> native_window_;
  std::unique_ptr<impeller::egl::Surface> onscreen_surface_;
  std::unique_ptr<impeller::egl::Surface> offscreen_surface_;
  std::unique_ptr<impeller::egl::Surface> raster_pbuffer_surface_;
  bool is_valid_ = false;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidSurfaceGLImpeller);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_GL_IMPELLER_H_
