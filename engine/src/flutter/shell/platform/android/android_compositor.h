// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/shell/platform/android/android_surface_manager.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

//------------------------------------------------------------------------------
/// @brief Coordinates backing store management, layer composition, and platform
///        view presentation between the Flutter engine and the Android platform.
///
class AndroidCompositor {
 public:
  /// @brief Metadata recorded during each frame presentation.
  struct PresentedFrame {
    FlutterViewId view_id = 0;
    size_t backing_store_count = 0;
    size_t platform_view_count = 0;
    std::vector<FlutterPlatformViewIdentifier> platform_view_ids;
    uint64_t presentation_time = 0;
  };

  AndroidCompositor(
      std::shared_ptr<AndroidSurfaceManager> surface_manager,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade);

  virtual ~AndroidCompositor();

  //----------------------------------------------------------------------------
  /// @brief Returns the initialized FlutterCompositor struct for FlutterProjectArgs.
  ///
  FlutterCompositor GetCompositor();

  //----------------------------------------------------------------------------
  /// @brief Allocates or retrieves a backing store for rendering.
  ///
  virtual bool CreateBackingStore(const FlutterBackingStoreConfig* config,
                                  FlutterBackingStore* backing_store_out);

  //----------------------------------------------------------------------------
  /// @brief Releases or recycles a backing store.
  ///
  virtual bool CollectBackingStore(const FlutterBackingStore* store);

  //----------------------------------------------------------------------------
  /// @brief Composites and presents the provided layers for the specified view.
  ///
  virtual bool Present(FlutterViewId view_id,
                       const FlutterLayer** layers,
                       size_t layers_count);

  //----------------------------------------------------------------------------
  /// @brief Registers a view with the compositor.
  ///
  virtual void AddView(FlutterViewId view_id);

  //----------------------------------------------------------------------------
  /// @brief Unregisters a view from the compositor.
  ///
  virtual void RemoveView(FlutterViewId view_id);

  //----------------------------------------------------------------------------
  /// @brief Returns the last presented frame metadata (useful for verification).
  ///
  PresentedFrame GetLastPresentedFrame() const;

  //----------------------------------------------------------------------------
  /// @brief Returns the number of registered views.
  ///
  size_t GetViewCount() const;

  //----------------------------------------------------------------------------
  /// @brief Accessor for the associated AndroidSurfaceManager.
  ///
  const std::shared_ptr<AndroidSurfaceManager>& GetSurfaceManager() const {
    return surface_manager_;
  }

  //----------------------------------------------------------------------------
  /// @brief Accessor for the associated PlatformViewAndroidJNI facade.
  ///
  const std::shared_ptr<PlatformViewAndroidJNI>& GetJniFacade() const {
    return jni_facade_;
  }

 private:
  std::shared_ptr<AndroidSurfaceManager> surface_manager_;
  std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;

  mutable std::mutex mutex_;
  std::unordered_map<FlutterViewId, bool> active_views_;
  PresentedFrame last_presented_frame_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidCompositor);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_
