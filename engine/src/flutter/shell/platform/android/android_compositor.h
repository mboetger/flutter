// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/fml/memory/ref_ptr.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/task_runner.h"
#include "flutter/shell/platform/android/android_surface_manager.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

/// Implements `FlutterCompositor` layer composition and frame presentation for
/// the Android Embedder API.
///
/// Features:
/// - Handles backing store allocation and collection via
/// `AndroidSurfaceManager`.
/// - Supports view presentation (`FlutterPresentViewCallback`).
/// - Implements a synchronous surface detachment barrier (`OnSurfaceDestroyed`)
///   using `fml::AutoResetWaitableEvent` to guarantee that all active raster
///   operations on the `ANativeWindow` complete before the OS destroys the
///   window.
class AndroidCompositor {
 public:
  AndroidCompositor(
      std::shared_ptr<AndroidSurfaceManager> surface_manager,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade = nullptr,
      fml::RefPtr<fml::TaskRunner> platform_task_runner = nullptr,
      fml::RefPtr<fml::TaskRunner> raster_task_runner = nullptr);

  ~AndroidCompositor();

  /// Returns a configured `FlutterCompositor` struct populated with callbacks
  /// that dispatch to this instance.
  FlutterCompositor GetFlutterCompositor();

  /// Creates a backing store for the specified layer configuration.
  bool CreateBackingStore(const FlutterBackingStoreConfig* config,
                          FlutterBackingStore* backing_store_out);

  /// Collects and recycles the given backing store.
  bool CollectBackingStore(const FlutterBackingStore* backing_store);

  /// Presents the composited layers for the target view.
  bool PresentView(const FlutterPresentViewInfo* info);

  /// Presents the composited layers for the implicit or specified view.
  bool Present(FlutterViewId view_id,
               const FlutterLayer** layers,
               size_t layers_count);

  /// Invoked when a new native window surface is created.
  void OnSurfaceCreated(fml::RefPtr<AndroidNativeWindow> native_window);

  /// Invoked when the native window surface is about to be destroyed by the OS.
  ///
  /// Executes a synchronous detach barrier blocking on the raster thread until
  /// all active rendering operations on the `ANativeWindow` have completed and
  /// references to the native window are released.
  void OnSurfaceDestroyed();

  /// Invoked when the native window surface is replaced or recreated.
  void OnSurfaceWindowChanged(fml::RefPtr<AndroidNativeWindow> native_window);

  /// Returns whether the surface is currently marked as destroyed.
  bool IsSurfaceDestroyed() const;

  /// Returns the underlying surface manager.
  std::shared_ptr<AndroidSurfaceManager> GetSurfaceManager() const;

  /// Converts `FlutterPlatformView` mutations array to a `MutatorsStack` for
  /// JNI platform view presentation.
  static MutatorsStack ConvertMutationsToMutatorsStack(
      const FlutterPlatformView* platform_view);

 private:
  // Static callback thunks matching FlutterCompositor function pointer
  // signatures.
  static bool OnCreateBackingStore(const FlutterBackingStoreConfig* config,
                                   FlutterBackingStore* backing_store_out,
                                   void* user_data);

  static bool OnCollectBackingStore(const FlutterBackingStore* backing_store,
                                    void* user_data);

  static bool OnPresentView(const FlutterPresentViewInfo* info);

  const std::shared_ptr<AndroidSurfaceManager> surface_manager_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  const fml::RefPtr<fml::TaskRunner> platform_task_runner_;
  const fml::RefPtr<fml::TaskRunner> raster_task_runner_;

  mutable std::mutex present_mutex_;
  std::atomic<bool> surface_destroyed_{true};
  std::atomic<size_t> presented_frame_count_{0};

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidCompositor);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_H_
