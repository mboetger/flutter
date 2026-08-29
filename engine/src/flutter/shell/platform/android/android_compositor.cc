// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <EGL/egl.h>

#include "flutter/fml/logging.h"
#include "flutter/fml/trace_event.h"

namespace flutter {

namespace {

// Maximum alpha value for 8-bit channel opacity conversion.
constexpr double kMaxAlpha = 255.0;

}  // namespace

AndroidCompositor::AndroidCompositor(
    std::shared_ptr<AndroidSurfaceManager> surface_manager,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    fml::RefPtr<fml::TaskRunner> platform_task_runner,
    fml::RefPtr<fml::TaskRunner> raster_task_runner)
    : surface_manager_(std::move(surface_manager)),
      jni_facade_(std::move(jni_facade)),
      platform_task_runner_(std::move(platform_task_runner)),
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
  TRACE_EVENT0("flutter", "AndroidCompositor::Present");

  if (surface_destroyed_.load(std::memory_order_acquire) ||
      !surface_manager_->HasNativeWindow()) {
    // Drop presentation gracefully when native surface has been destroyed.
    return true;
  }

  if (layers_count > 0 && layers == nullptr) {
    FML_LOG(ERROR) << "AndroidCompositor::Present: layers pointer is null but "
                      "layers_count is "
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
      FML_LOG(ERROR)
          << "AndroidCompositor::Present: invalid layer struct_size: "
          << layer->struct_size;
      continue;
    }
    switch (layer->type) {
      case kFlutterLayerContentTypeBackingStore:
        // Backing store layer is presented to the surface/swapchain.
        break;
      case kFlutterLayerContentTypePlatformView: {
        TRACE_EVENT0("flutter", "AndroidCompositor::PresentPlatformView");
        const FlutterPlatformView* platform_view = layer->platform_view;
        if (platform_view != nullptr && jni_facade_ != nullptr) {
          AndroidMutatorsStack mutators_stack =
              ConvertMutationsToMutatorsStack(platform_view);
          // Round floating-point physical coordinates using std::lround to
          // prevent 1-pixel rounding seams on fractional device pixel ratios.
          int x = static_cast<int>(std::lround(layer->offset.x));
          int y = static_cast<int>(std::lround(layer->offset.y));
          int width = static_cast<int>(std::lround(layer->size.width));
          int height = static_cast<int>(std::lround(layer->size.height));
          int view_width = width;
          int view_height = height;

          jni_facade_->FlutterViewOnDisplayPlatformView(
              platform_view->identifier, x, y, width, height, view_width,
              view_height, std::move(mutators_stack));
        }
        break;
      }
    }
  }

  EGLDisplay current_display = eglGetCurrentDisplay();
  EGLSurface current_surface = eglGetCurrentSurface(EGL_DRAW);
  if (current_display != EGL_NO_DISPLAY && current_surface != EGL_NO_SURFACE) {
    eglSwapBuffers(current_display, current_surface);
  }

  if (presented_frame_count_.fetch_add(1, std::memory_order_relaxed) == 0) {
    if (jni_facade_ != nullptr) {
      if (platform_task_runner_) {
        platform_task_runner_->PostTask(
            [jni = jni_facade_]() { jni->FlutterViewOnFirstFrame(); });
      } else {
        jni_facade_->FlutterViewOnFirstFrame();
      }
    }
  }
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

  if (raster_task_runner_ && !raster_task_runner_->RunsTasksOnCurrentThread()) {
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
AndroidMutatorsStack AndroidCompositor::ConvertMutationsToMutatorsStack(
    const FlutterPlatformView* platform_view) {
  TRACE_EVENT0("flutter", "AndroidCompositor::ConvertMutationsToMutatorsStack");

  AndroidMutatorsStack mutators_stack;
  if (platform_view == nullptr) {
    return mutators_stack;
  }
  if (platform_view->struct_size < sizeof(FlutterPlatformView)) {
    FML_LOG(ERROR) << "AndroidCompositor::ConvertMutationsToMutatorsStack: "
                      "invalid struct_size: "
                   << platform_view->struct_size;
    return mutators_stack;
  }

  if (platform_view->mutations == nullptr ||
      platform_view->mutations_count == 0) {
    return mutators_stack;
  }

  for (size_t i = 0; i < platform_view->mutations_count; ++i) {
    const FlutterPlatformViewMutation* mutation = platform_view->mutations[i];
    if (mutation == nullptr) {
      continue;
    }

    switch (mutation->type) {
      case kFlutterPlatformViewMutationTypeTransformation: {
        const FlutterTransformation& t = mutation->transformation;
        AndroidMutator mutator;
        mutator.type = AndroidMutatorType::kTransform;
        mutator.matrix = {
            static_cast<float>(t.scaleX), static_cast<float>(t.skewX),
            static_cast<float>(t.transX), static_cast<float>(t.skewY),
            static_cast<float>(t.scaleY), static_cast<float>(t.transY),
            static_cast<float>(t.pers0),  static_cast<float>(t.pers1),
            static_cast<float>(t.pers2),
        };
        mutators_stack.push_back(mutator);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRect: {
        const FlutterRect& r = mutation->clip_rect;
        AndroidMutator mutator;
        mutator.type = AndroidMutatorType::kClipRect;
        mutator.rect_left = static_cast<float>(r.left);
        mutator.rect_top = static_cast<float>(r.top);
        mutator.rect_right = static_cast<float>(r.right);
        mutator.rect_bottom = static_cast<float>(r.bottom);
        mutators_stack.push_back(mutator);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRoundedRect: {
        const FlutterRoundedRect& r = mutation->clip_rounded_rect;
        AndroidMutator mutator;
        mutator.type = AndroidMutatorType::kClipRRect;
        mutator.rect_left = static_cast<float>(r.rect.left);
        mutator.rect_top = static_cast<float>(r.rect.top);
        mutator.rect_right = static_cast<float>(r.rect.right);
        mutator.rect_bottom = static_cast<float>(r.rect.bottom);
        mutator.radii = {
            static_cast<float>(r.upper_left_corner_radius.width),
            static_cast<float>(r.upper_left_corner_radius.height),
            static_cast<float>(r.upper_right_corner_radius.width),
            static_cast<float>(r.upper_right_corner_radius.height),
            static_cast<float>(r.lower_right_corner_radius.width),
            static_cast<float>(r.lower_right_corner_radius.height),
            static_cast<float>(r.lower_left_corner_radius.width),
            static_cast<float>(r.lower_left_corner_radius.height),
        };
        mutators_stack.push_back(mutator);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRoundedSuperellipse: {
        const FlutterRoundedSuperellipse& r =
            mutation->clip_rounded_superellipse;
        AndroidMutator mutator;
        mutator.type = AndroidMutatorType::kClipRSE;
        mutator.rect_left = static_cast<float>(r.rect.left);
        mutator.rect_top = static_cast<float>(r.rect.top);
        mutator.rect_right = static_cast<float>(r.rect.right);
        mutator.rect_bottom = static_cast<float>(r.rect.bottom);
        mutator.radii = {
            static_cast<float>(r.upper_left_corner_radius.width),
            static_cast<float>(r.upper_left_corner_radius.height),
            static_cast<float>(r.upper_right_corner_radius.width),
            static_cast<float>(r.upper_right_corner_radius.height),
            static_cast<float>(r.lower_right_corner_radius.width),
            static_cast<float>(r.lower_right_corner_radius.height),
            static_cast<float>(r.lower_left_corner_radius.width),
            static_cast<float>(r.lower_left_corner_radius.height),
        };
        mutators_stack.push_back(mutator);
        break;
      }
      case kFlutterPlatformViewMutationTypeOpacity: {
        // Convert opacity [0.0, 1.0] to uint8_t [0, 255].
        double clamped_opacity = std::clamp(mutation->opacity, 0.0, 1.0);
        uint8_t alpha =
            static_cast<uint8_t>(std::lround(clamped_opacity * kMaxAlpha));
        AndroidMutator mutator;
        mutator.type = AndroidMutatorType::kOpacity;
        mutator.alpha = alpha;
        mutators_stack.push_back(mutator);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipPath:
        // ClipPath mutator is not directly passed to Java mutators stack.
        break;
    }
  }

  return mutators_stack;
}

// static
bool AndroidCompositor::OnCreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  if (user_data == nullptr) {
    FML_LOG(ERROR)
        << "AndroidCompositor::OnCreateBackingStore: user_data is null.";
    return false;
  }
  return static_cast<AndroidCompositor*>(user_data)->CreateBackingStore(
      config, backing_store_out);
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
  return static_cast<AndroidCompositor*>(user_data)->CollectBackingStore(
      backing_store);
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
