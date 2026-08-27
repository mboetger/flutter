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

/// Callback for platform view layer presentation handling.
using PlatformViewRendererCallback =
    std::function<bool(const FlutterPlatformView* platform_view,
                       const FlutterLayer& layer,
                       size_t layer_index)>;

/// Manages backing store creation, collection, layer presentation, and
/// synchronous surface detachment barrier for the Android Embedder API backend.
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
  std::atomic<size_t> present_count_{0};

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidCompositor);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_
