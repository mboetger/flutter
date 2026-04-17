// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_OPENGL_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_OPENGL_H_

#include <memory>
#include <vector>

#include <android/native_window.h>
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "impeller/toolkit/android/surface_control.h"

namespace flutter {

class AndroidSurface;

class AndroidCompositorOpenGL {
 public:
  explicit AndroidCompositorOpenGL(AndroidSurface* android_surface,
                                   AndroidRenderingAPI rendering_api);
  ~AndroidCompositorOpenGL();

  void SetNativeWindow(ANativeWindow* window);

  static bool CreateBackingStoreCallback(
      const FlutterBackingStoreConfig* config,
      FlutterBackingStore* backing_store_out,
      void* user_data);

  static bool CollectBackingStoreCallback(
      const FlutterBackingStore* backing_store,
      void* user_data);

  static bool PresentViewCallback(const FlutterPresentViewInfo* info);

  bool PresentView(const FlutterPresentViewInfo* info);

 private:
  bool CreateBackingStore(const FlutterBackingStoreConfig* config,
                          FlutterBackingStore* backing_store_out);

  bool CollectBackingStore(const FlutterBackingStore* backing_store);

  ANativeWindow* window_ = nullptr;
  std::unique_ptr<impeller::android::SurfaceControl> root_surface_control_;
  AndroidSurface* android_surface_ = nullptr;
  AndroidRenderingAPI rendering_api_;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_OPENGL_H_
