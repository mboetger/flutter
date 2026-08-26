// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <utility>

namespace flutter {

static bool CreateBackingStoreCallback(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  if (user_data == nullptr) {
    return false;
  }
  return reinterpret_cast<AndroidCompositor*>(user_data)
      ->CreateBackingStore(config, backing_store_out);
}

static bool CollectBackingStoreCallback(const FlutterBackingStore* renderer,
                                        void* user_data) {
  if (user_data == nullptr) {
    return false;
  }
  return reinterpret_cast<AndroidCompositor*>(user_data)
      ->CollectBackingStore(renderer);
}

static bool PresentViewCallback(const FlutterPresentViewInfo* info) {
  if (info == nullptr || info->user_data == nullptr) {
    return false;
  }
  return reinterpret_cast<AndroidCompositor*>(info->user_data)
      ->Present(info->view_id, info->layers, info->layers_count);
}

AndroidCompositor::AndroidCompositor(
    std::shared_ptr<AndroidSurfaceManager> surface_manager,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade)
    : surface_manager_(std::move(surface_manager)),
      jni_facade_(std::move(jni_facade)) {}

AndroidCompositor::~AndroidCompositor() = default;

FlutterCompositor AndroidCompositor::GetCompositor() {
  FlutterCompositor compositor = {};
  compositor.struct_size = sizeof(FlutterCompositor);
  compositor.user_data = this;
  compositor.create_backing_store_callback = CreateBackingStoreCallback;
  compositor.collect_backing_store_callback = CollectBackingStoreCallback;
  compositor.present_view_callback = PresentViewCallback;
  compositor.avoid_backing_store_cache = false;
  return compositor;
}

bool AndroidCompositor::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  if (surface_manager_ == nullptr) {
    return false;
  }
  return surface_manager_->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositor::CollectBackingStore(const FlutterBackingStore* store) {
  if (surface_manager_ == nullptr) {
    return false;
  }
  return surface_manager_->CollectBackingStore(store);
}

bool AndroidCompositor::Present(FlutterViewId view_id,
                                const FlutterLayer** layers,
                                size_t layers_count) {
  if (layers == nullptr && layers_count > 0) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  PresentedFrame frame;
  frame.view_id = view_id;

  for (size_t i = 0; i < layers_count; ++i) {
    const FlutterLayer* layer = layers[i];
    if (layer == nullptr) {
      continue;
    }

    if (i == 0) {
      frame.presentation_time = layer->presentation_time;
    }

    switch (layer->type) {
      case kFlutterLayerContentTypeBackingStore:
        frame.backing_store_count++;
        break;
      case kFlutterLayerContentTypePlatformView:
        frame.platform_view_count++;
        if (layer->platform_view != nullptr) {
          frame.platform_view_ids.push_back(layer->platform_view->identifier);
        }
        break;
    }
  }

  last_presented_frame_ = std::move(frame);
  return true;
}

void AndroidCompositor::AddView(FlutterViewId view_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  active_views_[view_id] = true;
}

void AndroidCompositor::RemoveView(FlutterViewId view_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  active_views_.erase(view_id);
}

AndroidCompositor::PresentedFrame AndroidCompositor::GetLastPresentedFrame()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  return last_presented_frame_;
}

size_t AndroidCompositor::GetViewCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_views_.size();
}

}  // namespace flutter
