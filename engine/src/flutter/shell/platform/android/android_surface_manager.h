// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_MANAGER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_MANAGER_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/fml/memory/ref_ptr.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

/// Manages backing store allocation pools (Software, OpenGL, Vulkan) for the
/// Android Embedder API layer compositor.
///
/// Features:
/// - Size-matched backing store caching to eliminate buffer churn during frame
///   rendering.
/// - Thread safety across platform and raster threads via internal mutex
/// synchronization.
/// - Resource cleanup on window destruction, memory pressure, or engine
/// teardown.
class AndroidSurfaceManager {
 public:
  /// Retains front and back buffers for double buffering by default.
  static constexpr size_t kDefaultMaxCachedPerSize = 2;

  /// Internal structure describing the allocated backing store resources.
  struct BackingStoreData {
    FlutterBackingStoreType type = kFlutterBackingStoreTypeOpenGL;
    size_t width = 0;
    size_t height = 0;

    // Software backing store buffer (RGBA8888, 4 bytes per pixel).
    std::vector<uint8_t> software_buffer;

    // OpenGL handles and metadata.
    uint32_t gl_framebuffer_id = 0;
    uint32_t gl_texture_id = 0;
    uint32_t gl_depth_stencil_id = 0;
    uint32_t gl_color_renderbuffer_id = 0;

    // Vulkan image description if applicable.
    FlutterVulkanImage vulkan_image = {};
  };

  explicit AndroidSurfaceManager(AndroidRenderingAPI rendering_api);
  ~AndroidSurfaceManager();

  /// Returns the configured rendering backend.
  AndroidRenderingAPI GetRenderingAPI() const;

  /// Sets the active native window for presentation.
  void SetNativeWindow(fml::RefPtr<AndroidNativeWindow> native_window);

  /// Clears the active native window.
  void ClearNativeWindow();

  /// Returns whether a valid native window is currently attached.
  bool HasNativeWindow() const;

  /// Returns the currently attached native window, or nullptr if none attached.
  fml::RefPtr<AndroidNativeWindow> GetNativeWindow() const;

  /// Creates or recycles a size-matched backing store for the specified
  /// configuration.
  ///
  /// Returns true on success and populates `backing_store_out`. Returns false
  /// on failure or invalid arguments.
  bool CreateBackingStore(const FlutterBackingStoreConfig* config,
                          FlutterBackingStore* backing_store_out);

  /// Collects and recycles a backing store back into the pool for future reuse.
  ///
  /// Returns true on success, false if the backing store pointer or metadata is
  /// invalid.
  bool CollectBackingStore(const FlutterBackingStore* backing_store);

  /// Clears all cached backing stores in the pool, releasing associated GPU/RAM
  /// memory.
  void ClearBackingStorePool();

  /// Trims excess backing stores from the pool so each size key retains at most
  /// `max_cached_per_size` entries.
  void TrimBackingStorePool(
      size_t max_cached_per_size = kDefaultMaxCachedPerSize);

  /// Returns the total number of cached backing stores currently retained in
  /// the pool.
  size_t GetCachedBackingStoreCount() const;

 private:
  // Key for size-matched caching: combined (width << 32) | height.
  static uint64_t MakeSizeKey(size_t width, size_t height) {
    // 32-bit shift packs width and height into a 64-bit lookup key.
    constexpr uint64_t kWidthShift = 32;
    constexpr uint64_t kHeightMask = 0xFFFFFFFFULL;
    return (static_cast<uint64_t>(width) << kWidthShift) |
           (static_cast<uint64_t>(height) & kHeightMask);
  }

  const AndroidRenderingAPI rendering_api_;
  mutable std::mutex mutex_;
  fml::RefPtr<AndroidNativeWindow> native_window_;

  // Pool of recycled backing stores keyed by dimension.
  std::unordered_map<uint64_t, std::deque<std::unique_ptr<BackingStoreData>>>
      pool_;

  // Set of actively in-flight backing stores given to the embedder engine.
  std::unordered_set<BackingStoreData*> in_flight_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidSurfaceManager);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_MANAGER_H_
