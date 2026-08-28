// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_MANAGER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_MANAGER_H_

#include <EGL/egl.h>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/fml/memory/ref_counted.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

/// Internal representation of a pooled backing store managed by
/// `AndroidSurfaceManager`.
struct AndroidBackingStoreEntry {
  uint64_t id = 0;
  FlutterBackingStoreType type = kFlutterBackingStoreTypeSoftware;
  FlutterSize size = {0, 0};
  std::vector<uint8_t> software_buffer;
  uint32_t gl_texture_id = 0;
  uint32_t gl_framebuffer_id = 0;
  FlutterVulkanImage vulkan_image = {};
  void* user_data = nullptr;
  bool is_in_use = false;
};

/// Manages surface allocation, dedicated rendering context isolation, and
/// size-matched backing store caching for the Flutter Android Embedder API
/// backend.
class AndroidSurfaceManager {
 public:
  static constexpr size_t kDefaultMaxCachedBackingStores = 4;

  explicit AndroidSurfaceManager(
      AndroidRenderingAPI rendering_api = AndroidRenderingAPI::kSoftware,
      size_t max_cached_backing_stores = kDefaultMaxCachedBackingStores);

  ~AndroidSurfaceManager();

  /// Sets the active native window. Thread-safe.
  void SetNativeWindow(fml::RefPtr<AndroidNativeWindow> window);

  /// Clears the active native window and evicts idle cached backing stores.
  void ClearNativeWindow();

  /// Returns the active native window (or nullptr if none). Thread-safe.
  fml::RefPtr<AndroidNativeWindow> GetNativeWindow() const;

  /// Returns true if a valid native window is currently attached.
  bool HasNativeWindow() const;

  /// Returns the dimensions of the active native window (or {0, 0} if none).
  FlutterSize GetSurfaceSize() const;

  /// Returns the configured rendering backend.
  AndroidRenderingAPI GetRenderingAPI() const;

  /// Returns true if the Embedder API feature flag is enabled.
  bool IsEmbedderAPIEnabled() const;

  /// Makes the primary on-screen rendering context current.
  /// Gracefully returns false if no native window is set.
  bool MakeCurrent();

  /// Clears the primary on-screen rendering context.
  bool ClearCurrent();

  /// Makes the isolated resource/IO context current for background
  /// asset/texture loading. Guarantees context isolation to eliminate
  /// cross-thread EGL_BAD_ACCESS collisions.
  bool MakeResourceCurrent();

  /// Clears the isolated resource/IO context.
  bool ClearResourceCurrent();

  /// Swaps display buffers for the active surface.
  bool SwapBuffers();

  /// Creates (or recycles from pool) a backing store matching `config`.
  bool CreateBackingStore(const FlutterBackingStoreConfig& config,
                          FlutterBackingStore* backing_store_out);

  /// Collects a previously created backing store, returning it to the cache
  /// pool if capacity allows or destroying it.
  bool CollectBackingStore(const FlutterBackingStore* backing_store);

  /// Clears and destroys idle cached backing store allocations in the pool.
  void ClearBackingStoreCache();

  /// Returns the number of currently cached (idle) backing stores in the pool.
  size_t GetCachedBackingStoreCount() const;

  /// Returns the total number of allocated backing stores (in-use + cached).
  size_t GetAllocatedBackingStoreCount() const;

 private:
  const AndroidRenderingAPI rendering_api_;
  const size_t max_cached_backing_stores_;

  mutable std::mutex window_mutex_;
  fml::RefPtr<AndroidNativeWindow> native_window_;

  EGLDisplay egl_display_ = EGL_NO_DISPLAY;
  EGLContext egl_context_ = EGL_NO_CONTEXT;
  EGLContext egl_resource_context_ = EGL_NO_CONTEXT;
  EGLConfig egl_config_ = nullptr;
  EGLSurface egl_pbuffer_surface_ = EGL_NO_SURFACE;
  EGLSurface egl_window_surface_ = EGL_NO_SURFACE;

  void InitializeEGL();
  void DestroyEGL();

  mutable std::mutex cache_mutex_;
  uint64_t next_backing_store_id_ = 1;
  std::unordered_map<uint64_t, std::unique_ptr<AndroidBackingStoreEntry>>
      allocated_backing_stores_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidSurfaceManager);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_MANAGER_H_
