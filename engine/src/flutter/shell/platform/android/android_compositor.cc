// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <cmath>

#include "flutter/fml/logging.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/trace_event.h"

namespace flutter {

namespace {
constexpr uint32_t kGLFramebuffer = 0x8D40;
}  // namespace

AndroidCompositor::AndroidCompositor(
    std::shared_ptr<AndroidContext> android_context,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    std::shared_ptr<AndroidSurfaceFactory> surface_factory,
    const TaskRunners& task_runners)
    : android_context_(std::move(android_context)),
      jni_facade_(std::move(jni_facade)),
      surface_factory_(std::move(surface_factory)),
      task_runners_(task_runners),
      surface_pool_(
          std::make_unique<SurfacePool>(/*use_new_surface_methods=*/true)) {}

AndroidCompositor::~AndroidCompositor() {
  Teardown();
}

FlutterCompositor AndroidCompositor::GetFlutterCompositor() {
  FlutterCompositor compositor = {};
  compositor.struct_size = sizeof(FlutterCompositor);
  compositor.user_data = this;
  compositor.create_backing_store_callback = &OnCreateBackingStore;
  compositor.collect_backing_store_callback = &OnCollectBackingStore;
  compositor.present_view_callback = &OnPresentView;
  compositor.avoid_backing_store_cache = false;
  return compositor;
}

bool AndroidCompositor::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  TRACE_EVENT0("flutter", "AndroidCompositor::CreateBackingStore");

  if (config == nullptr || backing_store_out == nullptr) {
    return false;
  }

  backing_store_out->struct_size = sizeof(FlutterBackingStore);
  backing_store_out->user_data = nullptr;
  backing_store_out->did_update = false;

  AndroidRenderingAPI rendering_api = AndroidRenderingAPI::kImpellerOpenGLES;
  if (android_context_) {
    rendering_api = android_context_->RenderingApi();
  }

  switch (rendering_api) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware: {
      size_t bytes = static_cast<size_t>(config->size.width) *
                     static_cast<size_t>(config->size.height) * 4;
      void* allocation = std::calloc(bytes, sizeof(uint8_t));
      if (!allocation) {
        return false;
      }
      backing_store_out->type = kFlutterBackingStoreTypeSoftware;
      backing_store_out->software.allocation = allocation;
      backing_store_out->software.height =
          static_cast<size_t>(config->size.height);
      backing_store_out->software.row_bytes =
          static_cast<size_t>(config->size.width) * 4;
      backing_store_out->software.user_data = nullptr;
      backing_store_out->software.destruction_callback = [](void*) {};
      return true;
    }
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerVulkan: {
      backing_store_out->type = kFlutterBackingStoreTypeVulkan;
      backing_store_out->vulkan.struct_size = sizeof(FlutterVulkanBackingStore);
      backing_store_out->vulkan.image = nullptr;
      backing_store_out->vulkan.user_data = nullptr;
      backing_store_out->vulkan.destruction_callback = [](void*) {};
      return true;
    }
#if !SLIMPELLER
    case AndroidRenderingAPI::kSkiaOpenGLES:
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
    case AndroidRenderingAPI::kImpellerAutoselect:
    default: {
      backing_store_out->type = kFlutterBackingStoreTypeOpenGL;
      backing_store_out->open_gl.type = kFlutterOpenGLTargetTypeFramebuffer;
      backing_store_out->open_gl.framebuffer.target = kGLFramebuffer;
      backing_store_out->open_gl.framebuffer.name = 0;
      backing_store_out->open_gl.framebuffer.user_data = nullptr;
      backing_store_out->open_gl.framebuffer.destruction_callback = [](void*) {
      };
      return true;
    }
  }
}

bool AndroidCompositor::CollectBackingStore(
    const FlutterBackingStore* backing_store) {
  TRACE_EVENT0("flutter", "AndroidCompositor::CollectBackingStore");

  if (backing_store == nullptr) {
    return false;
  }

  if (backing_store->type == kFlutterBackingStoreTypeSoftware) {
    if (backing_store->software.allocation != nullptr) {
      std::free(const_cast<void*>(backing_store->software.allocation));
    }
  }
  return true;
}

bool AndroidCompositor::PresentView(
    const FlutterPresentViewInfo* present_info) {
  if (present_info == nullptr ||
      present_info->struct_size != sizeof(FlutterPresentViewInfo)) {
    return false;
  }
  return Present(present_info->view_id, present_info->layers,
                 present_info->layers_count);
}

bool AndroidCompositor::Present(FlutterViewId view_id,
                                const FlutterLayer** layers,
                                size_t layers_count) {
  TRACE_EVENT0("flutter", "AndroidCompositor::Present");

  if (layers_count == 0) {
    return true;
  }

  if (layers == nullptr) {
    FML_LOG(ERROR)
        << "AndroidCompositor::Present received null layers with count "
        << layers_count;
    return false;
  }

  bool has_platform_views = false;
  for (size_t i = 0; i < layers_count; ++i) {
    if (layers[i] != nullptr &&
        layers[i]->type == kFlutterLayerContentTypePlatformView) {
      has_platform_views = true;
      break;
    }
  }

  if (!has_platform_views) {
    HideOverlayLayerIfNeeded();

    if (jni_facade_) {
      task_runners_.GetPlatformTaskRunner()->PostTask(fml::MakeCopyable(
          [jni_facade = jni_facade_,
           views_visible_last_frame = views_visible_last_frame_]() {
            for (int64_t id : views_visible_last_frame) {
              jni_facade->hidePlatformView2(id);
            }
            jni_facade->swapTransaction();
            jni_facade->onEndFrame2();
          }));
    }
    views_visible_last_frame_.clear();
    return true;
  }

  std::vector<int64_t> current_visible_views;
  bool has_overlay_backing_store = false;
  bool seen_platform_view = false;

  for (size_t i = 0; i < layers_count; ++i) {
    const FlutterLayer* layer = layers[i];
    if (layer == nullptr) {
      continue;
    }
    if (layer->type == kFlutterLayerContentTypePlatformView) {
      seen_platform_view = true;
      if (layer->platform_view != nullptr) {
        current_visible_views.push_back(layer->platform_view->identifier);
      }
    } else if (layer->type == kFlutterLayerContentTypeBackingStore &&
               seen_platform_view) {
      has_overlay_backing_store = true;
    }
  }

  if (has_overlay_backing_store) {
    ShowOverlayLayerIfNeeded();
  } else {
    HideOverlayLayerIfNeeded();
  }

  if (jni_facade_) {
    struct DisplayPlatformViewInfo {
      int32_t view_id;
      int32_t x;
      int32_t y;
      int32_t width;
      int32_t height;
      int32_t view_width;
      int32_t view_height;
      MutatorsStack mutators_stack;
    };

    std::vector<DisplayPlatformViewInfo> display_infos;
    for (size_t i = 0; i < layers_count; ++i) {
      const FlutterLayer* layer = layers[i];
      if (layer != nullptr &&
          layer->type == kFlutterLayerContentTypePlatformView &&
          layer->platform_view != nullptr) {
        DisplayPlatformViewInfo info;
        info.view_id = static_cast<int32_t>(layer->platform_view->identifier);
        info.x = static_cast<int32_t>(std::round(layer->offset.x));
        info.y = static_cast<int32_t>(std::round(layer->offset.y));
        info.width = static_cast<int32_t>(std::round(layer->size.width));
        info.height = static_cast<int32_t>(std::round(layer->size.height));
        info.view_width = static_cast<int32_t>(
            std::round(layer->size.width * device_pixel_ratio_));
        info.view_height = static_cast<int32_t>(
            std::round(layer->size.height * device_pixel_ratio_));
        display_infos.push_back(std::move(info));
      }
    }

    task_runners_.GetPlatformTaskRunner()->PostTask(fml::MakeCopyable(
        [jni_facade = jni_facade_, display_infos = std::move(display_infos),
         views_visible_last_frame = views_visible_last_frame_,
         current_views = current_visible_views]() mutable {
          absl::flat_hash_set<int64_t> current_set(current_views.begin(),
                                                   current_views.end());

          for (const auto& info : display_infos) {
            jni_facade->onDisplayPlatformView2(
                info.view_id, info.x, info.y, info.width, info.height,
                info.view_width, info.view_height, info.mutators_stack);
          }

          for (int64_t old_view_id : views_visible_last_frame) {
            if (!current_set.contains(old_view_id)) {
              jni_facade->hidePlatformView2(old_view_id);
            }
          }

          jni_facade->swapTransaction();
          jni_facade->onEndFrame2();
        }));
  }

  views_visible_last_frame_.clear();
  views_visible_last_frame_.insert(current_visible_views.begin(),
                                   current_visible_views.end());
  return true;
}

bool AndroidCompositor::OnCreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  FML_DCHECK(user_data != nullptr);
  auto* compositor = static_cast<AndroidCompositor*>(user_data);
  return compositor->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositor::OnCollectBackingStore(
    const FlutterBackingStore* backing_store,
    void* user_data) {
  FML_DCHECK(user_data != nullptr);
  auto* compositor = static_cast<AndroidCompositor*>(user_data);
  return compositor->CollectBackingStore(backing_store);
}

bool AndroidCompositor::OnPresentView(
    const FlutterPresentViewInfo* present_info) {
  FML_DCHECK(present_info != nullptr);
  FML_DCHECK(present_info->user_data != nullptr);
  auto* compositor = static_cast<AndroidCompositor*>(present_info->user_data);
  return compositor->PresentView(present_info);
}

void AndroidCompositor::SetAndroidSurface(
    std::unique_ptr<AndroidSurface> surface) {
  android_surface_ = std::move(surface);
}

AndroidSurface* AndroidCompositor::GetAndroidSurface() const {
  return android_surface_.get();
}

void AndroidCompositor::SetSurfaceFactory(
    std::shared_ptr<AndroidSurfaceFactory> surface_factory) {
  surface_factory_ = std::move(surface_factory);
}

std::shared_ptr<AndroidSurfaceFactory> AndroidCompositor::GetSurfaceFactory()
    const {
  return surface_factory_;
}

bool AndroidCompositor::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> window) {
  if (android_surface_) {
    return android_surface_->SetNativeWindow(std::move(window), jni_facade_);
  }
  return false;
}

bool AndroidCompositor::OnScreenSurfaceResize(const DlISize& size) {
  frame_size_ = size;
  if (surface_pool_) {
    surface_pool_->SetFrameSize(size);
  }
  if (jni_facade_) {
    task_runners_.GetPlatformTaskRunner()->PostTask(
        fml::MakeCopyable([jni_facade = jni_facade_, size]() {
          jni_facade->MaybeResizeSurfaceView(size.width, size.height);
        }));
  }
  if (android_surface_) {
    return android_surface_->OnScreenSurfaceResize(size);
  }
  return true;
}

void AndroidCompositor::Teardown() {
  DestroySurfaces();
  if (android_surface_) {
    android_surface_->TeardownOnScreenContext();
  }
}

void AndroidCompositor::DestroySurfaces() {
  if (surface_pool_ && surface_pool_->HasLayers()) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        task_runners_.GetPlatformTaskRunner(), [&]() {
          if (surface_pool_) {
            surface_pool_->DestroyLayers(jni_facade_);
          }
          latch.Signal();
        });
    latch.Wait();
  }
  overlay_layer_is_shown_.store(false);
}

void AndroidCompositor::ShowOverlayLayerIfNeeded() {
  if (!overlay_layer_is_shown_.load()) {
    if (jni_facade_) {
      jni_facade_->showOverlaySurface2();
    }
    overlay_layer_is_shown_.store(true);
  }
}

void AndroidCompositor::HideOverlayLayerIfNeeded() {
  if (overlay_layer_is_shown_.load()) {
    if (jni_facade_) {
      jni_facade_->hideOverlaySurface2();
    }
    overlay_layer_is_shown_.store(false);
  }
}

bool AndroidCompositor::IsOverlayLayerShown() const {
  return overlay_layer_is_shown_.load();
}

SurfacePool* AndroidCompositor::GetSurfacePool() const {
  return surface_pool_.get();
}

const std::shared_ptr<AndroidContext>& AndroidCompositor::GetAndroidContext()
    const {
  return android_context_;
}

const std::shared_ptr<PlatformViewAndroidJNI>& AndroidCompositor::GetJniFacade()
    const {
  return jni_facade_;
}

const TaskRunners& AndroidCompositor::GetTaskRunners() const {
  return task_runners_;
}

void AndroidCompositor::SetDevicePixelRatio(double device_pixel_ratio) {
  device_pixel_ratio_ = device_pixel_ratio;
}

double AndroidCompositor::GetDevicePixelRatio() const {
  return device_pixel_ratio_;
}

}  // namespace flutter
