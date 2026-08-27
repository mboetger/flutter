// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_software.h"

namespace flutter {

AndroidSurfaceSoftware::AndroidSurfaceSoftware() = default;

AndroidSurfaceSoftware::~AndroidSurfaceSoftware() = default;

bool AndroidSurfaceSoftware::IsValid() const {
  return true;
}

bool AndroidSurfaceSoftware::ResourceContextMakeCurrent() {
  return false;
}

bool AndroidSurfaceSoftware::ResourceContextClearCurrent() {
  return false;
}

void AndroidSurfaceSoftware::TeardownOnScreenContext() {
  native_window_ = nullptr;
}

bool AndroidSurfaceSoftware::OnScreenSurfaceResize(const DlISize& size) {
  return true;
}

bool AndroidSurfaceSoftware::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> window,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) {
  native_window_ = window;
  return native_window_ && native_window_->IsValid();
}

}  // namespace flutter
