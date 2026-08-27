// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_VIEW_EMBEDDER_SURFACE_POOL_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_VIEW_EMBEDDER_SURFACE_POOL_H_

#include <mutex>
#include <vector>

#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/shell/platform/android/surface/android_surface.h"

namespace flutter {

struct OverlayLayer {
  OverlayLayer(int id, std::unique_ptr<AndroidSurface> android_surface);

  ~OverlayLayer();

  const int id;
  const std::unique_ptr<AndroidSurface> android_surface;
};

class SurfacePool {
 public:
  explicit SurfacePool(bool use_new_surface_methods);

  ~SurfacePool();

  std::shared_ptr<OverlayLayer> GetLayer(
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
      const std::shared_ptr<AndroidSurfaceFactory>& surface_factory);

  std::vector<std::shared_ptr<OverlayLayer>> GetUnusedLayers();

  void RecycleLayers();

  bool HasLayers();

  void ResetLayers();

  void TrimLayers();

  void SetFrameSize(DlISize size);

  void DestroyLayers(const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade);

  std::vector<std::shared_ptr<OverlayLayer>> GetLayers() const;

 private:
  void DestroyLayersLocked(
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade);

  mutable std::mutex mutex_;
  std::vector<std::shared_ptr<OverlayLayer>> layers_;
  size_t available_layer_index_ = 0;
  DlISize requested_frame_size_;
  DlISize current_frame_size_;
  const bool use_new_surface_methods_;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_VIEW_EMBEDDER_SURFACE_POOL_H_
