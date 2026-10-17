// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_SURFACE_ANDROID_SURFACE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_SURFACE_ANDROID_SURFACE_H_

#include <functional>
#include <memory>
#include "flutter/fml/macros.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/embedder/embedder_surface.h"

namespace impeller {
class Context;
}  // namespace impeller

// From <android/surface_control.h>.
struct ASurfaceTransaction;

namespace flutter {

//------------------------------------------------------------------------------
/// @brief      Returns a transaction that the Vulkan swapchain adds its frames
///             to, or null if the swapchain should create and apply its own.
///             The embedder owns and applies a transaction it returns; with
///             HCPP, that is how a frame lands together with the platform
///             view changes of the same frame. Called on the raster thread.
///
using CreateSurfaceTransaction = std::function<ASurfaceTransaction*(
    std::function<void()>& publish_callback)>;

class AndroidExternalViewEmbedder;

class AndroidSurface : public EmbedderSurface {
 public:
  ~AndroidSurface() override;

  // |EmbedderSurface|
  bool IsValid() const override = 0;

  virtual void TeardownOnScreenContext() = 0;

  // |EmbedderSurface|
  std::unique_ptr<Surface> CreateGPUSurface() override;

  virtual std::unique_ptr<Surface> CreateGPUSurface(
      GrDirectContext* gr_context) = 0;

  virtual bool OnScreenSurfaceResize(const DlISize& size) = 0;

  virtual bool ResourceContextMakeCurrent() = 0;

  virtual bool ResourceContextClearCurrent() = 0;

  virtual bool SetNativeWindow(fml::RefPtr<AndroidNativeWindow> window) = 0;

  virtual std::unique_ptr<Surface> CreateSnapshotSurface();

  virtual std::shared_ptr<impeller::Context> GetImpellerContext();

  // |EmbedderSurface|
  std::shared_ptr<impeller::Context> CreateImpellerContext() const override;

  virtual void SetupImpellerSurface();

  // |EmbedderSurface|
  void SetupImpellerContext() override;

 protected:
  AndroidSurface();
};

class AndroidSurfaceFactory {
 public:
  AndroidSurfaceFactory() = default;

  virtual ~AndroidSurfaceFactory() = default;

  virtual std::unique_ptr<AndroidSurface> CreateSurface() = 0;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_SURFACE_ANDROID_SURFACE_H_
