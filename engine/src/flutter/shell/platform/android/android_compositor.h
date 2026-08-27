// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_

#include <atomic>
#include <memory>
#include <vector>

#include "flutter/common/task_runners.h"
#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/shell/platform/android/android_mutators_stack.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/external_view_embedder/surface_pool.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace flutter {

//------------------------------------------------------------------------------
/// @brief      Compositor implementation for the Flutter Android embedder
///             backed by the public Embedder API (`embedder.h`).
///
///             Provides backing store allocation and layer presentation
///             callbacks conforming to `FlutterCompositor`. Bridges Flutter
///             render layers and embedded platform views to Android surfaces
///             and JNI calls.
///
class AndroidCompositor
    : public std::enable_shared_from_this<AndroidCompositor> {
 public:
  AndroidCompositor(std::shared_ptr<AndroidContext> android_context,
                    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                    std::shared_ptr<AndroidSurfaceFactory> surface_factory,
                    const TaskRunners& task_runners);

  ~AndroidCompositor();

  //----------------------------------------------------------------------------
  /// @brief Returns a FlutterCompositor struct configured with callbacks
  ///        pointing to this AndroidCompositor instance.
  ///
  FlutterCompositor GetFlutterCompositor();

  //----------------------------------------------------------------------------
  /// @brief Allocates or acquires a backing store for Flutter to render into.
  ///
  bool CreateBackingStore(const FlutterBackingStoreConfig* config,
                          FlutterBackingStore* backing_store_out);

  //----------------------------------------------------------------------------
  /// @brief Releases or recycles a backing store allocated by
  ///        CreateBackingStore.
  ///
  bool CollectBackingStore(const FlutterBackingStore* backing_store);

  //----------------------------------------------------------------------------
  /// @brief Presents the layers composited by the engine for the given view.
  ///
  bool Present(FlutterViewId view_id,
               const FlutterLayer** layers,
               size_t layers_count);

  //----------------------------------------------------------------------------
  /// @brief Handles FlutterPresentViewCallback from FlutterCompositor.
  ///
  bool PresentView(const FlutterPresentViewInfo* present_info);

  //----------------------------------------------------------------------------
  /// @brief Sets the on-screen AndroidSurface used for rendering.
  ///
  void SetAndroidSurface(std::unique_ptr<AndroidSurface> surface);

  //----------------------------------------------------------------------------
  /// @brief Returns the on-screen AndroidSurface.
  ///
  AndroidSurface* GetAndroidSurface() const;

  //----------------------------------------------------------------------------
  /// @brief Sets the AndroidSurfaceFactory used for creating overlay surfaces.
  ///
  void SetSurfaceFactory(
      std::shared_ptr<AndroidSurfaceFactory> surface_factory);

  //----------------------------------------------------------------------------
  /// @brief Returns the AndroidSurfaceFactory.
  ///
  std::shared_ptr<AndroidSurfaceFactory> GetSurfaceFactory() const;

  //----------------------------------------------------------------------------
  /// @brief Associates a native ANativeWindow with the underlying
  /// AndroidSurface.
  ///
  bool SetNativeWindow(fml::RefPtr<AndroidNativeWindow> window);

  //----------------------------------------------------------------------------
  /// @brief Resizes the on-screen surface and surface pool layers.
  ///
  bool OnScreenSurfaceResize(const DlISize& size);

  //----------------------------------------------------------------------------
  /// @brief Destroys all surfaces and teardown GPU context.
  ///
  void Teardown();

  //----------------------------------------------------------------------------
  /// @brief Destroys overlay layers in the surface pool.
  ///
  void DestroySurfaces();

  //----------------------------------------------------------------------------
  /// @brief Shows the overlay layer if not already shown.
  ///
  void ShowOverlayLayerIfNeeded();

  //----------------------------------------------------------------------------
  /// @brief Hides the overlay layer if currently shown.
  ///
  void HideOverlayLayerIfNeeded();

  //----------------------------------------------------------------------------
  /// @brief Whether the overlay layer is currently shown.
  ///
  bool IsOverlayLayerShown() const;

  //----------------------------------------------------------------------------
  /// @brief Whether SurfaceControl / HCPP platform view strategy is enabled.
  ///
  bool IsSurfaceControlEnabled() const;

  //----------------------------------------------------------------------------
  /// @brief Sets whether SurfaceControl / HCPP is enabled for testing.
  ///
  void SetSurfaceControlEnabledForTesting(std::optional<bool> enabled);

  //----------------------------------------------------------------------------
  /// @brief Returns the SurfacePool managing overlay surfaces.
  ///
  SurfacePool* GetSurfacePool() const;

  //----------------------------------------------------------------------------
  /// @brief Returns the AndroidContext.
  ///
  const std::shared_ptr<AndroidContext>& GetAndroidContext() const;

  //----------------------------------------------------------------------------
  /// @brief Returns the PlatformViewAndroidJNI facade.
  ///
  const std::shared_ptr<PlatformViewAndroidJNI>& GetJniFacade() const;

  //----------------------------------------------------------------------------
  /// @brief Returns the TaskRunners.
  ///
  const TaskRunners& GetTaskRunners() const;

  //----------------------------------------------------------------------------
  /// @brief Sets device pixel ratio for layout conversions.
  ///
  void SetDevicePixelRatio(double device_pixel_ratio);

  //----------------------------------------------------------------------------
  /// @brief Returns current device pixel ratio.
  ///
  double GetDevicePixelRatio() const;

  //----------------------------------------------------------------------------
  /// @brief Converts a FlutterPlatformView's mutations into a
  /// AndroidMutatorsStack.
  ///
  static AndroidMutatorsStack ToAndroidMutatorsStack(
      const FlutterPlatformView* platform_view);

  //----------------------------------------------------------------------------
  /// @brief Converts a FlutterTransformation to a DlMatrix.
  ///
  static DlMatrix ToDlMatrix(const FlutterTransformation& transformation);

  //----------------------------------------------------------------------------
  /// @brief Converts a FlutterRoundedRect to a DlRoundRect.
  ///
  static DlRoundRect ToDlRoundRect(const FlutterRoundedRect& rrect);

  //----------------------------------------------------------------------------
  /// @brief Converts a FlutterRect to a DlRect.
  ///
  static DlRect ToDlRect(const FlutterRect& rect);

 private:
  static bool OnCreateBackingStore(const FlutterBackingStoreConfig* config,
                                   FlutterBackingStore* backing_store_out,
                                   void* user_data);

  static bool OnCollectBackingStore(const FlutterBackingStore* backing_store,
                                    void* user_data);

  static bool OnPresentView(const FlutterPresentViewInfo* present_info);

  std::shared_ptr<AndroidContext> android_context_;
  std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  std::shared_ptr<AndroidSurfaceFactory> surface_factory_;
  const TaskRunners task_runners_;

  std::unique_ptr<AndroidSurface> android_surface_;
  std::unique_ptr<SurfacePool> surface_pool_;

  std::atomic_bool overlay_layer_is_shown_{false};
  absl::flat_hash_set<int64_t> views_visible_last_frame_;

  DlISize frame_size_{0, 0};
  double device_pixel_ratio_ = 1.0;

  bool legacy_overlay_created_ = false;
  std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>
      legacy_overlay_metadata_;
  std::unique_ptr<AndroidSurface> legacy_overlay_surface_;

  std::optional<bool> surface_control_enabled_for_testing_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidCompositor);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_
