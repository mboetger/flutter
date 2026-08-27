// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_manager.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "flutter/fml/logging.h"

namespace flutter {

AndroidSurfaceManager::AndroidSurfaceManager(AndroidRenderingAPI rendering_api,
                                             size_t max_cached_backing_stores)
    : rendering_api_(rendering_api),
      max_cached_backing_stores_(max_cached_backing_stores) {}

AndroidSurfaceManager::~AndroidSurfaceManager() {
  ClearBackingStoreCache();
}

void AndroidSurfaceManager::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> window) {
  std::lock_guard<std::mutex> lock(window_mutex_);
  native_window_ = std::move(window);
}

void AndroidSurfaceManager::ClearNativeWindow() {
  {
    std::lock_guard<std::mutex> lock(window_mutex_);
    native_window_ = nullptr;
  }
  ClearBackingStoreCache();
}

fml::RefPtr<AndroidNativeWindow> AndroidSurfaceManager::GetNativeWindow()
    const {
  std::lock_guard<std::mutex> lock(window_mutex_);
  return native_window_;
}

bool AndroidSurfaceManager::HasNativeWindow() const {
  std::lock_guard<std::mutex> lock(window_mutex_);
  return native_window_ && native_window_->IsValid();
}

FlutterSize AndroidSurfaceManager::GetSurfaceSize() const {
  std::lock_guard<std::mutex> lock(window_mutex_);
  if (native_window_ && native_window_->IsValid()) {
    auto size = native_window_->GetSize();
    return FlutterSize{static_cast<double>(size.width),
                       static_cast<double>(size.height)};
  }
  return FlutterSize{0, 0};
}

AndroidRenderingAPI AndroidSurfaceManager::GetRenderingAPI() const {
  return rendering_api_;
}

bool AndroidSurfaceManager::IsEmbedderAPIEnabled() const {
  return FlutterMain::IsEmbedderAPIEnabled();
}

bool AndroidSurfaceManager::MakeCurrent() {
  if (!HasNativeWindow()) {
    return false;
  }
  return true;
}

bool AndroidSurfaceManager::ClearCurrent() {
  return true;
}

bool AndroidSurfaceManager::MakeResourceCurrent() {
  return true;
}

bool AndroidSurfaceManager::ClearResourceCurrent() {
  return true;
}

bool AndroidSurfaceManager::SwapBuffers() {
  if (!HasNativeWindow()) {
    return false;
  }
  return true;
}

bool AndroidSurfaceManager::CreateBackingStore(
    const FlutterBackingStoreConfig& config,
    FlutterBackingStore* backing_store_out) {
  if (backing_store_out == nullptr) {
    return false;
  }
  if (config.struct_size != sizeof(FlutterBackingStoreConfig)) {
    return false;
  }
  if (!std::isfinite(config.size.width) || !std::isfinite(config.size.height) ||
      config.size.width <= 0.0 || config.size.height <= 0.0 ||
      config.size.width > 65536.0 || config.size.height > 65536.0) {
    return false;
  }

  FlutterBackingStoreType expected_type;
  switch (rendering_api_) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      expected_type = kFlutterBackingStoreTypeSoftware;
      break;
    case AndroidRenderingAPI::kSkiaOpenGLES:
      expected_type = kFlutterBackingStoreTypeOpenGL;
      break;
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
    case AndroidRenderingAPI::kImpellerAutoselect:
      expected_type = kFlutterBackingStoreTypeOpenGL;
      break;
    case AndroidRenderingAPI::kImpellerVulkan:
      expected_type = kFlutterBackingStoreTypeVulkan;
      break;
  }

  std::lock_guard<std::mutex> lock(cache_mutex_);

  // Check if a size-matched idle backing store is available in the pool.
  for (auto& [id, entry] : allocated_backing_stores_) {
    if (!entry->is_in_use && entry->type == expected_type &&
        entry->size.width == config.size.width &&
        entry->size.height == config.size.height) {
      entry->is_in_use = true;

      backing_store_out->struct_size = sizeof(FlutterBackingStore);
      backing_store_out->user_data = entry.get();
      backing_store_out->type = entry->type;
      backing_store_out->did_update = true;

      if (entry->type == kFlutterBackingStoreTypeSoftware) {
        backing_store_out->software.allocation = entry->software_buffer.data();
        backing_store_out->software.row_bytes =
            static_cast<size_t>(config.size.width) * 4;
        backing_store_out->software.height =
            static_cast<size_t>(config.size.height);
        backing_store_out->software.user_data = entry.get();
        backing_store_out->software.destruction_callback = [](void*) {};
      } else if (entry->type == kFlutterBackingStoreTypeOpenGL) {
        backing_store_out->open_gl.type = kFlutterOpenGLTargetTypeFramebuffer;
        backing_store_out->open_gl.framebuffer.name = entry->gl_framebuffer_id;
        backing_store_out->open_gl.framebuffer.target = 0x8058;  // GL_RGBA8
        backing_store_out->open_gl.framebuffer.user_data = entry.get();
        backing_store_out->open_gl.framebuffer.destruction_callback =
            [](void*) {};
      } else if (entry->type == kFlutterBackingStoreTypeVulkan) {
        backing_store_out->vulkan.struct_size =
            sizeof(FlutterVulkanBackingStore);
        backing_store_out->vulkan.image = &entry->vulkan_image;
        backing_store_out->vulkan.user_data = entry.get();
        backing_store_out->vulkan.destruction_callback = [](void*) {};
      }
      return true;
    }
  }

  // Allocate a new backing store entry.
  auto new_entry = std::make_unique<AndroidBackingStoreEntry>();
  new_entry->id = next_backing_store_id_++;
  new_entry->type = expected_type;
  new_entry->size = config.size;
  new_entry->is_in_use = true;

  if (expected_type == kFlutterBackingStoreTypeSoftware) {
    size_t byte_size = static_cast<size_t>(config.size.width) *
                       static_cast<size_t>(config.size.height) * 4;
    new_entry->software_buffer.resize(byte_size, 0);
  } else if (expected_type == kFlutterBackingStoreTypeOpenGL) {
    new_entry->gl_framebuffer_id = static_cast<uint32_t>(new_entry->id);
    new_entry->gl_texture_id = static_cast<uint32_t>(new_entry->id);
  } else if (expected_type == kFlutterBackingStoreTypeVulkan) {
    new_entry->vulkan_image.struct_size = sizeof(FlutterVulkanImage);
    new_entry->vulkan_image.image =
        reinterpret_cast<FlutterVulkanImageHandle>(new_entry->id);
    new_entry->vulkan_image.format = 0x8058;  // VK_FORMAT_R8G8B8A8_UNORM / RGBA
    new_entry->vulkan_image.width = static_cast<size_t>(config.size.width);
    new_entry->vulkan_image.height = static_cast<size_t>(config.size.height);
  }

  AndroidBackingStoreEntry* raw_entry = new_entry.get();
  allocated_backing_stores_[new_entry->id] = std::move(new_entry);

  backing_store_out->struct_size = sizeof(FlutterBackingStore);
  backing_store_out->user_data = raw_entry;
  backing_store_out->type = raw_entry->type;
  backing_store_out->did_update = true;

  if (raw_entry->type == kFlutterBackingStoreTypeSoftware) {
    backing_store_out->software.allocation = raw_entry->software_buffer.data();
    backing_store_out->software.row_bytes =
        static_cast<size_t>(config.size.width) * 4;
    backing_store_out->software.height =
        static_cast<size_t>(config.size.height);
    backing_store_out->software.user_data = raw_entry;
    backing_store_out->software.destruction_callback = [](void*) {};
  } else if (raw_entry->type == kFlutterBackingStoreTypeOpenGL) {
    backing_store_out->open_gl.type = kFlutterOpenGLTargetTypeFramebuffer;
    backing_store_out->open_gl.framebuffer.name = raw_entry->gl_framebuffer_id;
    backing_store_out->open_gl.framebuffer.target = 0x8058;  // GL_RGBA8
    backing_store_out->open_gl.framebuffer.user_data = raw_entry;
    backing_store_out->open_gl.framebuffer.destruction_callback = [](void*) {};
  } else if (raw_entry->type == kFlutterBackingStoreTypeVulkan) {
    backing_store_out->vulkan.struct_size = sizeof(FlutterVulkanBackingStore);
    backing_store_out->vulkan.image = &raw_entry->vulkan_image;
    backing_store_out->vulkan.user_data = raw_entry;
    backing_store_out->vulkan.destruction_callback = [](void*) {};
  }

  return true;
}

bool AndroidSurfaceManager::CollectBackingStore(
    const FlutterBackingStore* backing_store) {
  if (backing_store == nullptr) {
    return false;
  }
  if (backing_store->struct_size != sizeof(FlutterBackingStore)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(cache_mutex_);

  auto* entry_ptr =
      static_cast<AndroidBackingStoreEntry*>(backing_store->user_data);
  if (entry_ptr == nullptr) {
    return false;
  }

  auto it = allocated_backing_stores_.find(entry_ptr->id);
  if (it == allocated_backing_stores_.end()) {
    return false;
  }

  if (!HasNativeWindow()) {
    allocated_backing_stores_.erase(it);
    return true;
  }

  size_t idle_count = 0;
  for (const auto& [id, e] : allocated_backing_stores_) {
    if (!e->is_in_use) {
      idle_count++;
    }
  }

  if (idle_count >= max_cached_backing_stores_) {
    // Evict a size-mismatched idle entry first to prevent cache poisoning on
    // resize.
    auto evict_it = allocated_backing_stores_.end();
    for (auto candidate_it = allocated_backing_stores_.begin();
         candidate_it != allocated_backing_stores_.end(); ++candidate_it) {
      if (!candidate_it->second->is_in_use &&
          candidate_it->first != it->first) {
        if (candidate_it->second->size.width != it->second->size.width ||
            candidate_it->second->size.height != it->second->size.height) {
          evict_it = candidate_it;
          break;
        } else if (evict_it == allocated_backing_stores_.end()) {
          evict_it = candidate_it;
        }
      }
    }

    if (evict_it != allocated_backing_stores_.end()) {
      allocated_backing_stores_.erase(evict_it);
    }
  }

  it->second->is_in_use = false;
  return true;
}

void AndroidSurfaceManager::ClearBackingStoreCache() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  for (auto it = allocated_backing_stores_.begin();
       it != allocated_backing_stores_.end();) {
    if (!it->second->is_in_use) {
      it = allocated_backing_stores_.erase(it);
    } else {
      ++it;
    }
  }
}

size_t AndroidSurfaceManager::GetCachedBackingStoreCount() const {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  size_t count = 0;
  for (const auto& [id, entry] : allocated_backing_stores_) {
    if (!entry->is_in_use) {
      count++;
    }
  }
  return count;
}

size_t AndroidSurfaceManager::GetAllocatedBackingStoreCount() const {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  return allocated_backing_stores_.size();
}

}  // namespace flutter
