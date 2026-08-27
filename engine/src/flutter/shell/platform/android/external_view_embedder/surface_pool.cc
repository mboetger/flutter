// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/external_view_embedder/surface_pool.h"

#include <utility>

namespace flutter {

OverlayLayer::OverlayLayer(int id,
                           std::unique_ptr<AndroidSurface> android_surface)
    : id(id), android_surface(std::move(android_surface)) {}

OverlayLayer::~OverlayLayer() = default;

SurfacePool::SurfacePool(bool use_new_surface_methods)
    : use_new_surface_methods_(use_new_surface_methods) {}

SurfacePool::~SurfacePool() = default;

std::shared_ptr<OverlayLayer> SurfacePool::GetLayer(
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<AndroidSurfaceFactory>& surface_factory) {
  std::lock_guard lock(mutex_);
  if (requested_frame_size_ != current_frame_size_) {
    if (use_new_surface_methods_) {
      for (const std::shared_ptr<OverlayLayer>& layer : layers_) {
        layer->android_surface->OnScreenSurfaceResize(requested_frame_size_);
      }
    } else {
      DestroyLayersLocked(jni_facade);
    }
  }

  if (available_layer_index_ >= layers_.size()) {
    std::unique_ptr<AndroidSurface> android_surface =
        surface_factory->CreateSurface();

    FML_CHECK(android_surface && android_surface->IsValid())
        << "Could not create an OpenGL, Vulkan or Software surface to set up "
           "rendering.";

    std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata> java_metadata =
        use_new_surface_methods_
            ? jni_facade->createOverlaySurface2()
            : jni_facade->FlutterViewCreateOverlaySurface();

    FML_CHECK(java_metadata->window);
    android_surface->SetNativeWindow(java_metadata->window, jni_facade);
    android_surface->SetupImpellerSurface();

    std::shared_ptr<OverlayLayer> layer = std::make_shared<OverlayLayer>(
        java_metadata->id, std::move(android_surface));
    layers_.push_back(layer);
  }

  std::shared_ptr<OverlayLayer> layer = layers_[available_layer_index_];
  available_layer_index_++;
  current_frame_size_ = requested_frame_size_;
  return layer;
}

std::vector<std::shared_ptr<OverlayLayer>> SurfacePool::GetUnusedLayers() {
  std::lock_guard lock(mutex_);
  std::vector<std::shared_ptr<OverlayLayer>> results;
  for (size_t i = available_layer_index_; i < layers_.size(); i++) {
    results.push_back(layers_[i]);
  }
  return results;
}

void SurfacePool::RecycleLayers() {
  std::lock_guard lock(mutex_);
  available_layer_index_ = 0;
}

bool SurfacePool::HasLayers() {
  std::lock_guard lock(mutex_);
  return !layers_.empty();
}

void SurfacePool::ResetLayers() {
  std::lock_guard lock(mutex_);
  available_layer_index_ = 0;
}

void SurfacePool::TrimLayers() {
  std::lock_guard lock(mutex_);
  layers_.erase(layers_.begin() + available_layer_index_, layers_.end());
  available_layer_index_ = 0;
}

void SurfacePool::SetFrameSize(DlISize size) {
  std::lock_guard lock(mutex_);
  requested_frame_size_ = size;
}

void SurfacePool::DestroyLayers(
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) {
  std::lock_guard lock(mutex_);
  DestroyLayersLocked(jni_facade);
}

void SurfacePool::DestroyLayersLocked(
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) {
  for (const std::shared_ptr<OverlayLayer>& layer : layers_) {
    if (use_new_surface_methods_) {
      jni_facade->destroyOverlaySurface2();
    } else {
      jni_facade->FlutterViewDestroyOverlaySurfaces();
    }
    layer->android_surface->TeardownOnScreenContext();
  }
  layers_.clear();
  available_layer_index_ = 0;
}

std::vector<std::shared_ptr<OverlayLayer>> SurfacePool::GetLayers() const {
  std::lock_guard lock(mutex_);
  return layers_;
}

}  // namespace flutter
