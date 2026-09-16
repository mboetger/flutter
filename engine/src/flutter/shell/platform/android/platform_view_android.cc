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
#include "flutter/fml/task_runner_util.h"
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
#include "flutter/shell/platform/android/android_compositor_adapter.h"
#include "flutter/shell/platform/android/android_external_texture_adapter.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder_wrapper.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_message_response_android.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/shell/platform/android/surface/snapshot_surface_producer.h"
#include "flutter/shell/platform/android/vsync_waiter_android.h"

namespace flutter {

namespace {

static constexpr int kMinAPILevelHCPP = 34;
static constexpr int64_t kImplicitViewId = 0;

AndroidContext::ContextSettings CreateContextSettings(
    const Settings& p_settings) {
  AndroidContext::ContextSettings settings;
  settings.enable_gpu_tracing = p_settings.enable_vulkan_gpu_tracing;
  settings.enable_validation = p_settings.enable_vulkan_validation;
  settings.enable_surface_control = p_settings.enable_surface_control;
  return settings;
}
}  // namespace

AndroidSurfaceFactoryImpl::AndroidSurfaceFactoryImpl(
    const std::shared_ptr<AndroidContext>& context,
    bool enable_impeller,
    bool lazy_shader_mode)
    : android_context_(context),
      enable_impeller_(enable_impeller),
      lazy_shader_mode_(lazy_shader_mode) {}

AndroidSurfaceFactoryImpl::~AndroidSurfaceFactoryImpl() = default;

std::unique_ptr<AndroidSurface> AndroidSurfaceFactoryImpl::CreateSurface() {
  if (android_context_->IsDynamicSelection()) {
    auto cast_ptr = std::static_pointer_cast<AndroidContextDynamicImpeller>(
        android_context_);
    return std::make_unique<AndroidSurfaceDynamicImpeller>(cast_ptr);
  }
  switch (android_context_->RenderingApi()) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      return std::make_unique<AndroidSurfaceSoftware>();
    case AndroidRenderingAPI::kSkiaOpenGLES:
      return std::make_unique<AndroidSurfaceGLSkia>(
          std::static_pointer_cast<AndroidContextGLSkia>(android_context_));
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
      return std::make_unique<AndroidSurfaceGLImpeller>(
          std::static_pointer_cast<AndroidContextGLImpeller>(android_context_));
    case AndroidRenderingAPI::kImpellerVulkan:
      return std::make_unique<AndroidSurfaceVKImpeller>(
          std::static_pointer_cast<AndroidContextVKImpeller>(android_context_));
    case AndroidRenderingAPI::kImpellerAutoselect: {
      auto cast_ptr = std::static_pointer_cast<AndroidContextDynamicImpeller>(
          android_context_);
      return std::make_unique<AndroidSurfaceDynamicImpeller>(cast_ptr);
    }
  }
  FML_UNREACHABLE();
}

static std::shared_ptr<flutter::AndroidContext> CreateAndroidContext(
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
    const flutter::Settings& settings,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    AndroidRenderingAPI rendering_api,
    std::shared_ptr<fml::BasicTaskRunner> io_task_runner)
    : PlatformViewAndroid(
          settings,
          task_runners,
          jni_facade,
          CreateAndroidContext(
              task_runners,
              rendering_api,
              settings.enable_opengl_gpu_tracing,
              CreateContextSettings(settings),
              io_task_runner ? std::move(io_task_runner)
                             : std::make_shared<fml::WrapperBasicTaskRunner>(
                                   task_runners.GetIOTaskRunner()))) {}

PlatformViewAndroid::PlatformViewAndroid(
    const flutter::Settings& settings,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<flutter::AndroidContext>& android_context)
    : task_runners_(task_runners),
      jni_facade_(jni_facade),
      android_context_(android_context),
      platform_view_android_delegate_(jni_facade),
      platform_message_handler_(new PlatformMessageHandlerAndroid(jni_facade)),
      weak_factory_(this) {
  if (android_context_) {
    FML_CHECK(android_context_->IsValid())
        << "Could not create surface from invalid Android context.";
    surface_factory_ = std::make_shared<AndroidSurfaceFactoryImpl>(
        android_context_,                          //
        settings.enable_impeller,                  //
        settings.impeller_enable_lazy_shader_mode  //
    );
    android_surface_ = surface_factory_->CreateSurface();
    android_meets_hcpp_criteria_ =
        settings.enable_surface_control &&
        android_get_device_api_level() >= kMinAPILevelHCPP &&
        settings.enable_impeller;
    FML_CHECK(android_surface_ && android_surface_->IsValid())
        << "Could not create an OpenGL, Vulkan or Software surface to set "
           "up "
           "rendering.";
  }
  surface_lifecycle_ = std::make_unique<AndroidSurfaceLifecycle>(
      task_runners_.GetRasterTaskRunner(), jni_facade_, android_surface_.get(),
      this);
  external_texture_adapter_ = std::make_unique<AndroidExternalTextureAdapter>(
      android_context_, jni_facade_, this);
}

PlatformViewAndroid::~PlatformViewAndroid() = default;

void PlatformViewAndroid::SetPlatformView(PlatformView* platform_view) {
  platform_view_ = platform_view;
}

PlatformView* PlatformViewAndroid::GetPlatformViewDelegate() const {
  return platform_view_;
}

void PlatformViewAndroid::NotifyCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifyCreated(std::move(native_window));
  }
}

void PlatformViewAndroid::NotifySurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifySurfaceWindowChanged(std::move(native_window));
  }
}

void PlatformViewAndroid::NotifyDestroyed() {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifyDestroyed();
  }
}

void PlatformViewAndroid::NotifyChanged(const DlISize& size) {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifyChanged(size);
  }
}

void PlatformViewAndroid::SetGpuAvailability(
    FlutterGpuAvailability availability) {
  if (surface_lifecycle_) {
    surface_lifecycle_->SetGpuAvailability(availability);
  }
}

void PlatformViewAndroid::OnSurfaceCreated() {
  if (platform_view_) {
    platform_view_->NotifyCreated();
  }
}

void PlatformViewAndroid::OnSurfaceDestroyed() {
  if (platform_view_) {
    platform_view_->NotifyDestroyed();
  }
}

void PlatformViewAndroid::OnScheduleFrame() {
  if (platform_view_) {
    platform_view_->ScheduleFrame();
  }
}

void PlatformViewAndroid::OnInstallFirstFrameCallback() {
  InstallFirstFrameCallback();
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

void PlatformViewAndroid::DispatchPointerDataPacket(
    std::unique_ptr<PointerDataPacket> packet) {
  if (platform_view_) {
    platform_view_->DispatchPointerDataPacket(std::move(packet));
  }
}

void PlatformViewAndroid::SetViewportMetrics(int64_t view_id,
                                             const ViewportMetrics& metrics) {
  if (platform_view_) {
    platform_view_->SetViewportMetrics(view_id, metrics);
  }
}

// |PlatformView|
void PlatformViewAndroid::HandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  if (platform_view_) {
    platform_view_->HandlePlatformMessage(std::move(message));
    return;
  }
  // Called from the ui thread.
  platform_message_handler_->HandlePlatformMessage(std::move(message));
}

// |PlatformView|
void PlatformViewAndroid::OnPreEngineRestart() const {
  if (platform_view_) {
    platform_view_->OnPreEngineRestart();
    return;
  }
  jni_facade_->FlutterViewOnPreEngineRestart();
}

void PlatformViewAndroid::DispatchSemanticsAction(JNIEnv* env,
                                                  jint node_id,
                                                  jint action,
                                                  jobject args,
                                                  jint args_position) {
  if (!platform_view_) {
    return;
  }
  // TODO(team-android): Remove implicit view assumption.
  // https://github.com/flutter/flutter/issues/142845
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

void PlatformViewAndroid::RegisterTexture(
    std::shared_ptr<flutter::Texture> texture) {
  if (platform_view_) {
    platform_view_->RegisterTexture(std::move(texture));
  }
}

void PlatformViewAndroid::UnregisterTexture(int64_t texture_id) {
  if (external_texture_adapter_) {
    external_texture_adapter_->UnregisterExternalTexture(texture_id);
    return;
  }
  if (platform_view_) {
    platform_view_->UnregisterTexture(texture_id);
  }
}

void PlatformViewAndroid::MarkTextureFrameAvailable(int64_t texture_id) {
  if (external_texture_adapter_) {
    external_texture_adapter_->MarkExternalTextureFrameAvailable(texture_id);
    return;
  }
  if (platform_view_) {
    platform_view_->MarkTextureFrameAvailable(texture_id);
  }
}

// |AndroidExternalTextureAdapter::Delegate|
void PlatformViewAndroid::OnRegisterTexture(
    std::shared_ptr<flutter::Texture> texture) {
  RegisterTexture(std::move(texture));
}

// |AndroidExternalTextureAdapter::Delegate|
void PlatformViewAndroid::OnUnregisterTexture(int64_t texture_id) {
  if (platform_view_) {
    platform_view_->UnregisterTexture(texture_id);
  }
}

// |AndroidExternalTextureAdapter::Delegate|
void PlatformViewAndroid::OnMarkTextureFrameAvailable(int64_t texture_id) {
  if (platform_view_) {
    platform_view_->MarkTextureFrameAvailable(texture_id);
  }
}

void PlatformViewAndroid::ScheduleFrame() {
  if (platform_view_) {
    platform_view_->ScheduleFrame();
  }
}

void PlatformViewAndroid::UpdateSemanticsLegacy(
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  platform_view_android_delegate_.UpdateSemantics(update, actions);
}

// |PlatformView|
void PlatformViewAndroid::UpdateSemantics(
    int64_t view_id,
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  if (platform_view_) {
    platform_view_->UpdateSemantics(view_id, std::move(update),
                                    std::move(actions));
    return;
  }
  UpdateSemanticsLegacy(std::move(update), std::move(actions));
}

// |PlatformView|
void PlatformViewAndroid::SetApplicationLocale(std::string locale) {
  if (platform_view_) {
    platform_view_->SetApplicationLocale(locale);
    return;
  }
  jni_facade_->FlutterViewSetApplicationLocale(std::move(locale));
}

// |PlatformView|
void PlatformViewAndroid::SetSemanticsTreeEnabled(bool enabled) {
  if (platform_view_) {
    platform_view_->SetSemanticsTreeEnabled(enabled);
  }
  jni_facade_->FlutterViewSetSemanticsTreeEnabled(enabled);
}

void PlatformViewAndroid::RegisterExternalTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) {
  if (external_texture_adapter_) {
    external_texture_adapter_->RegisterSurfaceTexture(texture_id,
                                                      surface_texture);
  }
}

void PlatformViewAndroid::RegisterImageTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    ImageExternalTexture::ImageLifecycle lifecycle) {
  if (external_texture_adapter_) {
    external_texture_adapter_->RegisterImageTexture(
        texture_id, image_texture_entry, lifecycle);
  }
}

// |PlatformView|
std::unique_ptr<VsyncWaiter> PlatformViewAndroid::CreateVSyncWaiter() {
  return std::make_unique<VsyncWaiterAndroid>(task_runners_);
}

namespace {
class PlatformViewProtectedAccessor : public PlatformView {
 public:
  using PlatformView::CreateRenderingSurface;
};
}  // namespace

std::unique_ptr<Surface> PlatformViewAndroid::CreateGPUSurface() {
  if (!android_surface_) {
    return nullptr;
  }
  return android_surface_->CreateGPUSurface(
      android_context_->GetMainSkiaContext().get());
}

// |PlatformView|
std::unique_ptr<Surface> PlatformViewAndroid::CreateRenderingSurface() {
  if (platform_view_) {
    return static_cast<PlatformViewProtectedAccessor*>(platform_view_)
        ->CreateRenderingSurface();
  }
  return CreateGPUSurface();
}

// |PlatformView|
std::shared_ptr<ExternalViewEmbedder>
PlatformViewAndroid::CreateExternalViewEmbedder() {
  if (platform_view_) {
    auto embedder = platform_view_->CreateExternalViewEmbedder();
    if (embedder) {
      return embedder;
    }
  }
  auto view_embedder = std::make_shared<AndroidExternalViewEmbedderWrapper>(
      android_meets_hcpp_criteria_, *android_context_, jni_facade_,
      surface_factory_, task_runners_);
  compositor_adapter_ =
      std::make_shared<AndroidCompositorAdapter>(std::move(view_embedder));
  return compositor_adapter_;
}

// |PlatformView|
std::unique_ptr<SnapshotSurfaceProducer>
PlatformViewAndroid::CreateSnapshotSurfaceProducer() {
  if (platform_view_) {
    auto producer = platform_view_->CreateSnapshotSurfaceProducer();
    if (producer) {
      return producer;
    }
  }
  if (!android_surface_) {
    return nullptr;
  }
  return std::make_unique<AndroidSnapshotSurfaceProducer>(*android_surface_);
}

sk_sp<GrDirectContext> PlatformViewAndroid::CreateResourceContextLegacy()
    const {
  if (!android_surface_) {
    return nullptr;
  }
#if !SLIMPELLER
  sk_sp<GrDirectContext> resource_context;
  if (android_surface_->ResourceContextMakeCurrent()) {
    // TODO(chinmaygarde): Currently, this code depends on the fact that only
    // the OpenGL surface will be able to make a resource context current. If
    // this changes, this assumption breaks. Handle the same.
    resource_context = ShellIOManager::CreateCompatibleResourceLoadingContext(
        GrBackendApi::kOpenGL,
        GPUSurfaceGLDelegate::GetDefaultPlatformGLInterface());
  } else {
    FML_DLOG(ERROR) << "Could not make the resource context current.";
  }
  return resource_context;
#else
  android_surface_->ResourceContextMakeCurrent();
  return nullptr;
#endif  //  !SLIMPELLER
}

// |PlatformView|
sk_sp<GrDirectContext> PlatformViewAndroid::CreateResourceContext() const {
  if (platform_view_) {
    return platform_view_->CreateResourceContext();
  }
  return CreateResourceContextLegacy();
}

void PlatformViewAndroid::ReleaseResourceContextLegacy() const {
  if (android_surface_) {
    android_surface_->ResourceContextClearCurrent();
  }
}

// |PlatformView|
void PlatformViewAndroid::ReleaseResourceContext() const {
  if (platform_view_) {
    platform_view_->ReleaseResourceContext();
    return;
  }
  ReleaseResourceContextLegacy();
}

std::shared_ptr<impeller::Context>
PlatformViewAndroid::GetImpellerContextLegacy() const {
  if (android_surface_) {
    return android_surface_->GetImpellerContext();
  }
  return android_context_->GetImpellerContext();
}

// |PlatformView|
std::shared_ptr<impeller::Context> PlatformViewAndroid::GetImpellerContext()
    const {
  if (platform_view_) {
    return platform_view_->GetImpellerContext();
  }
  return GetImpellerContextLegacy();
}

// |PlatformView|
std::unique_ptr<std::vector<std::string>>
PlatformViewAndroid::ComputePlatformResolvedLocales(
    const std::vector<std::string>& supported_locale_data) {
  if (platform_view_) {
    return platform_view_->ComputePlatformResolvedLocales(
        supported_locale_data);
  }
  return jni_facade_->FlutterViewComputePlatformResolvedLocale(
      supported_locale_data);
}

// |PlatformView|
void PlatformViewAndroid::RequestDartDeferredLibrary(intptr_t loading_unit_id) {
  if (platform_view_) {
    platform_view_->RequestDartDeferredLibrary(loading_unit_id);
    return;
  }
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
  if (platform_view_) {
    platform_view_->LoadDartDeferredLibrary(loading_unit_id,
                                            std::move(snapshot_data),
                                            std::move(snapshot_instructions));
  }
}

// |PlatformView|
void PlatformViewAndroid::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  if (platform_view_) {
    platform_view_->LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                                 transient);
  }
}

// |PlatformView|
void PlatformViewAndroid::UpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  if (platform_view_) {
    platform_view_->UpdateAssetResolverByType(std::move(updated_asset_resolver),
                                              type);
  }
}

void PlatformViewAndroid::InstallFirstFrameCallback() {
  if (platform_view_) {
    platform_view_->SetNextFrameCallback(
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
}

void PlatformViewAndroid::FireFirstFrameCallback() {
  jni_facade_->FlutterViewOnFirstFrame();
}

double PlatformViewAndroid::GetScaledFontSize(double unscaled_font_size,
                                              int configuration_id) const {
  if (platform_view_) {
    return platform_view_->GetScaledFontSize(unscaled_font_size,
                                             configuration_id);
  }
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
  if (platform_view_) {
    platform_view_->SetupImpellerContext();
  }
  android_context_->SetupImpellerContext();
  android_surface_->SetupImpellerSurface();
}

AndroidSurface::ScreenshotResult PlatformViewAndroid::Screenshot() {
  if (!android_surface_) {
    return {};
  }
  AndroidSurface::ScreenshotResult screenshot;
  fml::AutoResetWaitableEvent latch;
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetRasterTaskRunner(),
      [&latch, surface = android_surface_.get(), &screenshot]() {
        screenshot = surface->Screenshot();
        latch.Signal();
      });
  latch.Wait();
  return screenshot;
}

}  // namespace flutter
