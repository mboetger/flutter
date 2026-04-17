// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor_software.h"
#include "flutter/fml/logging.h"
#include "flutter/impeller/toolkit/android/hardware_buffer.h"
#include "flutter/impeller/toolkit/android/surface_control.h"
#include "flutter/impeller/toolkit/android/surface_transaction.h"

namespace flutter {

struct BackingStoreRecord {
  std::unique_ptr<impeller::android::HardwareBuffer> hardware_buffer;
};

AndroidCompositorSoftware::AndroidCompositorSoftware() = default;
AndroidCompositorSoftware::~AndroidCompositorSoftware() = default;

void AndroidCompositorSoftware::SetNativeWindow(ANativeWindow* window) {
  window_ = window;
  if (window) {
    root_surface_control_ = std::make_unique<impeller::android::SurfaceControl>(
        window, "Flutter Root");
  } else {
    root_surface_control_.reset();
  }
}

bool AndroidCompositorSoftware::CreateBackingStoreCallback(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  auto* self = static_cast<AndroidCompositorSoftware*>(user_data);
  return self->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositorSoftware::CollectBackingStoreCallback(
    const FlutterBackingStore* backing_store,
    void* user_data) {
  auto* self = static_cast<AndroidCompositorSoftware*>(user_data);
  return self->CollectBackingStore(backing_store);
}

bool AndroidCompositorSoftware::PresentViewCallback(
    const FlutterPresentViewInfo* info) {
  auto* self = static_cast<AndroidCompositorSoftware*>(info->user_data);
  return self->PresentView(info);
}

bool AndroidCompositorSoftware::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  impeller::ISize size(config->size.width, config->size.height);

  impeller::android::HardwareBufferDescriptor desc;
  desc.format = impeller::android::HardwareBufferFormat::kR8G8B8A8UNormInt;
  desc.size = size;
  desc.usage = static_cast<impeller::android::HardwareBufferUsageFlags>(
      static_cast<uint32_t>(
          impeller::android::HardwareBufferUsageFlags::kCPUWriteOften) |
      static_cast<uint32_t>(
          impeller::android::HardwareBufferUsageFlags::kCompositorOverlay));

  auto hardware_buffer =
      std::make_unique<impeller::android::HardwareBuffer>(desc);
  if (!hardware_buffer->IsValid()) {
    FML_LOG(ERROR)
        << "Failed to allocate AHardwareBuffer for software rendering.";
    return false;
  }

  void* allocation = hardware_buffer->Lock(
      impeller::android::HardwareBuffer::CPUAccessType::kWrite);
  if (!allocation) {
    FML_LOG(ERROR) << "Failed to lock AHardwareBuffer for CPU access.";
    return false;
  }

  auto record = std::make_unique<BackingStoreRecord>();
  record->hardware_buffer = std::move(hardware_buffer);

  backing_store_out->type = kFlutterBackingStoreTypeSoftware;
  backing_store_out->software.allocation = allocation;
  backing_store_out->software.row_bytes =
      size.width * 4;  // RGBA8 is 4 bytes per pixel
  backing_store_out->software.height = size.height;
  backing_store_out->software.user_data = record.release();
  backing_store_out->software.destruction_callback = [](void* p) {
    auto* r = static_cast<BackingStoreRecord*>(p);
    r->hardware_buffer->Unlock();
    delete r;
  };

  return true;
}

bool AndroidCompositorSoftware::CollectBackingStore(
    const FlutterBackingStore* backing_store) {
  if (backing_store->type == kFlutterBackingStoreTypeSoftware) {
    if (backing_store->software.destruction_callback) {
      backing_store->software.destruction_callback(
          backing_store->software.user_data);
    }
  }
  return true;
}

bool AndroidCompositorSoftware::PresentView(
    const FlutterPresentViewInfo* info) {
  if (!root_surface_control_ || !root_surface_control_->IsValid()) {
    FML_LOG(ERROR) << "No valid root surface control.";
    return false;
  }

  impeller::android::SurfaceTransaction transaction;
  if (!transaction.IsValid()) {
    FML_LOG(ERROR) << "Failed to create surface transaction.";
    return false;
  }

  for (size_t i = 0; i < info->layers_count; ++i) {
    const FlutterLayer* layer = info->layers[i];
    if (layer->type == kFlutterLayerContentTypeBackingStore) {
      const FlutterBackingStore* backing_store = layer->backing_store;
      if (backing_store->type == kFlutterBackingStoreTypeSoftware) {
        auto* record =
            static_cast<BackingStoreRecord*>(backing_store->software.user_data);
        const impeller::android::HardwareBuffer* buffer =
            record->hardware_buffer.get();

        if (!transaction.SetContents(root_surface_control_.get(), buffer)) {
          FML_LOG(ERROR) << "Failed to set contents of surface control.";
          return false;
        }
      }
    }
  }

  return transaction.Apply();
}

}  // namespace flutter
