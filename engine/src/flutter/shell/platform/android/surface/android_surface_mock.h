// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_SURFACE_ANDROID_SURFACE_MOCK_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_SURFACE_ANDROID_SURFACE_MOCK_H_

#include "flutter/shell/platform/android/surface/android_surface.h"
#include "gmock/gmock.h"

namespace flutter {

//------------------------------------------------------------------------------
/// Mock for |AndroidSurface|. This implementation can be used in unit
/// tests without requiring the Android toolchain.
///
class AndroidSurfaceMock : public AndroidSurface {
 public:
  MOCK_METHOD(bool, IsValid, (), (const, override));

  MOCK_METHOD(void, TeardownOnScreenContext, (), (override));

  MOCK_METHOD(bool, OnScreenSurfaceResize, (const DlISize& size), (override));

  MOCK_METHOD(bool, ResourceContextMakeCurrent, (), (override));

  MOCK_METHOD(bool, ResourceContextClearCurrent, (), (override));

  MOCK_METHOD(bool, OnGLContextMakeCurrent, (), (override));

  MOCK_METHOD(bool, GLContextClearCurrent, (), (override));

  MOCK_METHOD(bool,
              SetNativeWindow,
              (fml::RefPtr<AndroidNativeWindow> window,
               const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade),
              (override));

  MOCK_METHOD(bool, PresentOnscreenSurface, (), (override));

  MOCK_METHOD(std::shared_ptr<impeller::Context>,
              GetImpellerContext,
              (),
              (override));

  MOCK_METHOD(void, SetupImpellerSurface, (), (override));
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_SURFACE_ANDROID_SURFACE_MOCK_H_
