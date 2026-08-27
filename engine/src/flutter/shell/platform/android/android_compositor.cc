// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <cmath>

#include "flutter/fml/logging.h"

namespace flutter {

AndroidCompositor::AndroidCompositor(
    std::shared_ptr<AndroidSurfaceManager> surface_manager,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    fml::RefPtr<fml::TaskRunner> raster_task_runner,
    fml::RefPtr<fml::TaskRunner> platform_task_runner)
    : surface_manager_(std::move(surface_manager)),
      jni_facade_(std::move(jni_facade)),
      raster_task_runner_(std::move(raster_task_runner)),
      platform_task_runner_(std::move(platform_task_runner)) {}

AndroidCompositor::~AndroidCompositor() = default;

FlutterCompositor AndroidCompositor::GetCompositorConfig() {
  FlutterCompositor compositor = {};
  compositor.struct_size = sizeof(FlutterCompositor);
  compositor.user_data = this;
  compositor.create_backing_store_callback = &AndroidCompositor::OnCreateBackingStore;
  compositor.collect_backing_store_callback = &AndroidCompositor::OnCollectBackingStore;
  compositor.present_view_callback = &AndroidCompositor::OnPresentView;
  compositor.avoid_backing_store_cache = false;
  return compositor;
}

bool AndroidCompositor::OnCreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  if (!user_data) {
    FML_LOG(ERROR) << "AndroidCompositor user_data was null during CreateBackingStore.";
    return false;
  }
  auto* compositor = static_cast<AndroidCompositor*>(user_data);
  return compositor->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositor::OnCollectBackingStore(
    const FlutterBackingStore* backing_store,
    void* user_data) {
  if (!user_data) {
    FML_LOG(ERROR) << "AndroidCompositor user_data was null during CollectBackingStore.";
    return false;
  }
  auto* compositor = static_cast<AndroidCompositor*>(user_data);
  return compositor->CollectBackingStore(backing_store);
}

bool AndroidCompositor::OnPresentView(
    const FlutterPresentViewInfo* present_info) {
  if (!present_info || !present_info->user_data) {
    FML_LOG(ERROR) << "AndroidCompositor present_info or user_data was null during PresentView.";
    return false;
  }
  auto* compositor = static_cast<AndroidCompositor*>(present_info->user_data);
  return compositor->PresentView(present_info);
}

bool AndroidCompositor::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  if (!surface_manager_) {
    FML_LOG(ERROR) << "AndroidCompositor has no surface manager.";
    return false;
  }
  if (!config || config->struct_size < sizeof(FlutterBackingStoreConfig)) {
    FML_LOG(ERROR) << "Invalid FlutterBackingStoreConfig provided.";
    return false;
  }
  if (!backing_store_out) {
    FML_LOG(ERROR) << "Invalid FlutterBackingStore buffer provided.";
    return false;
  }

  return surface_manager_->CreateBackingStore(*config, backing_store_out);
}

bool AndroidCompositor::CollectBackingStore(
    const FlutterBackingStore* backing_store) {
  if (!surface_manager_) {
    FML_LOG(ERROR) << "AndroidCompositor has no surface manager.";
    return false;
  }
  if (!backing_store ||
      backing_store->struct_size < sizeof(FlutterBackingStore)) {
    FML_LOG(ERROR) << "Invalid FlutterBackingStore provided for collection.";
    return false;
  }

  return surface_manager_->CollectBackingStore(backing_store);
}

bool AndroidCompositor::PresentView(
    const FlutterPresentViewInfo* present_info) {
  if (!present_info ||
      present_info->struct_size < sizeof(FlutterPresentViewInfo)) {
    FML_LOG(ERROR) << "Invalid FlutterPresentViewInfo supplied.";
    return false;
  }
  if (present_info->layers_count > 0 && !present_info->layers) {
    FML_LOG(ERROR) << "Null layers array supplied with non-zero layer count.";
    return false;
  }

  return Present(present_info->view_id, present_info->layers,
                 present_info->layers_count);
}

bool AndroidCompositor::Present(FlutterViewId view_id,
                                const FlutterLayer** layers,
                                size_t layers_count) {
  if (!surface_manager_) {
    FML_LOG(ERROR) << "Cannot present without a surface manager.";
    return false;
  }

  if (!surface_manager_->HasNativeWindow()) {
    FML_LOG(INFO) << "Cannot present frame before native surface is attached.";
    return false;
  }

  if (layers_count > 0 && !layers) {
    FML_LOG(ERROR) << "Null layers array passed with layers_count > 0.";
    return false;
  }

  PlatformViewRendererCallback platform_view_renderer_copy;
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    platform_view_renderer_copy = platform_view_renderer_;
  }

  for (size_t i = 0; i < layers_count; ++i) {
    const FlutterLayer* layer = layers[i];
    if (!layer || layer->struct_size < sizeof(FlutterLayer)) {
      FML_LOG(ERROR) << "Invalid layer at index " << i;
      return false;
    }

    if (!std::isfinite(layer->offset.x) || !std::isfinite(layer->offset.y) ||
        !std::isfinite(layer->size.width) || !std::isfinite(layer->size.height) ||
        layer->size.width < 0.0 || layer->size.height < 0.0) {
      FML_LOG(ERROR) << "Invalid geometry on layer at index " << i;
      return false;
    }

    switch (layer->type) {
      case kFlutterLayerContentTypeBackingStore: {
        if (!layer->backing_store ||
            layer->backing_store->struct_size < sizeof(FlutterBackingStore)) {
          FML_LOG(ERROR) << "Backing store layer at index " << i
                         << " has invalid backing store pointer.";
          return false;
        }
        break;
      }
      case kFlutterLayerContentTypePlatformView: {
        if (!layer->platform_view ||
            layer->platform_view->struct_size < sizeof(FlutterPlatformView)) {
          FML_LOG(ERROR) << "Platform view layer at index " << i
                         << " has invalid platform view pointer.";
          return false;
        }
        if (platform_view_renderer_copy) {
          if (!platform_view_renderer_copy(layer->platform_view, *layer, i)) {
            FML_LOG(ERROR) << "Platform view presentation failed for view "
                           << layer->platform_view->identifier;
            return false;
          }
        }
        break;
      }
      default:
        FML_LOG(ERROR) << "Unknown layer content type at index " << i;
        return false;
    }
  }

  if (!surface_manager_->SwapBuffers()) {
    FML_LOG(ERROR) << "Failed to swap display buffers.";
    return false;
  }

  present_count_++;
  return true;
}

void AndroidCompositor::OnSurfaceCreated(
    fml::RefPtr<AndroidNativeWindow> window) {
  if (raster_task_runner_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        raster_task_runner_,
        [&latch, surface_manager = surface_manager_,
         window = std::move(window)]() mutable {
          if (surface_manager) {
            surface_manager->SetNativeWindow(std::move(window));
          }
          latch.Signal();
        });
    latch.Wait();
  } else if (surface_manager_) {
    surface_manager_->SetNativeWindow(std::move(window));
  }
}

void AndroidCompositor::OnSurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> window) {
  if (raster_task_runner_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        raster_task_runner_,
        [&latch, surface_manager = surface_manager_,
         window = std::move(window)]() mutable {
          if (surface_manager) {
            surface_manager->ClearNativeWindow();
            surface_manager->SetNativeWindow(std::move(window));
          }
          latch.Signal();
        });
    latch.Wait();
  } else if (surface_manager_) {
    surface_manager_->ClearNativeWindow();
    surface_manager_->SetNativeWindow(std::move(window));
  }
}

void AndroidCompositor::OnSurfaceDestroyed() {
  if (raster_task_runner_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        raster_task_runner_,
        [&latch, surface_manager = surface_manager_]() {
          if (surface_manager) {
            surface_manager->ClearNativeWindow();
          }
          latch.Signal();
        });
    latch.Wait();
  } else if (surface_manager_) {
    surface_manager_->ClearNativeWindow();
  }
}

void AndroidCompositor::OnSurfaceResized(const FlutterSize& size) {
  if (!std::isfinite(size.width) || !std::isfinite(size.height) ||
      size.width <= 0.0 || size.height <= 0.0) {
    FML_LOG(ERROR) << "Invalid surface size supplied to OnSurfaceResized.";
    return;
  }
  if (raster_task_runner_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        raster_task_runner_,
        [&latch, surface_manager = surface_manager_]() {
          if (surface_manager) {
            surface_manager->ClearBackingStoreCache();
          }
          latch.Signal();
        });
    latch.Wait();
  } else if (surface_manager_) {
    surface_manager_->ClearBackingStoreCache();
  }
}

void AndroidCompositor::SetPlatformViewRendererCallback(
    PlatformViewRendererCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  platform_view_renderer_ = std::move(callback);
}

size_t AndroidCompositor::GetPresentCount() const {
  return present_count_;
}

std::shared_ptr<AndroidSurfaceManager> AndroidCompositor::GetSurfaceManager()
    const {
  return surface_manager_;
}

std::shared_ptr<PlatformViewAndroidJNI> AndroidCompositor::GetJNIFacade()
    const {
  return jni_facade_;
}

fml::RefPtr<fml::TaskRunner> AndroidCompositor::GetRasterTaskRunner() const {
  return raster_task_runner_;
}

fml::RefPtr<fml::TaskRunner> AndroidCompositor::GetPlatformTaskRunner() const {
  return platform_task_runner_;
}

bool AndroidCompositor::IsEmbedderAPIEnabled() const {
  return surface_manager_ ? surface_manager_->IsEmbedderAPIEnabled()
                          : FlutterMain::IsEmbedderAPIEnabled();
}

}  // namespace flutter
