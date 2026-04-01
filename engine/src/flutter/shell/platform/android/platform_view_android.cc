// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android.h"

#include <android/api-level.h>
#include <sys/system_properties.h>
#include <memory>
#include <utility>

#include "common/settings.h"
#include "flutter/common/graphics/texture.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/shell/common/shell_io_manager.h"
#include "flutter/shell/gpu/gpu_surface_gl_delegate.h"
#include "flutter/shell/platform/android/android_context_dynamic_impeller.h"
#include "flutter/shell/platform/android/android_context_gl_impeller.h"
#include "flutter/shell/platform/android/android_context_vk_impeller.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_surface_dynamic_impeller.h"
#include "flutter/shell/platform/android/android_surface_gl_impeller.h"
#include "flutter/shell/platform/android/image_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_vk_impeller.h"

#if !SLIMPELLER
#include "flutter/shell/platform/android/android_context_gl_skia.h"
#include "flutter/shell/platform/android/android_surface_gl_skia.h"
#include "flutter/shell/platform/android/android_surface_software.h"
#include "flutter/shell/platform/android/image_external_texture_gl_skia.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_skia.h"
#endif  // !SLIMPELLER

#include "fml/logging.h"
#include "impeller/display_list/aiks_context.h"
#if IMPELLER_ENABLE_VULKAN  // b/258506856 for why this is behind an if
#include "flutter/shell/platform/android/android_surface_vk_impeller.h"
#include "flutter/shell/platform/android/image_external_texture_vk_impeller.h"
#endif
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder_wrapper.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_message_response_android.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/shell/platform/android/surface/snapshot_surface_producer.h"
#include "flutter/shell/platform/android/vsync_waiter_android.h"
#include "flutter/shell/platform/embedder/vsync_waiter_embedder.h"
#include "impeller/toolkit/android/choreographer.h"

namespace flutter {

namespace {

static constexpr int kMinAPILevelHCPP = 34;
static constexpr int64_t kImplicitViewId = 0;

}  // namespace

AndroidContext::ContextSettings PlatformViewAndroid::CreateContextSettings(
    const Settings& p_settings) {
  AndroidContext::ContextSettings settings;
  settings.enable_gpu_tracing = p_settings.enable_vulkan_gpu_tracing;
  settings.enable_validation = p_settings.enable_vulkan_validation;
  settings.enable_surface_control = p_settings.enable_surface_control;
  settings.impeller_flags.antialiased_lines =
      p_settings.impeller_antialiased_lines;
  return settings;
}

std::shared_ptr<flutter::AndroidContext>
PlatformViewAndroid::CreateAndroidContext(
    const flutter::TaskRunners& task_runners,
    AndroidRenderingAPI android_rendering_api,
    bool enable_opengl_gpu_tracing,
    const AndroidContext::ContextSettings& settings) {
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
          std::make_unique<impeller::egl::Display>(),
          enable_opengl_gpu_tracing);
    case AndroidRenderingAPI::kImpellerAutoselect:
      // Determine if we're using GL or Vulkan.
      return std::make_unique<AndroidContextDynamicImpeller>(settings);
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
        delegate_.OnPlatformViewGetSettings().enable_surface_control &&
        android_get_device_api_level() >= kMinAPILevelHCPP &&
        delegate_.OnPlatformViewGetSettings().enable_impeller;
  }
}

PlatformViewAndroid::~PlatformViewAndroid() = default;

void PlatformViewAndroid::NotifyCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (embedder_surface_) {
    InstallFirstFrameCallback();

    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        task_runners_.GetRasterTaskRunner(),
        [&latch, surface = embedder_surface_,
         native_window = std::move(native_window), jni_facade = jni_facade_]() {
          surface->NotifyCreated(native_window, jni_facade);
          latch.Signal();
        });
    latch.Wait();
  }

  std::unique_ptr<Surface> surface;
  fml::AutoResetWaitableEvent latch;
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetRasterTaskRunner(), [this, &surface, &latch]() {
        if (embedder_surface_) {
          surface = embedder_surface_->CreateGPUSurface();
        }
        if (surface && !surface->IsValid()) {
          surface.reset();
        }
        latch.Signal();
      });
  latch.Wait();
  if (!surface) {
    FML_LOG(ERROR) << "Failed to create platform view rendering surface";
    return;
  }
  delegate_.OnPlatformViewCreated(std::move(surface));
}

void PlatformViewAndroid::NotifySurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (embedder_surface_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        task_runners_.GetRasterTaskRunner(),
        [&latch, surface = embedder_surface_,
         native_window = std::move(native_window), jni_facade = jni_facade_]() {
          surface->NotifySurfaceWindowChanged(native_window, jni_facade);
          latch.Signal();
        });
    latch.Wait();
  }

  delegate_.OnPlatformViewScheduleFrame();
}

void PlatformViewAndroid::NotifyDestroyed() {
  delegate_.OnPlatformViewDestroyed();

  if (embedder_surface_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(task_runners_.GetRasterTaskRunner(),
                                      [&latch, surface = embedder_surface_]() {
                                        surface->TeardownOnScreenContext();
                                        latch.Signal();
                                      });
    latch.Wait();
  }
}

void PlatformViewAndroid::NotifyChanged(const DlISize& size) {
  if (!embedder_surface_) {
    return;
  }
  fml::AutoResetWaitableEvent latch;
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetRasterTaskRunner(),  //
      [&latch, surface = embedder_surface_, size]() {
        surface->NotifyChanged(size);
        latch.Signal();
      });
  latch.Wait();
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

  delegate_.OnPlatformViewDispatchPlatformMessage(
      std::make_unique<flutter::PlatformMessage>(
          std::move(name), std::move(message), std::move(response)));
}

void PlatformViewAndroid::DispatchEmptyPlatformMessage(JNIEnv* env,
                                                       std::string name,
                                                       jint response_id) {
  fml::RefPtr<flutter::PlatformMessageResponse> response;
  if (response_id) {
    response = fml::MakeRefCounted<PlatformMessageResponseAndroid>(
        response_id, jni_facade_, task_runners_.GetPlatformTaskRunner());
  }

  delegate_.OnPlatformViewDispatchPlatformMessage(
      std::make_unique<flutter::PlatformMessage>(std::move(name),
                                                 std::move(response)));
}

// |PlatformView|
void PlatformViewAndroid::HandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  // Called from the ui thread.
  platform_message_handler_->HandlePlatformMessage(std::move(message));
}

// |PlatformView|
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
  if (env->IsSameObject(args, NULL)) {
    delegate_.OnPlatformViewDispatchSemanticsAction(
        kImplicitViewId, node_id, static_cast<flutter::SemanticsAction>(action),
        fml::MallocMapping());
    return;
  }

  uint8_t* args_data = static_cast<uint8_t*>(env->GetDirectBufferAddress(args));
  auto args_vector = fml::MallocMapping::Copy(args_data, args_position);

  delegate_.OnPlatformViewDispatchSemanticsAction(
      kImplicitViewId, node_id, static_cast<flutter::SemanticsAction>(action),
      std::move(args_vector));
}

// |PlatformView|
void PlatformViewAndroid::UpdateSemantics(
    int64_t view_id,
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  platform_view_android_delegate_.UpdateSemantics(update, actions);
}

// |PlatformView|
void PlatformViewAndroid::SetApplicationLocale(std::string locale) {
  jni_facade_->FlutterViewSetApplicationLocale(std::move(locale));
}

// |PlatformView|
void PlatformViewAndroid::SetSemanticsTreeEnabled(bool enabled) {
  jni_facade_->FlutterViewSetSemanticsTreeEnabled(enabled);
}

void PlatformViewAndroid::RegisterExternalTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) {
  std::shared_ptr<flutter::Texture> texture;
  switch (android_context_->RenderingApi()) {
    case AndroidRenderingAPI::kImpellerOpenGLES:
      // Impeller GLES.
      texture = std::make_shared<SurfaceTextureExternalTextureGLImpeller>(
          std::static_pointer_cast<impeller::ContextGLES>(
              android_context_->GetImpellerContext()),  //
          texture_id,                                   //
          surface_texture,                              //
          jni_facade_                                   //
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
              android_context_->GetImpellerContext()),  //
          texture_id,                                   //
          surface_texture,                              //
          jni_facade_                                   //
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
  std::shared_ptr<flutter::Texture> texture;
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
          std::static_pointer_cast<impeller::ContextGLES>(
              android_context_->GetImpellerContext()),
          texture_id, image_texture_entry, jni_facade_, lifecycle);
      break;
    case AndroidRenderingAPI::kImpellerVulkan:
      texture = std::make_shared<ImageExternalTextureVKImpeller>(
          std::static_pointer_cast<impeller::ContextVK>(
              android_context_->GetImpellerContext()),
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

// |PlatformDispatchTable|
void PlatformViewAndroid::OnVsyncCallback(intptr_t baton) {
  const static bool use_choreographer =
      impeller::android::Choreographer::IsAvailableOnPlatform();
  if (use_choreographer) {
    fml::TaskRunner::RunNowOrPostTask(
        task_runners_.GetUITaskRunner(),
        [baton, task_runners = task_runners_]() {
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
            // TODO(team-android): Get the actual refresh rate from the display.
            // https://github.com/flutter/flutter/issues/142845
            auto target_time = frame_time + fml::TimeDelta::FromNanoseconds(
                                                1000000000.0 / 60.0);
            VsyncWaiterEmbedder::OnEmbedderVsync(task_runners, baton,
                                                 frame_time, target_time);
          });
        });
  } else {
    // TODO(99798): Remove it when we drop support for API level < 29 and 32-bit
    // devices.
    task_runners_.GetPlatformTaskRunner()->PostTask(
        [task_runners = task_runners_]() {
          JNIEnv* env = fml::jni::AttachCurrentThread();
          // We need a way to trigger Java vsync and get back to
          // VsyncWaiterEmbedder::OnEmbedderVsync.
          // For now, this is a gap that needs to be addressed if we care
          // about API < 29.
          FML_LOG(ERROR) << "Java-based Vsync is not yet implemented in the "
                            "new Android embedder.";
        });
  }
}

// |PlatformDispatchTable|
void PlatformViewAndroid::SendChannelUpdate(const std::string& name,
                                            bool listening) {}

// |PlatformDispatchTable|
void PlatformViewAndroid::RequestViewFocusChange(
    const ViewFocusChangeRequest& request) {}

// |PlatformDispatchTable|
std::unique_ptr<std::vector<std::string>>
PlatformViewAndroid::ComputePlatformResolvedLocales(
    const std::vector<std::string>& supported_locale_data) {
  return jni_facade_->FlutterViewComputePlatformResolvedLocale(
      supported_locale_data);
}

// |PlatformView|
void PlatformViewAndroid::RequestDartDeferredLibrary(intptr_t loading_unit_id) {
  if (jni_facade_->RequestDartDeferredLibrary(loading_unit_id)) {
    return;
  }
  return;  // TODO(garyq): Call LoadDartDeferredLibraryFailure()
}

// |PlatformView|
void PlatformViewAndroid::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  delegate_.LoadDartDeferredLibrary(loading_unit_id, std::move(snapshot_data),
                                    std::move(snapshot_instructions));
}

// |PlatformView|
void PlatformViewAndroid::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  delegate_.LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                         transient);
}

// |PlatformView|
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
        platform_task_runner->PostTask([platform_view]() {
          // Back on Platform Task Runner.
          if (platform_view) {
            platform_view->FireFirstFrameCallback();
          }
        });
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
  }
}

fml::WeakPtr<PlatformViewAndroid> PlatformViewAndroid::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

void PlatformViewAndroid::SetSemanticsEnabled(bool enabled) {
  delegate_.OnPlatformViewSetSemanticsEnabled(enabled);
}

void PlatformViewAndroid::SetAccessibilityFeatures(int32_t flags) {
  delegate_.OnPlatformViewSetAccessibilityFeatures(flags);
}

void PlatformViewAndroid::SetViewportMetrics(int64_t view_id,
                                             const ViewportMetrics& metrics) {
  delegate_.OnPlatformViewSetViewportMetrics(view_id, metrics);
}

void PlatformViewAndroid::DispatchPointerDataPacket(
    std::unique_ptr<PointerDataPacket> packet) {
  delegate_.OnPlatformViewDispatchPointerDataPacket(std::move(packet));
}

void PlatformViewAndroid::UnregisterTexture(int64_t texture_id) {
  delegate_.OnPlatformViewUnregisterTexture(texture_id);
}

void PlatformViewAndroid::MarkTextureFrameAvailable(int64_t texture_id) {
  delegate_.OnPlatformViewMarkTextureFrameAvailable(texture_id);
}

void PlatformViewAndroid::ScheduleFrame() {
  delegate_.OnPlatformViewScheduleFrame();
}

}  // namespace flutter
