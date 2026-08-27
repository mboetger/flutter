// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <algorithm>
#include <cmath>

#include "flutter/fml/logging.h"

namespace flutter {

void AndroidPlatformViewMutatorsStack::PushTransform(const float matrix[9]) {
  AndroidPlatformViewMutator mutator;
  mutator.type = AndroidMutatorType::kTransform;
  std::copy(matrix, matrix + 9, mutator.transform_matrix);
  mutators_.push_back(std::move(mutator));
}

void AndroidPlatformViewMutatorsStack::PushClipRect(float left,
                                                    float top,
                                                    float right,
                                                    float bottom) {
  AndroidPlatformViewMutator mutator;
  mutator.type = AndroidMutatorType::kClipRect;
  mutator.rect = FlutterRect{left, top, right, bottom};
  mutators_.push_back(std::move(mutator));
}

void AndroidPlatformViewMutatorsStack::PushClipRRect(float left,
                                                     float top,
                                                     float right,
                                                     float bottom,
                                                     const float radii[8]) {
  AndroidPlatformViewMutator mutator;
  mutator.type = AndroidMutatorType::kClipRRect;
  mutator.rect = FlutterRect{left, top, right, bottom};
  std::copy(radii, radii + 8, mutator.radii);
  mutators_.push_back(std::move(mutator));
}

void AndroidPlatformViewMutatorsStack::PushClipRSE(float left,
                                                   float top,
                                                   float right,
                                                   float bottom,
                                                   const float radii[8]) {
  AndroidPlatformViewMutator mutator;
  mutator.type = AndroidMutatorType::kClipRSE;
  mutator.rect = FlutterRect{left, top, right, bottom};
  std::copy(radii, radii + 8, mutator.radii);
  mutators_.push_back(std::move(mutator));
}

void AndroidPlatformViewMutatorsStack::PushOpacity(float opacity) {
  AndroidPlatformViewMutator mutator;
  mutator.type = AndroidMutatorType::kOpacity;
  mutator.opacity =
      std::isfinite(opacity) ? std::clamp(opacity, 0.0f, 1.0f) : 1.0f;
  mutators_.push_back(std::move(mutator));
}

void AndroidPlatformViewMutatorsStack::PushClipPath(
    const AndroidPathData& path) {
  AndroidPlatformViewMutator mutator;
  mutator.type = AndroidMutatorType::kClipPath;
  mutator.path = path;
  mutators_.push_back(std::move(mutator));
}

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
  compositor.create_backing_store_callback =
      &AndroidCompositor::OnCreateBackingStore;
  compositor.collect_backing_store_callback =
      &AndroidCompositor::OnCollectBackingStore;
  compositor.present_view_callback = &AndroidCompositor::OnPresentView;
  compositor.avoid_backing_store_cache = false;
  return compositor;
}

bool AndroidCompositor::OnCreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  if (!user_data) {
    FML_LOG(ERROR)
        << "AndroidCompositor user_data was null during CreateBackingStore.";
    return false;
  }
  auto* compositor = static_cast<AndroidCompositor*>(user_data);
  return compositor->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositor::OnCollectBackingStore(
    const FlutterBackingStore* backing_store,
    void* user_data) {
  if (!user_data) {
    FML_LOG(ERROR)
        << "AndroidCompositor user_data was null during CollectBackingStore.";
    return false;
  }
  auto* compositor = static_cast<AndroidCompositor*>(user_data);
  return compositor->CollectBackingStore(backing_store);
}

bool AndroidCompositor::OnPresentView(
    const FlutterPresentViewInfo* present_info) {
  if (!present_info || !present_info->user_data) {
    FML_LOG(ERROR) << "AndroidCompositor present_info or user_data was null "
                      "during PresentView.";
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

void AndroidCompositor::SetDevicePixelRatio(double dpr) {
  if (std::isfinite(dpr) && dpr > 0.0) {
    device_pixel_ratio_.store(dpr);
  }
}

double AndroidCompositor::GetDevicePixelRatio() const {
  return device_pixel_ratio_.load();
}

void AndroidCompositor::NormalizeRootTransform(
    const FlutterTransformation& in_transform,
    double dpr,
    float out_matrix[9]) {
  if (!std::isfinite(dpr) || dpr <= 0.0) {
    dpr = 1.0;
  }

  // Row-major 3x3 layout matching android.graphics.Matrix.setValues:
  // [0] scaleX   [1] skewX   [2] transX
  // [3] skewY    [4] scaleY  [5] transY
  // [6] pers0    [7] pers1   [8] pers2
  out_matrix[0] = static_cast<float>(in_transform.scaleX / dpr);
  out_matrix[1] = static_cast<float>(in_transform.skewX / dpr);
  out_matrix[2] = static_cast<float>(in_transform.transX / dpr);
  out_matrix[3] = static_cast<float>(in_transform.skewY / dpr);
  out_matrix[4] = static_cast<float>(in_transform.scaleY / dpr);
  out_matrix[5] = static_cast<float>(in_transform.transY / dpr);
  out_matrix[6] = static_cast<float>(in_transform.pers0);
  out_matrix[7] = static_cast<float>(in_transform.pers1);
  out_matrix[8] = static_cast<float>(in_transform.pers2);
}

bool AndroidCompositor::PopulateMutatorsStack(
    const FlutterPlatformView* platform_view,
    AndroidPlatformViewMutatorsStack* stack_out,
    double dpr) const {
  if (!platform_view || !stack_out) {
    return false;
  }

  stack_out->Clear();

  if (platform_view->mutations_count == 0) {
    return true;
  }

  if (!platform_view->mutations) {
    FML_LOG(ERROR) << "Null mutations array with non-zero mutations_count.";
    return false;
  }

  if (!std::isfinite(dpr) || dpr <= 0.0) {
    dpr = 1.0;
  }

  for (size_t i = 0; i < platform_view->mutations_count; ++i) {
    const FlutterPlatformViewMutation* mutation = platform_view->mutations[i];
    if (!mutation) {
      FML_LOG(ERROR) << "Null mutation at index " << i;
      return false;
    }

    switch (mutation->type) {
      case kFlutterPlatformViewMutationTypeOpacity: {
        if (!std::isfinite(mutation->opacity)) {
          FML_LOG(ERROR) << "Invalid non-finite opacity at index " << i;
          return false;
        }
        stack_out->PushOpacity(static_cast<float>(mutation->opacity));
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRect: {
        const auto& rect = mutation->clip_rect;
        if (!std::isfinite(rect.left) || !std::isfinite(rect.top) ||
            !std::isfinite(rect.right) || !std::isfinite(rect.bottom) ||
            rect.right < rect.left || rect.bottom < rect.top) {
          FML_LOG(ERROR) << "Invalid clip rect at index " << i;
          return false;
        }
        stack_out->PushClipRect(rect.left, rect.top, rect.right, rect.bottom);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRoundedRect: {
        const auto& rrect = mutation->clip_rounded_rect;
        if (!std::isfinite(rrect.rect.left) || !std::isfinite(rrect.rect.top) ||
            !std::isfinite(rrect.rect.right) ||
            !std::isfinite(rrect.rect.bottom) ||
            rrect.rect.right < rrect.rect.left ||
            rrect.rect.bottom < rrect.rect.top) {
          FML_LOG(ERROR) << "Invalid clip rounded rect at index " << i;
          return false;
        }
        float radii[8] = {
            static_cast<float>(rrect.upper_left_corner_radius.width),
            static_cast<float>(rrect.upper_left_corner_radius.height),
            static_cast<float>(rrect.upper_right_corner_radius.width),
            static_cast<float>(rrect.upper_right_corner_radius.height),
            static_cast<float>(rrect.lower_right_corner_radius.width),
            static_cast<float>(rrect.lower_right_corner_radius.height),
            static_cast<float>(rrect.lower_left_corner_radius.width),
            static_cast<float>(rrect.lower_left_corner_radius.height),
        };
        for (int r = 0; r < 8; ++r) {
          if (!std::isfinite(radii[r]) || radii[r] < 0.0f) {
            FML_LOG(ERROR) << "Invalid corner radius at index " << i;
            return false;
          }
        }
        stack_out->PushClipRRect(rrect.rect.left, rrect.rect.top,
                                 rrect.rect.right, rrect.rect.bottom, radii);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRoundedSuperellipse: {
        const auto& rse = mutation->clip_rounded_superellipse;
        if (!std::isfinite(rse.rect.left) || !std::isfinite(rse.rect.top) ||
            !std::isfinite(rse.rect.right) || !std::isfinite(rse.rect.bottom) ||
            rse.rect.right < rse.rect.left || rse.rect.bottom < rse.rect.top) {
          FML_LOG(ERROR) << "Invalid superellipse rect at index " << i;
          return false;
        }
        float radii[8] = {
            static_cast<float>(rse.upper_left_corner_radius.width),
            static_cast<float>(rse.upper_left_corner_radius.height),
            static_cast<float>(rse.upper_right_corner_radius.width),
            static_cast<float>(rse.upper_right_corner_radius.height),
            static_cast<float>(rse.lower_right_corner_radius.width),
            static_cast<float>(rse.lower_right_corner_radius.height),
            static_cast<float>(rse.lower_left_corner_radius.width),
            static_cast<float>(rse.lower_left_corner_radius.height),
        };
        for (int r = 0; r < 8; ++r) {
          if (!std::isfinite(radii[r]) || radii[r] < 0.0f) {
            FML_LOG(ERROR) << "Invalid superellipse radius at index " << i;
            return false;
          }
        }
        stack_out->PushClipRSE(rse.rect.left, rse.rect.top, rse.rect.right,
                               rse.rect.bottom, radii);
        break;
      }
      case kFlutterPlatformViewMutationTypeTransformation: {
        const auto& t = mutation->transformation;
        if (!std::isfinite(t.scaleX) || !std::isfinite(t.skewX) ||
            !std::isfinite(t.transX) || !std::isfinite(t.skewY) ||
            !std::isfinite(t.scaleY) || !std::isfinite(t.transY) ||
            !std::isfinite(t.pers0) || !std::isfinite(t.pers1) ||
            !std::isfinite(t.pers2)) {
          FML_LOG(ERROR) << "Invalid non-finite transformation matrix at index "
                         << i;
          return false;
        }
        // Row-major 3x3 layout matching android.graphics.Matrix.setValues:
        // [0] scaleX   [1] skewX   [2] transX
        // [3] skewY    [4] scaleY  [5] transY
        // [6] pers0    [7] pers1   [8] pers2
        float matrix[9] = {
            static_cast<float>(t.scaleX), static_cast<float>(t.skewX),
            static_cast<float>(t.transX), static_cast<float>(t.skewY),
            static_cast<float>(t.scaleY), static_cast<float>(t.transY),
            static_cast<float>(t.pers0),  static_cast<float>(t.pers1),
            static_cast<float>(t.pers2),
        };
        stack_out->PushTransform(matrix);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipPath: {
        const auto& path = mutation->clip_path;
        if (path.segments_count > 0 && !path.segments) {
          FML_LOG(ERROR) << "Null path segments with non-zero count at index "
                         << i;
          return false;
        }
        AndroidPathData path_data;
        path_data.fill_type = path.fill_type;
        path_data.segments.reserve(path.segments_count);
        for (size_t s = 0; s < path.segments_count; ++s) {
          const auto& seg = path.segments[s];
          AndroidPathSegment path_seg;
          path_seg.verb = seg.verb;
          path_seg.conic_weight = seg.conic_weight;

          int required_points = 0;
          switch (seg.verb) {
            case kFlutterPathVerbMove:
            case kFlutterPathVerbLine:
              required_points = 1;
              break;
            case kFlutterPathVerbQuad:
              required_points = 2;
              break;
            case kFlutterPathVerbConic:
              required_points = 2;
              if (!std::isfinite(seg.conic_weight) || seg.conic_weight <= 0.0) {
                FML_LOG(ERROR) << "Invalid conic weight at segment " << s;
                return false;
              }
              break;
            case kFlutterPathVerbCubic:
              required_points = 3;
              break;
            case kFlutterPathVerbClose:
              required_points = 0;
              break;
            default:
              FML_LOG(ERROR) << "Unknown path verb at segment " << s;
              return false;
          }

          for (int p = 0; p < required_points; ++p) {
            if (!std::isfinite(seg.points[p].x) ||
                !std::isfinite(seg.points[p].y)) {
              FML_LOG(ERROR)
                  << "Invalid non-finite path point at segment " << s;
              return false;
            }
            path_seg.points[p] = seg.points[p];
          }
          path_data.segments.push_back(path_seg);
        }
        stack_out->PushClipPath(path_data);
        break;
      }
      default:
        FML_LOG(ERROR) << "Unknown platform view mutation type at index " << i;
        return false;
    }
  }

  return true;
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
  PlatformViewMutatorsRendererCallback platform_view_mutators_renderer_copy;
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    platform_view_renderer_copy = platform_view_renderer_;
    platform_view_mutators_renderer_copy = platform_view_mutators_renderer_;
  }

  const double dpr = device_pixel_ratio_.load();

  for (size_t i = 0; i < layers_count; ++i) {
    const FlutterLayer* layer = layers[i];
    if (!layer || layer->struct_size < sizeof(FlutterLayer)) {
      FML_LOG(ERROR) << "Invalid layer at index " << i;
      return false;
    }

    if (!std::isfinite(layer->offset.x) || !std::isfinite(layer->offset.y) ||
        !std::isfinite(layer->size.width) ||
        !std::isfinite(layer->size.height) || layer->size.width < 0.0 ||
        layer->size.height < 0.0) {
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

        AndroidPlatformViewMutatorsStack mutators_stack;
        if (!PopulateMutatorsStack(layer->platform_view, &mutators_stack,
                                   dpr)) {
          FML_LOG(ERROR) << "Failed to map mutators for platform view "
                         << layer->platform_view->identifier;
          return false;
        }

        if (platform_view_mutators_renderer_copy) {
          if (!platform_view_mutators_renderer_copy(
                  layer->platform_view, *layer, mutators_stack, i)) {
            FML_LOG(ERROR)
                << "Platform view mutators presentation failed for view "
                << layer->platform_view->identifier;
            return false;
          }
        } else if (platform_view_renderer_copy) {
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

  size_t count = ++present_count_;
  if (count == 1 && jni_facade_) {
    jni_facade_->FlutterViewOnFirstFrame();
  }
  return true;
}

void AndroidCompositor::OnSurfaceCreated(
    fml::RefPtr<AndroidNativeWindow> window) {
  if (raster_task_runner_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        raster_task_runner_, [&latch, surface_manager = surface_manager_,
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
        raster_task_runner_, [&latch, surface_manager = surface_manager_,
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
        raster_task_runner_, [&latch, surface_manager = surface_manager_]() {
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
        raster_task_runner_, [&latch, surface_manager = surface_manager_]() {
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

void AndroidCompositor::SetPlatformViewMutatorsRendererCallback(
    PlatformViewMutatorsRendererCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  platform_view_mutators_renderer_ = std::move(callback);
}

size_t AndroidCompositor::GetPresentCount() const {
  return present_count_.load();
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
