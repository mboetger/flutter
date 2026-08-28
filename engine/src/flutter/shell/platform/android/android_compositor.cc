// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <utility>

#include "flutter/fml/logging.h"

namespace flutter {

AndroidCompositor::AndroidCompositor(
    std::shared_ptr<AndroidSurfaceManager> surface_manager,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    fml::RefPtr<fml::TaskRunner> raster_task_runner)
    : surface_manager_(std::move(surface_manager)),
      jni_facade_(std::move(jni_facade)),
      raster_task_runner_(std::move(raster_task_runner)) {
  FML_CHECK(surface_manager_ != nullptr)
      << "AndroidCompositor requires a non-null AndroidSurfaceManager.";
}

AndroidCompositor::~AndroidCompositor() {
  OnSurfaceDestroyed();
}

FlutterCompositor AndroidCompositor::GetFlutterCompositor() {
  FlutterCompositor compositor = {};
  compositor.struct_size = sizeof(FlutterCompositor);
  compositor.user_data = this;
  compositor.create_backing_store_callback =
      &AndroidCompositor::OnCreateBackingStore;
  compositor.collect_backing_store_callback =
      &AndroidCompositor::OnCollectBackingStore;
  compositor.present_view_callback = &AndroidCompositor::OnPresentView;
  compositor.avoid_backing_store_cache = false;
  return compositor;
}

bool AndroidCompositor::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  return surface_manager_->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositor::CollectBackingStore(
    const FlutterBackingStore* backing_store) {
  return surface_manager_->CollectBackingStore(backing_store);
}

bool AndroidCompositor::PresentView(const FlutterPresentViewInfo* info) {
  if (info == nullptr) {
    FML_LOG(ERROR) << "AndroidCompositor::PresentView: info is null.";
    return false;
  }
  if (info->struct_size < sizeof(FlutterPresentViewInfo)) {
    FML_LOG(ERROR) << "AndroidCompositor::PresentView: invalid struct_size: "
                   << info->struct_size;
    return false;
  }

  return Present(info->view_id, info->layers, info->layers_count);
}

bool AndroidCompositor::Present(FlutterViewId view_id,
                               const FlutterLayer** layers,
                               size_t layers_count) {
  if (surface_destroyed_.load(std::memory_order_acquire) ||
      !surface_manager_->HasNativeWindow()) {
    // Drop presentation gracefully when native surface has been destroyed.
    return true;
  }

  if (layers_count > 0 && layers == nullptr) {
    FML_LOG(ERROR)
        << "AndroidCompositor::Present: layers pointer is null but layers_count is "
        << layers_count;
    return false;
  }

  std::lock_guard<std::mutex> lock(present_mutex_);
  if (surface_destroyed_.load(std::memory_order_acquire) ||
      !surface_manager_->HasNativeWindow()) {
    return true;
  }

  for (size_t i = 0; i < layers_count; ++i) {
    const FlutterLayer* layer = layers[i];
    if (layer == nullptr) {
      continue;
    }
    if (layer->struct_size < sizeof(FlutterLayer)) {
      FML_LOG(ERROR) << "AndroidCompositor::Present: invalid layer struct_size: "
                     << layer->struct_size;
      continue;
    }
    switch (layer->type) {
      case kFlutterLayerContentTypeBackingStore:
        // Backing store layer is presented to the surface/swapchain.
        break;
      case kFlutterLayerContentTypePlatformView:
        // Platform view mutations will be applied via mutators stack.
        break;
    }
  }

  presented_frame_count_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

void AndroidCompositor::OnSurfaceCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  std::lock_guard<std::mutex> lock(present_mutex_);
  surface_destroyed_.store(false, std::memory_order_release);
  surface_manager_->SetNativeWindow(std::move(native_window));
}

void AndroidCompositor::OnSurfaceDestroyed() {
  surface_destroyed_.store(true, std::memory_order_release);

  if (raster_task_runner_ &&
      !raster_task_runner_->RunsTasksOnCurrentThread()) {
    // Synchronous surface detachment barrier: blocks until the raster thread
    // drops all native window references and cleans up active raster state.
    //
    // Note: The lock is scoped strictly before latch.Signal() to prevent
    // use-after-free hazards on `present_mutex_` if the calling thread destroys
    // `this` immediately upon unblocking from latch.Wait().
    fml::AutoResetWaitableEvent latch;
    raster_task_runner_->PostTask([this, &latch]() {
      {
        std::lock_guard<std::mutex> lock(present_mutex_);
        surface_manager_->ClearNativeWindow();
        surface_manager_->ClearBackingStorePool();
      }
      latch.Signal();
    });
    latch.Wait();
  } else {
    std::lock_guard<std::mutex> lock(present_mutex_);
    surface_manager_->ClearNativeWindow();
    surface_manager_->ClearBackingStorePool();
  }
}

void AndroidCompositor::OnSurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  std::lock_guard<std::mutex> lock(present_mutex_);
  surface_destroyed_.store(false, std::memory_order_release);
  surface_manager_->SetNativeWindow(std::move(native_window));
}

bool AndroidCompositor::IsSurfaceDestroyed() const {
  return surface_destroyed_.load(std::memory_order_acquire);
}

std::shared_ptr<AndroidSurfaceManager> AndroidCompositor::GetSurfaceManager()
    const {
  return surface_manager_;
}

// static
bool AndroidCompositor::OnCreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  if (user_data == nullptr) {
    FML_LOG(ERROR) << "AndroidCompositor::OnCreateBackingStore: user_data is null.";
    return false;
  }
  return static_cast<AndroidCompositor*>(user_data)
      ->CreateBackingStore(config, backing_store_out);
}

// static
bool AndroidCompositor::OnCollectBackingStore(
    const FlutterBackingStore* backing_store,
    void* user_data) {
  if (user_data == nullptr) {
    FML_LOG(ERROR)
        << "AndroidCompositor::OnCollectBackingStore: user_data is null.";
    return false;
  }
  return static_cast<AndroidCompositor*>(user_data)
      ->CollectBackingStore(backing_store);
}

// static
bool AndroidCompositor::OnPresentView(const FlutterPresentViewInfo* info) {
  if (info == nullptr) {
    FML_LOG(ERROR) << "AndroidCompositor::OnPresentView: info is null.";
    return false;
  }
  if (info->struct_size < sizeof(FlutterPresentViewInfo)) {
    FML_LOG(ERROR) << "AndroidCompositor::OnPresentView: invalid struct_size: "
                   << info->struct_size;
    return false;
  }
  if (info->user_data == nullptr) {
    FML_LOG(ERROR) << "AndroidCompositor::OnPresentView: user_data is null.";
    return false;
  }
  return static_cast<AndroidCompositor*>(info->user_data)->PresentView(info);
}

}  // namespace flutter
