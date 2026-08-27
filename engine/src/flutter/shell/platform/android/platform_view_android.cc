// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android.h"

#include <android/api-level.h>
#include <android/log.h>
#include <sys/system_properties.h>
#include <memory>
#include <utility>

#include "common/settings.h"
#include "flutter/common/graphics/texture.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/shell/platform/android/android_context_dynamic_impeller.h"
#include "flutter/shell/platform/android/android_context_gl_impeller.h"
#include "flutter/shell/platform/android/android_context_vk_impeller.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/image_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_vk_impeller.h"
#include "flutter/shell/platform/embedder/vsync_waiter_embedder.h"
#include "impeller/toolkit/android/choreographer.h"

#if !SLIMPELLER
#include "flutter/shell/platform/android/android_context_gl_skia.h"
#include "flutter/shell/platform/android/image_external_texture_gl_skia.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_skia.h"
#endif  // !SLIMPELLER

#if IMPELLER_ENABLE_VULKAN
#include "flutter/shell/platform/android/image_external_texture_vk_impeller.h"
#endif
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_message_response_android.h"

namespace flutter {

namespace {

// Android API 34 (UpsideDownCake) is required for HardwareBuffer based Platform
// Views (HCPP).
static constexpr int kMinAPILevelHCPP = 34;
static constexpr int64_t kImplicitViewId = 0;

}  // namespace

AndroidContext::ContextSettings PlatformViewAndroid::CreateContextSettings(
    const Settings& p_settings) {
  AndroidContext::ContextSettings settings;
  settings.enable_gpu_tracing = p_settings.enable_vulkan_gpu_tracing;
  settings.enable_validation = p_settings.enable_vulkan_validation;
  settings.enable_surface_control = p_settings.enable_surface_control;
  return settings;
}

std::shared_ptr<flutter::AndroidContext>
PlatformViewAndroid::CreateAndroidContext(
    const flutter::TaskRunners& task_runners,
    AndroidRenderingAPI android_rendering_api,
    bool enable_opengl_gpu_tracing,
    const AndroidContext::ContextSettings& settings,
    std::shared_ptr<fml::BasicTaskRunner> io_task_runner) {
  switch (android_rendering_api) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      return std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);
    case AndroidRenderingAPI::kSkiaOpenGLES:
      return std::make_unique<AndroidContextGLSkia>(
          fml::MakeRefCounted<AndroidEnvironmentGL>(),  //
          task_runners                                  //
      );
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerVulkan:
      return std::make_unique<AndroidContextVKImpeller>(settings);
    case AndroidRenderingAPI::kImpellerOpenGLES:
      return std::make_unique<AndroidContextGLImpeller>(
          std::make_unique<impeller::egl::Display>(), enable_opengl_gpu_tracing,
          std::move(io_task_runner));
    case AndroidRenderingAPI::kImpellerAutoselect:
      // Determine if we're using GL or Vulkan.
      return std::make_unique<AndroidContextDynamicImpeller>(
          settings, std::move(io_task_runner));
  }
  FML_UNREACHABLE();
}

PlatformViewAndroid::PlatformViewAndroid(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<flutter::AndroidContext>& android_context,
    EmbedderSurfaceAndroid* embedder_surface)
    : delegate_(delegate),
      task_runners_(task_runners),
      jni_facade_(jni_facade),
      android_context_(android_context),
      embedder_surface_(embedder_surface),
      platform_view_android_delegate_(jni_facade),
      platform_message_handler_(new PlatformMessageHandlerAndroid(jni_facade)),
      weak_factory_(this) {
  if (android_context_) {
    FML_CHECK(android_context_->IsValid())
        << "Could not create surface from invalid Android context.";
    android_meets_hcpp_criteria_ =
        delegate.OnPlatformViewGetSettings().enable_surface_control &&
        android_get_device_api_level() >= kMinAPILevelHCPP &&
        delegate.OnPlatformViewGetSettings().enable_impeller;
  }
}

PlatformViewAndroid::~PlatformViewAndroid() = default;

void PlatformViewAndroid::NotifyCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (embedder_surface_) {
    InstallFirstFrameCallback();

    if (auto raster_runner = task_runners_.GetRasterTaskRunner()) {
      fml::AutoResetWaitableEvent latch;
      fml::TaskRunner::RunNowOrPostTask(
          raster_runner, [&latch, embedder_surface = embedder_surface_,
                          native_window = std::move(native_window),
                          jni_facade = jni_facade_]() {
            embedder_surface->NotifyCreated(native_window, jni_facade);
            latch.Signal();
          });
      latch.Wait();
    } else {
      embedder_surface_->NotifyCreated(native_window, jni_facade_);
    }
  }

  if (platform_view_) {
    platform_view_->NotifyCreated();
  }
}

void PlatformViewAndroid::NotifySurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (embedder_surface_) {
    if (auto raster_runner = task_runners_.GetRasterTaskRunner()) {
      fml::AutoResetWaitableEvent latch;
      fml::TaskRunner::RunNowOrPostTask(
          raster_runner, [&latch, embedder_surface = embedder_surface_,
                          native_window = std::move(native_window),
                          jni_facade = jni_facade_]() {
            embedder_surface->NotifySurfaceWindowChanged(native_window,
                                                         jni_facade);
            latch.Signal();
          });
      latch.Wait();
    } else {
      embedder_surface_->NotifySurfaceWindowChanged(native_window, jni_facade_);
    }
  }

  if (platform_view_) {
    platform_view_->ScheduleFrame();
  }
}

void PlatformViewAndroid::NotifyDestroyed() {
  if (platform_view_) {
    platform_view_->NotifyDestroyed();
  }

  if (embedder_surface_) {
    if (auto raster_runner = task_runners_.GetRasterTaskRunner()) {
      fml::AutoResetWaitableEvent latch;
      fml::TaskRunner::RunNowOrPostTask(
          raster_runner, [&latch, embedder_surface = embedder_surface_]() {
            embedder_surface->NotifyDestroyed();
            latch.Signal();
          });
      latch.Wait();
    } else {
      embedder_surface_->NotifyDestroyed();
    }
  }
}

void PlatformViewAndroid::NotifyChanged(const DlISize& size) {
  if (!embedder_surface_) {
    return;
  }
  if (auto raster_runner = task_runners_.GetRasterTaskRunner()) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        raster_runner, [&latch, embedder_surface = embedder_surface_, size]() {
          embedder_surface->NotifyChanged(size);
          latch.Signal();
        });
    latch.Wait();
  } else {
    embedder_surface_->NotifyChanged(size);
  }
}

void PlatformViewAndroid::DispatchPlatformMessage(JNIEnv* env,
                                                  std::string name,
                                                  jobject java_message_data,
                                                  jint java_message_position,
                                                  jint response_id) {
  uint8_t* message_data =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(java_message_data));
  fml::MallocMapping message =
      fml::MallocMapping::Copy(message_data, java_message_position);

  fml::RefPtr<flutter::PlatformMessageResponse> response;
  if (response_id) {
    response = fml::MakeRefCounted<PlatformMessageResponseAndroid>(
        response_id, jni_facade_, task_runners_.GetPlatformTaskRunner());
  }

  if (platform_view_) {
    platform_view_->DispatchPlatformMessage(
        std::make_unique<flutter::PlatformMessage>(
            std::move(name), std::move(message), std::move(response)));
  }
}

void PlatformViewAndroid::DispatchEmptyPlatformMessage(JNIEnv* env,
                                                       std::string name,
                                                       jint response_id) {
  fml::RefPtr<flutter::PlatformMessageResponse> response;
  if (response_id) {
    response = fml::MakeRefCounted<PlatformMessageResponseAndroid>(
        response_id, jni_facade_, task_runners_.GetPlatformTaskRunner());
  }

  if (platform_view_) {
    platform_view_->DispatchPlatformMessage(
        std::make_unique<flutter::PlatformMessage>(std::move(name),
                                                   std::move(response)));
  }
}

void PlatformViewAndroid::HandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  // Called from the ui thread.
  platform_message_handler_->HandlePlatformMessage(std::move(message));
}

void PlatformViewAndroid::OnPreEngineRestart() const {
  jni_facade_->FlutterViewOnPreEngineRestart();
}

void PlatformViewAndroid::DispatchSemanticsAction(JNIEnv* env,
                                                  jint node_id,
                                                  jint action,
                                                  jobject args,
                                                  jint args_position) {
  // TODO(team-android): Remove implicit view assumption.
  // https://github.com/flutter/flutter/issues/142845
  if (!platform_view_) {
    return;
  }
  if (env->IsSameObject(args, NULL)) {
    platform_view_->DispatchSemanticsAction(
        kImplicitViewId, node_id, static_cast<flutter::SemanticsAction>(action),
        fml::MallocMapping());
    return;
  }

  uint8_t* args_data = static_cast<uint8_t*>(env->GetDirectBufferAddress(args));
  auto args_vector = fml::MallocMapping::Copy(args_data, args_position);

  platform_view_->DispatchSemanticsAction(
      kImplicitViewId, node_id, static_cast<flutter::SemanticsAction>(action),
      std::move(args_vector));
}

void PlatformViewAndroid::UpdateSemantics(
    int64_t view_id,
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  platform_view_android_delegate_.UpdateSemantics(update, actions);
}

void PlatformViewAndroid::SetApplicationLocale(std::string locale) {
  jni_facade_->FlutterViewSetApplicationLocale(std::move(locale));
}

void PlatformViewAndroid::SetSemanticsTreeEnabled(bool enabled) {
  jni_facade_->FlutterViewSetSemanticsTreeEnabled(enabled);
}

void PlatformViewAndroid::RegisterExternalTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) {
  if (android_context_->RenderingApi() ==
      AndroidRenderingAPI::kImpellerAutoselect) {
    SetupImpellerContext();
  }
  std::shared_ptr<Texture> texture;
  switch (android_context_->RenderingApi()) {
    case AndroidRenderingAPI::kImpellerOpenGLES:
      // Impeller GLES.
      texture = std::make_shared<SurfaceTextureExternalTextureGLImpeller>(
          std::static_pointer_cast<impeller::ContextGLES>(
              GetImpellerContext()),  //
          texture_id,                 //
          surface_texture,            //
          jni_facade_                 //
      );
      break;
#if !SLIMPELLER
    case AndroidRenderingAPI::kSkiaOpenGLES:
      // Legacy GL.
      texture = std::make_shared<SurfaceTextureExternalTextureGLSkia>(
          texture_id,       //
          surface_texture,  //
          jni_facade_       //
      );
      break;
    case AndroidRenderingAPI::kSoftware:
      FML_LOG(INFO) << "Software rendering does not support external textures.";
      break;
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerVulkan:
      FML_LOG(IMPORTANT)
          << "Flutter recommends migrating plugins that create and "
             "register surface textures to the new surface producer "
             "API. See https://docs.flutter.dev/release/breaking-changes/"
             "android-surface-plugins";
      texture = std::make_shared<SurfaceTextureExternalTextureVKImpeller>(
          std::static_pointer_cast<impeller::ContextVK>(
              GetImpellerContext()),  //
          texture_id,                 //
          surface_texture,            //
          jni_facade_                 //
      );
      break;
    case AndroidRenderingAPI::kImpellerAutoselect:
    default:
      FML_CHECK(false);
      break;
  }
  if (texture) {
    delegate_.OnPlatformViewRegisterTexture(std::move(texture));
  }
}

void PlatformViewAndroid::RegisterImageTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    ImageExternalTexture::ImageLifecycle lifecycle) {
  if (android_context_->RenderingApi() ==
      AndroidRenderingAPI::kImpellerAutoselect) {
    SetupImpellerContext();
  }
  std::shared_ptr<Texture> texture;
  switch (android_context_->RenderingApi()) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSkiaOpenGLES:
      // Legacy GL.
      texture = std::make_shared<ImageExternalTextureGLSkia>(
          std::static_pointer_cast<AndroidContextGLSkia>(android_context_),
          texture_id, image_texture_entry, jni_facade_, lifecycle);
      break;
    case AndroidRenderingAPI::kSoftware:
      FML_LOG(INFO) << "Software rendering does not support external textures.";
      break;
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
      // Impeller GLES.
      texture = std::make_shared<ImageExternalTextureGLImpeller>(
          std::static_pointer_cast<impeller::ContextGLES>(GetImpellerContext()),
          texture_id, image_texture_entry, jni_facade_, lifecycle);
      break;
    case AndroidRenderingAPI::kImpellerVulkan:
      texture = std::make_shared<ImageExternalTextureVKImpeller>(
          std::static_pointer_cast<impeller::ContextVK>(GetImpellerContext()),
          texture_id, image_texture_entry, jni_facade_, lifecycle);
      break;
    case AndroidRenderingAPI::kImpellerAutoselect:
      FML_CHECK(false);
      break;
  }
  if (texture) {
    delegate_.OnPlatformViewRegisterTexture(std::move(texture));
  }
}

void PlatformViewAndroid::OnVsyncCallback(intptr_t baton) {
  const static bool use_choreographer =
      impeller::android::Choreographer::IsAvailableOnPlatform();
  if (use_choreographer) {
    auto post_to_choreographer = [baton, task_runners = task_runners_]() {
      const auto& choreographer =
          impeller::android::Choreographer::GetInstance();
      choreographer.PostFrameCallback([baton, task_runners](auto time) {
        auto time_ns =
            std::chrono::time_point_cast<std::chrono::nanoseconds>(time)
                .time_since_epoch()
                .count();
        auto frame_time = fml::TimePoint::FromEpochDelta(
            fml::TimeDelta::FromNanoseconds(time_ns));
        auto now = fml::TimePoint::Now();
        if (frame_time > now) {
          frame_time = now;
        }
        // Assume 60 FPS standard display interval: 1,000,000,000 ns / 60
        // = 16.66ms
        // TODO(team-android): Get the actual refresh rate from the display.
        // https://github.com/flutter/flutter/issues/142845
        constexpr double kStandardRefreshRateHz = 60.0;
        constexpr double kNanosPerSecond = 1000000000.0;
        auto target_time =
            frame_time + fml::TimeDelta::FromNanoseconds(
                             kNanosPerSecond / kStandardRefreshRateHz);
        if (task_runners.GetUITaskRunner()) {
          VsyncWaiterEmbedder::OnEmbedderVsync(task_runners, baton, frame_time,
                                               target_time);
        }
      });
    };
    if (auto ui_runner = task_runners_.GetUITaskRunner()) {
      fml::TaskRunner::RunNowOrPostTask(ui_runner,
                                        std::move(post_to_choreographer));
    } else {
      post_to_choreographer();
    }
  } else {
    // TODO(99798): Remove it when we drop support for API level < 29 and 32-bit
    // devices.
    if (auto platform_runner = task_runners_.GetPlatformTaskRunner()) {
      platform_runner->PostTask([task_runners = task_runners_]() {
        FML_LOG(ERROR) << "Java-based Vsync is not yet implemented in the "
                          "new Android embedder.";
      });
    } else {
      FML_LOG(ERROR) << "Java-based Vsync is not yet implemented in the "
                        "new Android embedder.";
    }
  }
}

void PlatformViewAndroid::SendChannelUpdate(const std::string& name,
                                            bool listening) {}

void PlatformViewAndroid::RequestViewFocusChange(
    const ViewFocusChangeRequest& request) {}

std::unique_ptr<std::vector<std::string>>
PlatformViewAndroid::ComputePlatformResolvedLocales(
    const std::vector<std::string>& supported_locale_data) {
  return jni_facade_->FlutterViewComputePlatformResolvedLocale(
      supported_locale_data);
}

void PlatformViewAndroid::RequestDartDeferredLibrary(intptr_t loading_unit_id) {
  if (jni_facade_->RequestDartDeferredLibrary(loading_unit_id)) {
    return;
  }
  return;  // TODO(garyq): Call LoadDartDeferredLibraryFailure()
}

void PlatformViewAndroid::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  delegate_.LoadDartDeferredLibrary(loading_unit_id, std::move(snapshot_data),
                                    std::move(snapshot_instructions));
}

void PlatformViewAndroid::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  delegate_.LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                         transient);
}

void PlatformViewAndroid::UpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  delegate_.UpdateAssetResolverByType(std::move(updated_asset_resolver), type);
}

void PlatformViewAndroid::InstallFirstFrameCallback() {
  // On Platform Task Runner.
  delegate_.OnPlatformViewSetNextFrameCallback(
      [platform_view = GetWeakPtr(),
       platform_task_runner = task_runners_.GetPlatformTaskRunner()]() {
        // On GPU Task Runner.
        if (platform_task_runner) {
          platform_task_runner->PostTask([platform_view]() {
            // Back on Platform Task Runner.
            if (platform_view) {
              platform_view->FireFirstFrameCallback();
            }
          });
        }
      });
}

void PlatformViewAndroid::FireFirstFrameCallback() {
  jni_facade_->FlutterViewOnFirstFrame();
}

double PlatformViewAndroid::GetScaledFontSize(double unscaled_font_size,
                                              int configuration_id) const {
  return jni_facade_->FlutterViewGetScaledFontSize(unscaled_font_size,
                                                   configuration_id);
}

bool PlatformViewAndroid::IsSurfaceControlEnabled() const {
  // This needs to know if we're actually using HCPP.
  return android_meets_hcpp_criteria_ &&
         android_context_->RenderingApi() ==
             AndroidRenderingAPI::kImpellerVulkan &&
         impeller::ContextVK::Cast(*android_context_->GetImpellerContext())
             .GetShouldEnableSurfaceControlSwapchain();
}

void PlatformViewAndroid::SetupImpellerContext() {
  if (embedder_surface_) {
    embedder_surface_->SetupImpellerContext();
  } else if (android_context_) {
    android_context_->SetupImpellerContext();
  }
}

std::shared_ptr<impeller::Context> PlatformViewAndroid::GetImpellerContext()
    const {
  if (platform_view_) {
    return platform_view_->GetImpellerContext();
  }
  if (embedder_surface_) {
    return embedder_surface_->CreateImpellerContext();
  }
  if (android_context_) {
    return android_context_->GetImpellerContext();
  }
  return nullptr;
}

fml::WeakPtr<PlatformViewAndroid> PlatformViewAndroid::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

void PlatformViewAndroid::SetPlatformView(
    fml::WeakPtr<PlatformView> platform_view) {
  platform_view_ = platform_view;
}

void PlatformViewAndroid::SetSemanticsEnabled(bool enabled) {
  if (platform_view_) {
    platform_view_->SetSemanticsEnabled(enabled);
  }
}

void PlatformViewAndroid::SetAccessibilityFeatures(int32_t flags) {
  if (platform_view_) {
    platform_view_->SetAccessibilityFeatures(flags);
  }
}

void PlatformViewAndroid::SetViewportMetrics(int64_t view_id,
                                             const ViewportMetrics& metrics) {
  if (platform_view_) {
    platform_view_->SetViewportMetrics(view_id, metrics);
  }
}

void PlatformViewAndroid::DispatchPointerDataPacket(
    std::unique_ptr<PointerDataPacket> packet) {
  if (platform_view_) {
    platform_view_->DispatchPointerDataPacket(std::move(packet));
  }
}

void PlatformViewAndroid::UnregisterTexture(int64_t texture_id) {
  if (platform_view_) {
    platform_view_->UnregisterTexture(texture_id);
  }
}

void PlatformViewAndroid::MarkTextureFrameAvailable(int64_t texture_id) {
  if (platform_view_) {
    platform_view_->MarkTextureFrameAvailable(texture_id);
    platform_view_->ScheduleFrame();
  }
}

void PlatformViewAndroid::ScheduleFrame() {
  if (platform_view_) {
    platform_view_->ScheduleFrame();
  }
}

}  // namespace flutter
