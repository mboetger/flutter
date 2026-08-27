// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_manager.h"

#include <algorithm>

namespace flutter {

// Sized internal format for OpenGL RGBA8 framebuffer/texture attachment
// (GL_RGBA8 / GL_RGBA8_OES = 0x8058).
static constexpr uint32_t kOpenGLSizedFormatRGBA8 = 0x8058;

// Vulkan format VK_FORMAT_R8G8B8A8_UNORM = 44 standard 32-bit color format.
static constexpr uint32_t kVulkanFormatR8G8B8A8Unorm = 44;

// Bytes per pixel for 32-bit RGBA software pixel buffer.
static constexpr size_t kBytesPerPixelRGBA = 4;

AndroidSurfaceManager::AndroidSurfaceManager(
    const std::shared_ptr<AndroidContext>& android_context)
    : android_context_(android_context) {}

AndroidSurfaceManager::~AndroidSurfaceManager() {
  ClearBackingStores();
}

bool AndroidSurfaceManager::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  if (config == nullptr || backing_store_out == nullptr) {
    return false;
  }

  if (!android_context_) {
    return false;
  }

  switch (android_context_->RenderingApi()) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      return CreateSoftwareBackingStore(config, backing_store_out);
    case AndroidRenderingAPI::kSkiaOpenGLES:
      return CreateOpenGLBackingStore(config, backing_store_out);
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
      return CreateOpenGLBackingStore(config, backing_store_out);
    case AndroidRenderingAPI::kImpellerVulkan:
      return CreateVulkanBackingStore(config, backing_store_out);
    case AndroidRenderingAPI::kImpellerAutoselect:
      // When auto-selecting on Android, default to Vulkan/OpenGL backing store.
      return CreateOpenGLBackingStore(config, backing_store_out);
  }

  return false;
}

bool AndroidSurfaceManager::CreateOpenGLBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  std::lock_guard<std::mutex> lock(mutex_);

  const DlISize requested_size(static_cast<int32_t>(config->size.width),
                               static_cast<int32_t>(config->size.height));

  // Search the pool for an existing matching backing store that is not in use.
  std::shared_ptr<AllocationRecord> record;
  for (auto& entry : pool_) {
    if (!entry->in_use && entry->type == kFlutterBackingStoreTypeOpenGL &&
        entry->size == requested_size) {
      record = entry;
      break;
    }
  }

  if (!record) {
    record = std::make_shared<AllocationRecord>();
    record->type = kFlutterBackingStoreTypeOpenGL;
    record->size = requested_size;
    record->id = next_allocation_id_++;
    pool_.push_back(record);
  }

  record->in_use = true;

  backing_store_out->struct_size = sizeof(FlutterBackingStore);
  backing_store_out->type = kFlutterBackingStoreTypeOpenGL;
  backing_store_out->user_data = record.get();
  backing_store_out->did_update = true;

  backing_store_out->open_gl.type = kFlutterOpenGLTargetTypeFramebuffer;
  backing_store_out->open_gl.framebuffer.target = kOpenGLSizedFormatRGBA8;
  // Framebuffer 0 represents the default window framebuffer in OpenGL ES on
  // Android.
  backing_store_out->open_gl.framebuffer.name = 0;
  backing_store_out->open_gl.framebuffer.user_data = record.get();
  backing_store_out->open_gl.framebuffer.destruction_callback =
      [](void* user_data) {
        // Backing store destruction is managed by AndroidSurfaceManager pool.
      };

  return true;
}

bool AndroidSurfaceManager::CreateVulkanBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  std::lock_guard<std::mutex> lock(mutex_);

  const DlISize requested_size(static_cast<int32_t>(config->size.width),
                               static_cast<int32_t>(config->size.height));

  std::shared_ptr<AllocationRecord> record;
  for (auto& entry : pool_) {
    if (!entry->in_use && entry->type == kFlutterBackingStoreTypeVulkan &&
        entry->size == requested_size) {
      record = entry;
      break;
    }
  }

  if (!record) {
    record = std::make_shared<AllocationRecord>();
    record->type = kFlutterBackingStoreTypeVulkan;
    record->size = requested_size;
    record->id = next_allocation_id_++;
    record->vulkan_image = std::make_unique<FlutterVulkanImage>();
    record->vulkan_image->struct_size = sizeof(FlutterVulkanImage);
    record->vulkan_image->image = reinterpret_cast<FlutterVulkanImageHandle>(
        static_cast<uintptr_t>(record->id));
    record->vulkan_image->format = kVulkanFormatR8G8B8A8Unorm;
    pool_.push_back(record);
  }

  record->in_use = true;

  backing_store_out->struct_size = sizeof(FlutterBackingStore);
  backing_store_out->type = kFlutterBackingStoreTypeVulkan;
  backing_store_out->user_data = record.get();
  backing_store_out->did_update = true;

  backing_store_out->vulkan.struct_size = sizeof(FlutterVulkanBackingStore);
  backing_store_out->vulkan.image = record->vulkan_image.get();
  backing_store_out->vulkan.user_data = record.get();
  backing_store_out->vulkan.destruction_callback = [](void* user_data) {
    // Backing store destruction is managed by AndroidSurfaceManager pool.
  };

  return true;
}

bool AndroidSurfaceManager::CreateSoftwareBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  std::lock_guard<std::mutex> lock(mutex_);

  const DlISize requested_size(static_cast<int32_t>(config->size.width),
                               static_cast<int32_t>(config->size.height));

  std::shared_ptr<AllocationRecord> record;
  for (auto& entry : pool_) {
    if (!entry->in_use && entry->type == kFlutterBackingStoreTypeSoftware2 &&
        entry->size == requested_size) {
      record = entry;
      break;
    }
  }

  if (!record) {
    record = std::make_shared<AllocationRecord>();
    record->type = kFlutterBackingStoreTypeSoftware2;
    record->size = requested_size;
    record->id = next_allocation_id_++;
    const size_t row_bytes =
        static_cast<size_t>(config->size.width) * kBytesPerPixelRGBA;
    record->software_buffer.resize(row_bytes *
                                   static_cast<size_t>(config->size.height));
    pool_.push_back(record);
  }

  record->in_use = true;

  backing_store_out->struct_size = sizeof(FlutterBackingStore);
  backing_store_out->type = kFlutterBackingStoreTypeSoftware2;
  backing_store_out->user_data = record.get();
  backing_store_out->did_update = true;

  backing_store_out->software2.struct_size =
      sizeof(FlutterSoftwareBackingStore2);
  backing_store_out->software2.allocation = record->software_buffer.data();
  backing_store_out->software2.row_bytes =
      static_cast<size_t>(config->size.width) * kBytesPerPixelRGBA;
  backing_store_out->software2.height =
      static_cast<size_t>(config->size.height);
  backing_store_out->software2.pixel_format =
      kFlutterSoftwarePixelFormatRGBA8888;
  backing_store_out->software2.user_data = record.get();
  backing_store_out->software2.destruction_callback = [](void* user_data) {
    // Backing store destruction is managed by AndroidSurfaceManager pool.
  };

  return true;
}

bool AndroidSurfaceManager::CollectBackingStore(
    const FlutterBackingStore* store) {
  if (store == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& entry : pool_) {
    if (entry.get() == store->user_data) {
      entry->in_use = false;
      return true;
    }
  }

  return false;
}

void AndroidSurfaceManager::ClearBackingStores() {
  std::lock_guard<std::mutex> lock(mutex_);
  pool_.clear();
}

void AndroidSurfaceManager::TrimBackingStores() {
  std::lock_guard<std::mutex> lock(mutex_);
  pool_.erase(
      std::remove_if(pool_.begin(), pool_.end(),
                     [](const std::shared_ptr<AllocationRecord>& record) {
                       return !record->in_use;
                     }),
      pool_.end());
}

bool AndroidSurfaceManager::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> window) {
  native_window_ = window;
  return true;
}

bool AndroidSurfaceManager::OnScreenSurfaceResize(const DlISize& size) {
  current_surface_size_ = size;
  return true;
}

void AndroidSurfaceManager::Teardown() {
  ClearBackingStores();
  native_window_ = nullptr;
  current_surface_size_ = {0, 0};
}

size_t AndroidSurfaceManager::GetPoolSize() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return pool_.size();
}

size_t AndroidSurfaceManager::GetInUseCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  size_t in_use = 0;
  for (const auto& entry : pool_) {
    if (entry->in_use) {
      in_use++;
    }
  }
  return in_use;
}

}  // namespace flutter
