// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_GL_SKIA_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_GL_SKIA_H_

#include <jni.h>
#include <memory>

#include "flutter/fml/macros.h"
#include "flutter/shell/platform/android/android_context_gl_skia.h"
#include "flutter/shell/platform/android/android_environment_gl.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/surface/android_surface.h"

namespace flutter {

class AndroidSurfaceGLSkia final : public AndroidSurface {
 public:
  explicit AndroidSurfaceGLSkia(
      const std::shared_ptr<AndroidContextGLSkia>& android_context);

  ~AndroidSurfaceGLSkia() override;

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

  const AndroidEGLSurface* GetOnscreenSurface() const {
    return onscreen_surface_.get();
  }

 private:
  const std::shared_ptr<AndroidContextGLSkia> android_context_;
  fml::RefPtr<AndroidNativeWindow> native_window_;
  std::unique_ptr<AndroidEGLSurface> onscreen_surface_;
  std::unique_ptr<AndroidEGLSurface> offscreen_surface_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidSurfaceGLSkia);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_GL_SKIA_H_
