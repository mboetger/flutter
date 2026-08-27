// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/fml/memory/ref_counted.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/task_runner.h"
#include "flutter/shell/platform/android/android_surface_manager.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

/// Types of mutators applied to platform views.
enum class AndroidMutatorType {
  kTransform,
  kClipRect,
  kClipRRect,
  kClipRSE,
  kOpacity,
  kClipPath,
};

/// 2D path segment data for platform view clip path mutations.
struct AndroidPathSegment {
  FlutterPathVerb verb = kFlutterPathVerbMove;
  FlutterPoint points[3] = {};
  double conic_weight = 0.0;
};

/// Complete vector path data for clipping platform views.
struct AndroidPathData {
  FlutterPathFillType fill_type = kFlutterPathFillTypeNonZero;
  std::vector<AndroidPathSegment> segments;
};

/// Individual mutator applied to an Android platform view.
struct AndroidPlatformViewMutator {
  AndroidMutatorType type = AndroidMutatorType::kTransform;
  float opacity = 1.0f;
  FlutterRect rect = {};
  float radii[8] = {};  // [tl_w, tl_h, tr_w, tr_h, br_w, br_h, bl_w, bl_h]
  float transform_matrix[9] = {};  // 3x3 matrix in row-major order (matching android.graphics.Matrix)
  AndroidPathData path;
};

/// Container representing the ordered stack of mutations for a platform view.
class AndroidPlatformViewMutatorsStack {
 public:
  AndroidPlatformViewMutatorsStack() = default;
  ~AndroidPlatformViewMutatorsStack() = default;

  void PushTransform(const float matrix[9]);
  void PushClipRect(float left, float top, float right, float bottom);
  void PushClipRRect(float left,
                     float top,
                     float right,
                     float bottom,
                     const float radii[8]);
  void PushClipRSE(float left,
                   float top,
                   float right,
                   float bottom,
                   const float radii[8]);
  void PushOpacity(float opacity);
  void PushClipPath(const AndroidPathData& path);

  const std::vector<AndroidPlatformViewMutator>& GetMutators() const {
    return mutators_;
  }
  bool IsEmpty() const { return mutators_.empty(); }
  size_t Size() const { return mutators_.size(); }
  void Clear() { mutators_.clear(); }

 private:
  std::vector<AndroidPlatformViewMutator> mutators_;
};

/// Callback for platform view layer presentation handling.
using PlatformViewRendererCallback =
    std::function<bool(const FlutterPlatformView* platform_view,
                       const FlutterLayer& layer,
                       size_t layer_index)>;

/// Callback for platform view layer presentation with mapped mutators stack.
using PlatformViewMutatorsRendererCallback =
    std::function<bool(const FlutterPlatformView* platform_view,
                       const FlutterLayer& layer,
                       const AndroidPlatformViewMutatorsStack& mutators_stack,
                       size_t layer_index)>;

/// Manages backing store creation, collection, layer presentation, direct JNI
/// mutator mapping, and synchronous surface detachment barrier for the Android
/// Embedder API backend.
class AndroidCompositor {
 public:
  AndroidCompositor(
      std::shared_ptr<AndroidSurfaceManager> surface_manager,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade = nullptr,
      fml::RefPtr<fml::TaskRunner> raster_task_runner = nullptr,
      fml::RefPtr<fml::TaskRunner> platform_task_runner = nullptr);

  ~AndroidCompositor();

  /// Returns the `FlutterCompositor` configuration struct populated with
  /// callbacks pointing to this instance.
  FlutterCompositor GetCompositorConfig();

  /// Creates a backing store matching `config`.
  bool CreateBackingStore(const FlutterBackingStoreConfig* config,
                          FlutterBackingStore* backing_store_out);

  /// Collects a previously allocated backing store.
  bool CollectBackingStore(const FlutterBackingStore* backing_store);

  /// Presents the view described by `present_info`.
  bool PresentView(const FlutterPresentViewInfo* present_info);

  /// Presents layers for `view_id`.
  bool Present(FlutterViewId view_id,
               const FlutterLayer** layers,
               size_t layers_count);

  //----------------------------------------------------------------------------
  /// Platform View Mutator Mapping & DPR Normalization
  //----------------------------------------------------------------------------

  /// Sets the device pixel ratio for coordinate and root transform normalization.
  void SetDevicePixelRatio(double dpr);

  /// Returns the current device pixel ratio.
  double GetDevicePixelRatio() const;

  /// Populates `stack_out` with direct JNI-compatible mutators translated from
  /// `platform_view->mutations`, with optional root DPR transform normalization.
  bool PopulateMutatorsStack(
      const FlutterPlatformView* platform_view,
      AndroidPlatformViewMutatorsStack* stack_out,
      double dpr = 1.0) const;

  /// Normalizes a 3x3 root transformation matrix with device pixel ratio `dpr`.
  static void NormalizeRootTransform(const FlutterTransformation& in_transform,
                                     double dpr,
                                     float out_matrix[9]);

  //----------------------------------------------------------------------------
  /// Surface Lifecycle & Synchronous Detach Barrier
  //----------------------------------------------------------------------------

  /// Notifies the compositor that the on-screen surface has been created.
  /// Synchronizes with the raster task runner if available.
  void OnSurfaceCreated(fml::RefPtr<AndroidNativeWindow> window);

  /// Notifies the compositor that the surface window has changed.
  /// Synchronizes with the raster task runner if available.
  void OnSurfaceWindowChanged(fml::RefPtr<AndroidNativeWindow> window);

  /// Synchronous Surface Detach Barrier:
  /// Blocks the calling thread (platform thread) until the raster task runner
  /// completes any active frame rendering and clears the native window on the
  /// surface manager. Ensures no use-after-free occurs before the OS destroys
  /// the ANativeWindow.
  void OnSurfaceDestroyed();

  /// Notifies the compositor that the on-screen surface has resized.
  /// Synchronizes with the raster task runner and evicts mismatched caches.
  void OnSurfaceResized(const FlutterSize& size);

  /// Sets an optional platform view renderer callback (for testing/interception).
  void SetPlatformViewRendererCallback(PlatformViewRendererCallback callback);

  /// Sets an optional platform view mutators renderer callback.
  void SetPlatformViewMutatorsRendererCallback(
      PlatformViewMutatorsRendererCallback callback);

  /// Returns the number of successfully presented frames.
  size_t GetPresentCount() const;

  /// Returns the underlying surface manager.
  std::shared_ptr<AndroidSurfaceManager> GetSurfaceManager() const;

  /// Returns the JNI facade.
  std::shared_ptr<PlatformViewAndroidJNI> GetJNIFacade() const;

  /// Returns the raster task runner.
  fml::RefPtr<fml::TaskRunner> GetRasterTaskRunner() const;

  /// Returns the platform task runner.
  fml::RefPtr<fml::TaskRunner> GetPlatformTaskRunner() const;

  /// Returns true if the Embedder API feature flag is enabled.
  bool IsEmbedderAPIEnabled() const;

 private:
  static bool OnCreateBackingStore(const FlutterBackingStoreConfig* config,
                                  FlutterBackingStore* backing_store_out,
                                  void* user_data);

  static bool OnCollectBackingStore(const FlutterBackingStore* backing_store,
                                   void* user_data);

  static bool OnPresentView(const FlutterPresentViewInfo* present_info);

  const std::shared_ptr<AndroidSurfaceManager> surface_manager_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  const fml::RefPtr<fml::TaskRunner> raster_task_runner_;
  const fml::RefPtr<fml::TaskRunner> platform_task_runner_;

  mutable std::mutex callback_mutex_;
  PlatformViewRendererCallback platform_view_renderer_;
  PlatformViewMutatorsRendererCallback platform_view_mutators_renderer_;
  std::atomic<size_t> present_count_{0};
  std::atomic<double> device_pixel_ratio_{1.0};

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidCompositor);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_
