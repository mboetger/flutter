// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_SOFTWARE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_SOFTWARE_H_

#include "flutter/fml/macros.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/surface/android_surface.h"

namespace flutter {

class AndroidSurfaceSoftware final : public AndroidSurface {
 public:
  AndroidSurfaceSoftware();

  ~AndroidSurfaceSoftware() override;

  // |AndroidSurface|
  bool IsValid() const override;

  // |AndroidSurface|
  bool ResourceContextMakeCurrent() override;

  // |AndroidSurface|
  bool ResourceContextClearCurrent() override;

  // |AndroidSurface|
  void TeardownOnScreenContext() override;

  // |AndroidSurface|
  bool OnScreenSurfaceResize(const DlISize& size) override;

  // |AndroidSurface|
  bool SetNativeWindow(
      fml::RefPtr<AndroidNativeWindow> window,
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) override;

 private:
  fml::RefPtr<AndroidNativeWindow> native_window_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidSurfaceSoftware);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_SOFTWARE_H_
