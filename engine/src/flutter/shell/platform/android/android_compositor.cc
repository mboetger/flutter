// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <algorithm>
#include <cmath>

#include "flutter/fml/logging.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/trace_event.h"
#include "flutter/impeller/renderer/backend/vulkan/context_vk.h"

namespace flutter {

namespace {
constexpr uint32_t kGLRGBA8 = 0x8058;
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
          std::make_unique<SurfacePool>(/*use_new_surface_methods=*/true)) {
  if (surface_factory_) {
    android_surface_ = surface_factory_->CreateSurface();
  }
}

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

  if (config == nullptr || backing_store_out == nullptr ||
      config->size.width <= 0 || config->size.height <= 0) {
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
      backing_store_out->open_gl.framebuffer.target = kGLRGBA8;
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

  const bool is_hcpp = IsSurfaceControlEnabled();

  if (layers_count == 0) {
    if (jni_facade_) {
      if (is_hcpp) {
        task_runners_.GetPlatformTaskRunner()->PostTask(fml::MakeCopyable(
            [weak_this = weak_from_this(), jni_facade = jni_facade_,
             views_visible_last_frame = views_visible_last_frame_]() {
              if (auto strong_this = weak_this.lock()) {
                strong_this->HideOverlayLayerIfNeeded();
              }
              for (int64_t id : views_visible_last_frame) {
                jni_facade->hidePlatformView2(id);
              }
              jni_facade->swapTransaction();
              jni_facade->onEndFrame2();
            }));
      } else {
        if (!views_visible_last_frame_.empty()) {
          if (legacy_overlay_created_) {
            legacy_overlay_surface_.reset();
            legacy_overlay_metadata_.reset();
            legacy_overlay_created_ = false;
          }
          task_runners_.GetPlatformTaskRunner()->PostTask(
              fml::MakeCopyable([jni_facade = jni_facade_]() {
                jni_facade->FlutterViewDestroyOverlaySurfaces();
                jni_facade->FlutterViewBeginFrame();
                jni_facade->FlutterViewEndFrame();
              }));
        }
      }
    }
    views_visible_last_frame_.clear();
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
    if (jni_facade_) {
      if (is_hcpp) {
        task_runners_.GetPlatformTaskRunner()->PostTask(fml::MakeCopyable(
            [weak_this = weak_from_this(), jni_facade = jni_facade_,
             views_visible_last_frame = views_visible_last_frame_]() {
              if (auto strong_this = weak_this.lock()) {
                strong_this->HideOverlayLayerIfNeeded();
              }
              for (int64_t id : views_visible_last_frame) {
                jni_facade->hidePlatformView2(id);
              }
              jni_facade->swapTransaction();
              jni_facade->onEndFrame2();
            }));
      } else {
        if (!views_visible_last_frame_.empty()) {
          if (legacy_overlay_created_) {
            legacy_overlay_surface_.reset();
            legacy_overlay_metadata_.reset();
            legacy_overlay_created_ = false;
          }
          task_runners_.GetPlatformTaskRunner()->PostTask(
              fml::MakeCopyable([jni_facade = jni_facade_]() {
                jni_facade->FlutterViewDestroyOverlaySurfaces();
                jni_facade->FlutterViewBeginFrame();
                jni_facade->FlutterViewEndFrame();
              }));
        }
      }
    }
    if (!is_hcpp && android_surface_) {
      android_surface_->PresentOnscreenSurface();
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

  if (jni_facade_) {
    struct DisplayPlatformViewInfo {
      int32_t view_id;
      int32_t x;
      int32_t y;
      int32_t width;
      int32_t height;
      int32_t view_width;
      int32_t view_height;
      AndroidMutatorsStack mutators_stack;
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
        info.view_width = static_cast<int32_t>(std::round(layer->size.width));
        info.view_height = static_cast<int32_t>(std::round(layer->size.height));
        info.mutators_stack = ToAndroidMutatorsStack(layer->platform_view);
        display_infos.push_back(std::move(info));
      }
    }

    if (is_hcpp) {
      task_runners_.GetPlatformTaskRunner()->PostTask(fml::MakeCopyable(
          [weak_this = weak_from_this(), jni_facade = jni_facade_,
           display_infos = std::move(display_infos),
           views_visible_last_frame = views_visible_last_frame_,
           current_views = current_visible_views,
           has_overlay_backing_store]() mutable {
            if (auto strong_this = weak_this.lock()) {
              if (has_overlay_backing_store) {
                strong_this->ShowOverlayLayerIfNeeded();
              } else {
                strong_this->HideOverlayLayerIfNeeded();
              }
            }

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
    } else {
      struct OverlayInfo {
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
      };
      std::vector<OverlayInfo> overlay_infos;
      seen_platform_view = false;
      for (size_t i = 0; i < layers_count; ++i) {
        const FlutterLayer* layer = layers[i];
        if (layer == nullptr) {
          continue;
        }
        if (layer->type == kFlutterLayerContentTypePlatformView) {
          seen_platform_view = true;
        } else if (layer->type == kFlutterLayerContentTypeBackingStore &&
                   seen_platform_view) {
          OverlayInfo o;
          o.x = static_cast<int32_t>(std::round(layer->offset.x));
          o.y = static_cast<int32_t>(std::round(layer->offset.y));
          o.width = static_cast<int32_t>(std::round(layer->size.width));
          o.height = static_cast<int32_t>(std::round(layer->size.height));
          overlay_infos.push_back(o);
        }
      }

      if (!overlay_infos.empty()) {
        DlISize target_size =
            frame_size_.width > 0 && frame_size_.height > 0
                ? frame_size_
                : DlISize(overlay_infos[0].width, overlay_infos[0].height);
        if (target_size.width <= 0 || target_size.height <= 0) {
          target_size = DlISize(1, 1);
        }

        if (!legacy_overlay_created_) {
          fml::AutoResetWaitableEvent latch;
          fml::TaskRunner::RunNowOrPostTask(
              task_runners_.GetPlatformTaskRunner(), [&]() {
                if (jni_facade_) {
                  legacy_overlay_metadata_ =
                      jni_facade_->FlutterViewCreateOverlaySurface();
                }
                latch.Signal();
              });
          latch.Wait();
          legacy_overlay_created_ = true;
          if (legacy_overlay_metadata_ && surface_factory_ &&
              legacy_overlay_metadata_->window) {
            legacy_overlay_surface_ = surface_factory_->CreateSurface();
            if (legacy_overlay_surface_) {
              legacy_overlay_surface_->SetNativeWindow(
                  legacy_overlay_metadata_->window, jni_facade_);
              legacy_overlay_surface_->OnScreenSurfaceResize(target_size);
              legacy_overlay_surface_->SetupImpellerSurface();
              // legacy overlay surface created
            }
          }
        }
        if (android_surface_) {
          android_surface_->OnGLContextMakeCurrent();
        }
      } else if (legacy_overlay_created_) {
        legacy_overlay_surface_.reset();
        legacy_overlay_metadata_.reset();
        legacy_overlay_created_ = false;
      }

      bool has_overlay = legacy_overlay_metadata_ && !overlay_infos.empty();
      int32_t overlay_id =
          legacy_overlay_metadata_ ? legacy_overlay_metadata_->id : 0;

      if (android_surface_) {
        android_surface_->PresentOnscreenSurface();
      }

      task_runners_.GetPlatformTaskRunner()->PostTask(fml::MakeCopyable(
          [weak_this = weak_from_this(), jni_facade = jni_facade_,
           display_infos = std::move(display_infos),
           overlay_infos = std::move(overlay_infos), has_overlay,
           overlay_id]() mutable {
            jni_facade->FlutterViewBeginFrame();
            for (const auto& info : display_infos) {
              jni_facade->FlutterViewOnDisplayPlatformView(
                  info.view_id, info.x, info.y, info.width, info.height,
                  info.view_width, info.view_height, info.mutators_stack);
            }
            if (has_overlay) {
              for (const auto& o : overlay_infos) {
                jni_facade->FlutterViewDisplayOverlaySurface(
                    overlay_id, o.x, o.y, o.width, o.height);
              }
            } else {
              jni_facade->FlutterViewDestroyOverlaySurfaces();
            }
            jni_facade->FlutterViewEndFrame();
          }));
    }
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
  if (user_data == nullptr) {
    return false;
  }
  auto* compositor = static_cast<AndroidCompositor*>(user_data);
  return compositor->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositor::OnCollectBackingStore(
    const FlutterBackingStore* backing_store,
    void* user_data) {
  if (user_data == nullptr) {
    return false;
  }
  auto* compositor = static_cast<AndroidCompositor*>(user_data);
  return compositor->CollectBackingStore(backing_store);
}

bool AndroidCompositor::OnPresentView(
    const FlutterPresentViewInfo* present_info) {
  if (present_info == nullptr || present_info->user_data == nullptr) {
    return false;
  }
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
  if (!android_surface_ && surface_factory_) {
    android_surface_ = surface_factory_->CreateSurface();
  }
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
  if (legacy_overlay_created_) {
    legacy_overlay_surface_.reset();
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        task_runners_.GetPlatformTaskRunner(), [&]() {
          if (jni_facade_) {
            jni_facade_->FlutterViewDestroyOverlaySurfaces();
          }
          legacy_overlay_metadata_.reset();
          legacy_overlay_created_ = false;
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

void AndroidCompositor::SetSurfaceControlEnabledForTesting(
    std::optional<bool> enabled) {
  surface_control_enabled_for_testing_ = enabled;
}

bool AndroidCompositor::IsSurfaceControlEnabled() const {
  if (surface_control_enabled_for_testing_.has_value()) {
    return *surface_control_enabled_for_testing_;
  }
  if (!android_context_) {
    return false;
  }
  if (android_context_->RenderingApi() !=
      AndroidRenderingAPI::kImpellerVulkan) {
    return false;
  }
  auto impeller_context = android_context_->GetImpellerContext();
  if (!impeller_context) {
    return false;
  }
  return impeller::ContextVK::Cast(*impeller_context)
      .GetShouldEnableSurfaceControlSwapchain();
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

AndroidMutatorsStack AndroidCompositor::ToAndroidMutatorsStack(
    const FlutterPlatformView* platform_view) {
  AndroidMutatorsStack stack;
  if (platform_view == nullptr || platform_view->mutations == nullptr) {
    return stack;
  }
  for (size_t i = 0; i < platform_view->mutations_count; ++i) {
    const FlutterPlatformViewMutation* mutation = platform_view->mutations[i];
    if (mutation == nullptr) {
      continue;
    }
    switch (mutation->type) {
      case kFlutterPlatformViewMutationTypeOpacity: {
        double raw_opacity = mutation->opacity;
        if (std::isnan(raw_opacity)) {
          raw_opacity = 1.0;
        }
        double clamped_opacity = std::clamp(raw_opacity, 0.0, 1.0);
        auto alpha = static_cast<uint8_t>(std::round(clamped_opacity * 255.0));
        stack.PushOpacity(alpha);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRect: {
        stack.PushClipRect(ToDlRect(mutation->clip_rect));
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRoundedRect: {
        stack.PushClipRRect(ToDlRoundRect(mutation->clip_rounded_rect));
        break;
      }
      case kFlutterPlatformViewMutationTypeTransformation: {
        stack.PushTransform(ToDlMatrix(mutation->transformation));
        break;
      }
    }
  }
  return stack;
}

DlMatrix AndroidCompositor::ToDlMatrix(
    const FlutterTransformation& transformation) {
  return DlMatrix(static_cast<DlScalar>(transformation.scaleX),
                  static_cast<DlScalar>(transformation.skewY), 0.0f,
                  static_cast<DlScalar>(transformation.pers0),

                  static_cast<DlScalar>(transformation.skewX),
                  static_cast<DlScalar>(transformation.scaleY), 0.0f,
                  static_cast<DlScalar>(transformation.pers1),

                  0.0f, 0.0f, 1.0f, 0.0f,

                  static_cast<DlScalar>(transformation.transX),
                  static_cast<DlScalar>(transformation.transY), 0.0f,
                  static_cast<DlScalar>(transformation.pers2));
}

DlRoundRect AndroidCompositor::ToDlRoundRect(const FlutterRoundedRect& rrect) {
  DlRect rect = DlRect::MakeLTRB(static_cast<DlScalar>(rrect.rect.left),
                                 static_cast<DlScalar>(rrect.rect.top),
                                 static_cast<DlScalar>(rrect.rect.right),
                                 static_cast<DlScalar>(rrect.rect.bottom));
  DlRoundingRadii radii = {
      .top_left =
          DlSize(static_cast<DlScalar>(rrect.upper_left_corner_radius.width),
                 static_cast<DlScalar>(rrect.upper_left_corner_radius.height)),
      .top_right =
          DlSize(static_cast<DlScalar>(rrect.upper_right_corner_radius.width),
                 static_cast<DlScalar>(rrect.upper_right_corner_radius.height)),
      .bottom_left =
          DlSize(static_cast<DlScalar>(rrect.lower_left_corner_radius.width),
                 static_cast<DlScalar>(rrect.lower_left_corner_radius.height)),
      .bottom_right =
          DlSize(static_cast<DlScalar>(rrect.lower_right_corner_radius.width),
                 static_cast<DlScalar>(rrect.lower_right_corner_radius.height)),
  };
  return DlRoundRect::MakeRectRadii(rect, radii);
}

DlRect AndroidCompositor::ToDlRect(const FlutterRect& rect) {
  return DlRect::MakeLTRB(
      static_cast<DlScalar>(rect.left), static_cast<DlScalar>(rect.top),
      static_cast<DlScalar>(rect.right), static_cast<DlScalar>(rect.bottom));
}

}  // namespace flutter
