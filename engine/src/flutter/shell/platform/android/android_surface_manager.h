// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_MANAGER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_MANAGER_H_

#include <memory>
#include <mutex>
#include <vector>

#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/fml/macros.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

//------------------------------------------------------------------------------
/// @brief Manages the allocation, pooling, lifecycle, and recycling of backing
///        stores (both OpenGL and Vulkan) for Flutter Android surfaces.
///
class AndroidSurfaceManager {
 public:
  /// @brief Describes an allocated backing store managed within the surface pool.
  struct AllocationRecord {
    FlutterBackingStoreType type = kFlutterBackingStoreTypeOpenGL;
    DlISize size = {0, 0};
    bool in_use = false;
    uint32_t id = 0;
    void* user_data = nullptr;
    std::unique_ptr<FlutterVulkanImage> vulkan_image;
    std::vector<uint8_t> software_buffer;
  };

  explicit AndroidSurfaceManager(
      const std::shared_ptr<AndroidContext>& android_context);

  virtual ~AndroidSurfaceManager();

  //----------------------------------------------------------------------------
  /// @brief Creates a backing store using the specified configuration.
  ///        Reuses an existing pooled allocation if a matching size and type
  ///        is available, or creates a new one.
  ///
  virtual bool CreateBackingStore(const FlutterBackingStoreConfig* config,
                                  FlutterBackingStore* backing_store_out);

  //----------------------------------------------------------------------------
  /// @brief Collects a backing store released by the Flutter engine and marks
  ///        it available for recycling in the pool.
  ///
  virtual bool CollectBackingStore(const FlutterBackingStore* store);

  //----------------------------------------------------------------------------
  /// @brief Clears and frees all cached backing store allocations in the pool.
  ///
  virtual void ClearBackingStores();

  //----------------------------------------------------------------------------
  /// @brief Trims unused allocations from the pool to release GPU memory.
  ///
  virtual void TrimBackingStores();

  //----------------------------------------------------------------------------
  /// @brief Sets the native window for on-screen surface presentation.
  ///
  virtual bool SetNativeWindow(fml::RefPtr<AndroidNativeWindow> window);

  //----------------------------------------------------------------------------
  /// @brief Called when the on-screen surface is resized.
  ///
  virtual bool OnScreenSurfaceResize(const DlISize& size);

  //----------------------------------------------------------------------------
  /// @brief Destroys all on-screen and off-screen resources.
  ///
  virtual void Teardown();

  //----------------------------------------------------------------------------
  /// @brief Accessor for the associated AndroidContext.
  ///
  const std::shared_ptr<AndroidContext>& GetAndroidContext() const {
    return android_context_;
  }

  //----------------------------------------------------------------------------
  /// @brief Returns the total number of allocated backing stores in the pool.
  ///
  size_t GetPoolSize() const;

  //----------------------------------------------------------------------------
  /// @brief Returns the number of currently active (in-use) backing stores.
  ///
  size_t GetInUseCount() const;

 protected:
  virtual bool CreateOpenGLBackingStore(
      const FlutterBackingStoreConfig* config,
      FlutterBackingStore* backing_store_out);

  virtual bool CreateVulkanBackingStore(
      const FlutterBackingStoreConfig* config,
      FlutterBackingStore* backing_store_out);

  virtual bool CreateSoftwareBackingStore(
      const FlutterBackingStoreConfig* config,
      FlutterBackingStore* backing_store_out);

 private:
  std::shared_ptr<AndroidContext> android_context_;
  fml::RefPtr<AndroidNativeWindow> native_window_;
  DlISize current_surface_size_ = {0, 0};

  mutable std::mutex mutex_;
  std::vector<std::shared_ptr<AllocationRecord>> pool_;
  uint32_t next_allocation_id_ = 1;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidSurfaceManager);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_MANAGER_H_
