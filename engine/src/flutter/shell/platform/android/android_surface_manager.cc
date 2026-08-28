// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_manager.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "flutter/fml/logging.h"

namespace flutter {

namespace {

// Bytes per pixel in RGBA8888 32-bit color format.
constexpr size_t kBytesPerPixelRGBA8888 = 4;

// Internal color format for OpenGL framebuffer attachments (GL_RGBA8 = 0x8058).
constexpr uint32_t kGLFramebufferFormatRGBA8 = 0x8058;

}  // namespace

AndroidSurfaceManager::AndroidSurfaceManager(AndroidRenderingAPI rendering_api)
    : rendering_api_(rendering_api) {}

AndroidSurfaceManager::~AndroidSurfaceManager() {
  ClearBackingStorePool();
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto* data : in_flight_) {
    delete data;
  }
  in_flight_.clear();
}

AndroidRenderingAPI AndroidSurfaceManager::GetRenderingAPI() const {
  return rendering_api_;
}

void AndroidSurfaceManager::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  std::lock_guard<std::mutex> lock(mutex_);
  native_window_ = std::move(native_window);
}

void AndroidSurfaceManager::ClearNativeWindow() {
  std::lock_guard<std::mutex> lock(mutex_);
  native_window_ = nullptr;
}

bool AndroidSurfaceManager::HasNativeWindow() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return native_window_.get() != nullptr && native_window_->IsValid();
}

fml::RefPtr<AndroidNativeWindow> AndroidSurfaceManager::GetNativeWindow()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  return native_window_;
}

bool AndroidSurfaceManager::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  if (config == nullptr) {
    FML_LOG(ERROR)
        << "AndroidSurfaceManager::CreateBackingStore: config is null.";
    return false;
  }
  if (config->struct_size < sizeof(FlutterBackingStoreConfig)) {
    FML_LOG(ERROR)
        << "AndroidSurfaceManager::CreateBackingStore: invalid struct_size: "
        << config->struct_size;
    return false;
  }
  if (backing_store_out == nullptr) {
    FML_LOG(ERROR) << "AndroidSurfaceManager::CreateBackingStore: "
                      "backing_store_out is null.";
    return false;
  }

  const size_t width = static_cast<size_t>(config->size.width);
  const size_t height = static_cast<size_t>(config->size.height);
  if (width == 0 || height == 0) {
    FML_LOG(ERROR)
        << "AndroidSurfaceManager::CreateBackingStore: invalid dimensions "
        << width << "x" << height;
    return false;
  }

  std::unique_ptr<BackingStoreData> store_data;
  const uint64_t size_key = MakeSizeKey(width, height);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pool_.find(size_key);
    if (it != pool_.end() && !it->second.empty()) {
      store_data = std::move(it->second.front());
      it->second.pop_front();
    }
  }

  if (!store_data) {
    store_data = std::make_unique<BackingStoreData>();
    store_data->width = width;
    store_data->height = height;

    switch (rendering_api_) {
#if !SLIMPELLER
      case AndroidRenderingAPI::kSoftware: {
        store_data->type = kFlutterBackingStoreTypeSoftware;
        if (height > 0 && width > std::numeric_limits<size_t>::max() /
                                      (height * kBytesPerPixelRGBA8888)) {
          FML_LOG(ERROR)
              << "AndroidSurfaceManager: Dimensions overflow buffer size.";
          return false;
        }
        const size_t buffer_size = width * height * kBytesPerPixelRGBA8888;
        store_data->software_buffer.resize(buffer_size, 0);
        break;
      }
      case AndroidRenderingAPI::kSkiaOpenGLES:
#endif  // !SLIMPELLER
      case AndroidRenderingAPI::kImpellerOpenGLES: {
        store_data->type = kFlutterBackingStoreTypeOpenGL;
        // Default root framebuffer is 0. Overlays can assign allocated FBOs.
        store_data->gl_framebuffer_id = 0;
        break;
      }
      case AndroidRenderingAPI::kImpellerVulkan:
      case AndroidRenderingAPI::kImpellerAutoselect: {
        store_data->type = kFlutterBackingStoreTypeVulkan;
        store_data->vulkan_image.struct_size = sizeof(FlutterVulkanImage);
        store_data->vulkan_image.width = width;
        store_data->vulkan_image.height = height;
        break;
      }
    }
  }

  backing_store_out->struct_size = sizeof(FlutterBackingStore);
  backing_store_out->type = store_data->type;
  backing_store_out->did_update = true;
  backing_store_out->user_data = store_data.get();

  switch (store_data->type) {
    case kFlutterBackingStoreTypeSoftware:
      backing_store_out->software.allocation =
          store_data->software_buffer.data();
      backing_store_out->software.row_bytes = width * kBytesPerPixelRGBA8888;
      backing_store_out->software.height = height;
      backing_store_out->software.user_data = store_data.get();
      backing_store_out->software.destruction_callback = nullptr;
      break;

    case kFlutterBackingStoreTypeOpenGL:
      backing_store_out->open_gl.type = kFlutterOpenGLTargetTypeFramebuffer;
      backing_store_out->open_gl.framebuffer.target = kGLFramebufferFormatRGBA8;
      backing_store_out->open_gl.framebuffer.name =
          store_data->gl_framebuffer_id;
      backing_store_out->open_gl.framebuffer.user_data = store_data.get();
      backing_store_out->open_gl.framebuffer.destruction_callback = nullptr;
      break;

    case kFlutterBackingStoreTypeVulkan:
      backing_store_out->vulkan.struct_size = sizeof(FlutterVulkanBackingStore);
      backing_store_out->vulkan.image = &store_data->vulkan_image;
      backing_store_out->vulkan.user_data = store_data.get();
      backing_store_out->vulkan.destruction_callback = nullptr;
      break;

    case kFlutterBackingStoreTypeSoftware2:
    case kFlutterBackingStoreTypeMetal:
    default:
      FML_LOG(ERROR)
          << "AndroidSurfaceManager: Unsupported backing store type.";
      return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    in_flight_.insert(store_data.get());
  }

  // Release unique ownership of store_data; ownership is managed until
  // CollectBackingStore is called with backing_store->user_data.
  store_data.release();
  return true;
}

bool AndroidSurfaceManager::CollectBackingStore(
    const FlutterBackingStore* backing_store) {
  if (backing_store == nullptr) {
    FML_LOG(ERROR)
        << "AndroidSurfaceManager::CollectBackingStore: backing_store is null.";
    return false;
  }
  if (backing_store->struct_size < sizeof(FlutterBackingStore)) {
    FML_LOG(ERROR)
        << "AndroidSurfaceManager::CollectBackingStore: invalid struct_size: "
        << backing_store->struct_size;
    return false;
  }
  if (backing_store->user_data == nullptr) {
    FML_LOG(ERROR) << "AndroidSurfaceManager::CollectBackingStore: "
                      "backing_store->user_data is null.";
    return false;
  }

  auto data = static_cast<BackingStoreData*>(backing_store->user_data);
  const uint64_t size_key = MakeSizeKey(data->width, data->height);

  std::lock_guard<std::mutex> lock(mutex_);
  auto it = in_flight_.find(data);
  if (it == in_flight_.end()) {
    FML_LOG(ERROR) << "AndroidSurfaceManager::CollectBackingStore: backing "
                      "store was not in-flight (double collection?).";
    return false;
  }
  in_flight_.erase(it);
  pool_[size_key].push_back(std::unique_ptr<BackingStoreData>(data));
  return true;
}

void AndroidSurfaceManager::ClearBackingStorePool() {
  std::lock_guard<std::mutex> lock(mutex_);
  pool_.clear();
}

void AndroidSurfaceManager::TrimBackingStorePool(size_t max_cached_per_size) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& [key, queue] : pool_) {
    while (queue.size() > max_cached_per_size) {
      queue.pop_front();
    }
  }
}

size_t AndroidSurfaceManager::GetCachedBackingStoreCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  size_t count = 0;
  for (const auto& [key, queue] : pool_) {
    count += queue.size();
  }
  return count;
}

}  // namespace flutter
