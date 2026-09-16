// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include <android/hardware_buffer_jni.h>
#include "flutter/assets/asset_resolver.h"
#include "flutter/common/settings.h"
#include "flutter/common/task_runners.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/lib/ui/window/platform_message.h"
#include "flutter/lib/ui/window/pointer_data_packet.h"
#include "flutter/lib/ui/window/viewport_metrics.h"
#include "flutter/shell/common/platform_view.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/platform/android/android_external_texture_adapter.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_surface_lifecycle.h"
#include "flutter/shell/platform/android/android_task_runners.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/image_external_texture.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_message_handler_android.h"
#include "flutter/shell/platform/android/platform_view_android_delegate/platform_view_android_delegate.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

class PlatformViewEmbedder;
class AndroidSurfaceFactoryImpl;
class SnapshotSurfaceProducer;

//----------------------------------------------------------------------------
/// @brief      This is the Android C++ embedding engine host.
///
/// @details    Corresponds to a FlutterEngine on the Java side. Manages the
///             engine lifecycle, thread configuration, surface lifecycle,
///             external textures, and dispatches platform events through
///             the embedder API.
///
class AndroidEngine final : public AndroidSurfaceLifecycle::Delegate,
                            public AndroidExternalTextureAdapter::Delegate {
 public:
  static bool Register(JNIEnv* env);

  AndroidEngine(const flutter::Settings& settings,
                std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                AndroidRenderingAPI android_rendering_api);

  AndroidEngine(const flutter::Settings& settings,
                const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
                const std::shared_ptr<AndroidTaskRunners>& task_runners,
                std::unique_ptr<Shell> shell,
                std::unique_ptr<APKAssetProvider> apk_asset_provider,
                AndroidRenderingAPI rendering_api,
                PlatformViewEmbedder* platform_view_embedder = nullptr,
                std::shared_ptr<AndroidContext> android_context = nullptr);

  ~AndroidEngine() override;

  bool IsValid() const;

  std::unique_ptr<AndroidEngine> Spawn(
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::string& initial_route,
      const std::vector<std::string>& entrypoint_args,
      int64_t engine_id) const;

  void Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
              const std::string& entrypoint,
              const std::string& libraryUrl,
              const std::vector<std::string>& entrypoint_args,
              int64_t engine_id);

  const flutter::Settings& GetSettings() const;

  // Compatibility helper during dissolution of PlatformViewAndroid.
  AndroidEngine* GetPlatformView() { return this; }

  PlatformView* GetPlatformViewDelegate() const { return platform_view_; }
  void SetPlatformView(PlatformView* platform_view) {
    platform_view_ = platform_view;
  }

  const flutter::TaskRunners& GetTaskRunners() const {
    return task_runners_->GetTaskRunners();
  }

  bool IsSurfaceControlEnabled() const;

  FlutterEngineResult InitializeEngine();

  FlutterEngineResult RunEngine(
      const std::string& entrypoint = "",
      const std::string& library_url = "",
      const std::vector<std::string>& entrypoint_args = {},
      int64_t engine_id = 0);

  FlutterEngineResult SpawnEngine(
      const FlutterEngineSpawnConfig* config,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      std::unique_ptr<AndroidEngine>* spawned_engine_out) const;

  Rasterizer::Screenshot Screenshot(Rasterizer::ScreenshotType type,
                                    bool base64_encode);

  void NotifyLowMemoryWarning();

  const std::shared_ptr<PlatformMessageHandler>& GetPlatformMessageHandler()
      const {
    return shell_->GetPlatformMessageHandler();
  }

  void UpdateDisplayMetrics();

  FlutterProjectArgs CreateFlutterProjectArgs(
      const std::string& entrypoint = "",
      const std::string& library_url = "",
      const std::vector<std::string>& entrypoint_args = {},
      int64_t engine_id = 0) const;

  // Surface lifecycle methods
  void NotifyCreated(fml::RefPtr<AndroidNativeWindow> native_window);
  void NotifySurfaceWindowChanged(
      fml::RefPtr<AndroidNativeWindow> native_window);
  void NotifyChanged(const DlISize& size);
  void NotifyDestroyed();
  void SetGpuAvailability(FlutterGpuAvailability availability);
  void SetupImpellerContext();
  AndroidSurface::ScreenshotResult ScreenshotSurface();

  // AndroidSurfaceLifecycle::Delegate
  void OnSurfaceCreated() override;
  void OnSurfaceDestroyed() override;
  void OnScheduleFrame() override;
  void OnInstallFirstFrameCallback() override;

  // AndroidExternalTextureAdapter::Delegate
  void OnRegisterTexture(std::shared_ptr<flutter::Texture> texture) override;
  void OnUnregisterTexture(int64_t texture_id) override;
  void OnMarkTextureFrameAvailable(int64_t texture_id) override;

  // Event & Seam Dispatch
  void DispatchPlatformMessage(JNIEnv* env,
                               std::string name,
                               jobject message_data,
                               jint message_position,
                               jint response_id);
  void DispatchEmptyPlatformMessage(JNIEnv* env,
                                    std::string name,
                                    jint response_id);
  void DispatchPointerDataPacket(std::unique_ptr<PointerDataPacket> packet);
  FlutterEngineResult SendPointerEvents(const FlutterPointerEvent* events,
                                        size_t count);

  void SetViewportMetrics(int64_t view_id, const ViewportMetrics& metrics);
  FlutterEngineResult SendWindowMetricsEvent(
      const FlutterWindowMetricsEvent* event);

  void DispatchSemanticsAction(JNIEnv* env,
                               jint id,
                               jint action,
                               jobject args,
                               jint args_position);
  FlutterEngineResult DispatchSemanticsAction(uint64_t node_id,
                                              FlutterSemanticsAction action,
                                              const uint8_t* data,
                                              size_t data_length);
  void SetSemanticsEnabled(bool enabled);
  FlutterEngineResult UpdateSemanticsEnabled(bool enabled);
  void SetAccessibilityFeatures(int32_t flags);
  FlutterEngineResult UpdateAccessibilityFeatures(
      FlutterAccessibilityFeature features);

  void RegisterExternalTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture);
  FlutterEngineResult RegisterSurfaceExternalTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture);

  void RegisterImageTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
      ImageExternalTexture::ImageLifecycle lifecycle);
  FlutterEngineResult RegisterImageExternalTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
      ImageExternalTexture::ImageLifecycle lifecycle);

  void UnregisterTexture(int64_t texture_id);
  FlutterEngineResult UnregisterExternalTexture(int64_t texture_id);

  void MarkTextureFrameAvailable(int64_t texture_id);
  FlutterEngineResult MarkExternalTextureFrameAvailable(int64_t texture_id);

  void OnDisplayPlatformView(int32_t view_id,
                             int32_t x,
                             int32_t y,
                             int32_t width,
                             int32_t height,
                             int32_t view_width,
                             int32_t view_height,
                             MutatorsStack mutators_stack);
  FlutterEngineResult DisplayPlatformView(int32_t view_id,
                                          int32_t x,
                                          int32_t y,
                                          int32_t width,
                                          int32_t height,
                                          int32_t view_width,
                                          int32_t view_height,
                                          MutatorsStack mutators_stack);
  FlutterEngineResult DisplayPlatformViewEmbedder(int32_t view_id,
                                                  int32_t x,
                                                  int32_t y,
                                                  int32_t width,
                                                  int32_t height,
                                                  int32_t view_width,
                                                  int32_t view_height,
                                                  MutatorsStack mutators_stack);

  void OnDisplayOverlaySurface(int32_t surface_id,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height);
  FlutterEngineResult DisplayOverlaySurface(int32_t surface_id,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height);

  void OnDisplayVirtualDisplayPlatformView(int32_t view_id,
                                           int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height);
  FlutterEngineResult DisplayVirtualDisplayPlatformView(int32_t view_id,
                                                        int32_t x,
                                                        int32_t y,
                                                        int32_t width,
                                                        int32_t height);
  FlutterEngineResult DisplayVirtualDisplayPlatformViewEmbedder(int32_t view_id,
                                                                int32_t x,
                                                                int32_t y,
                                                                int32_t width,
                                                                int32_t height);

  void OnDisplayPlatformView2(int32_t view_id,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height,
                              int32_t view_width,
                              int32_t view_height,
                              MutatorsStack mutators_stack);
  FlutterEngineResult DisplayPlatformView2(int32_t view_id,
                                           int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height,
                                           int32_t view_width,
                                           int32_t view_height,
                                           MutatorsStack mutators_stack);
  FlutterEngineResult DisplayPlatformView2Embedder(
      int32_t view_id,
      int32_t x,
      int32_t y,
      int32_t width,
      int32_t height,
      int32_t view_width,
      int32_t view_height,
      MutatorsStack mutators_stack);

  void OnHidePlatformView2(int32_t view_id);
  FlutterEngineResult HidePlatformView2(int32_t view_id);

  void BeginFrameHC();
  FlutterEngineResult BeginFrameHCEmbedder();

  void EndFrameHC();
  FlutterEngineResult EndFrameHCEmbedder();

  std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>
  CreateOverlaySurfaceHC();
  FlutterEngineResult CreateOverlaySurfaceHCEmbedder(
      std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>* out_metadata);

  void DestroyOverlaySurfacesHC();
  FlutterEngineResult DestroyOverlaySurfacesHCEmbedder();

  void ScheduleFrame();

  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions);
  void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string error_message,
                                    bool transient);
  void UpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type);

  void UpdateSemantics(int64_t view_id,
                       flutter::SemanticsNodeUpdates update,
                       flutter::CustomAccessibilityActionUpdates actions);
  void SetApplicationLocale(std::string locale);
  void SetSemanticsTreeEnabled(bool enabled);
  void HandlePlatformMessage(std::unique_ptr<flutter::PlatformMessage> message);
  void OnPreEngineRestart() const;
  std::unique_ptr<VsyncWaiter> CreateVSyncWaiter();
  std::unique_ptr<Surface> CreateRenderingSurface();
  std::shared_ptr<ExternalViewEmbedder> CreateExternalViewEmbedder();
  std::unique_ptr<SnapshotSurfaceProducer> CreateSnapshotSurfaceProducer();
  sk_sp<GrDirectContext> CreateResourceContext() const;
  void ReleaseResourceContext() const;
  std::shared_ptr<impeller::Context> GetImpellerContext() const;
  std::unique_ptr<std::vector<std::string>> ComputePlatformResolvedLocales(
      const std::vector<std::string>& supported_locale_data);
  void RequestDartDeferredLibrary(intptr_t loading_unit_id);
  void InstallFirstFrameCallback();
  void FireFirstFrameCallback();
  double GetScaledFontSize(double unscaled_font_size,
                           int configuration_id) const;

  // Visible for testing.
  const std::unique_ptr<Shell>& GetShellForTesting() const { return shell_; }
  PlatformViewEmbedder* GetPlatformViewEmbedderForTesting() const {
    return platform_view_embedder_;
  }
  const std::shared_ptr<AndroidTaskRunners>& GetTaskRunnersForTesting() const {
    return task_runners_;
  }
  const FlutterAssetResolver* GetAssetResolverForTesting() const {
    return apk_asset_provider_ ? apk_asset_provider_->GetFlutterAssetResolver()
                               : nullptr;
  }
  const FlutterProjectArgs* GetProjectArgsForTesting() const {
    return &project_args_;
  }
  AndroidSurfaceLifecycle* GetSurfaceLifecycleForTesting() const {
    return surface_lifecycle_.get();
  }
  AndroidCompositorAdapter* GetCompositorAdapterForTesting() const {
    return compositor_adapter_.get();
  }
  AndroidExternalTextureAdapter* GetExternalTextureAdapterForTesting() const {
    return external_texture_adapter_.get();
  }
  const std::shared_ptr<AndroidContext>& GetAndroidContext() {
    return android_context_;
  }

 private:
  void InitializeProjectArgs();
  void InitializeSurfaceAndAdapters(
      std::shared_ptr<AndroidContext> android_context = nullptr);

  static void OnPreEngineRestart(void* user_data);
  static void OnSetApplicationLocale(const char* locale, void* user_data);
  static double OnGetScaledFontSize(double unscaled_font_size,
                                    int configuration_id,
                                    void* user_data);
  static void OnRequestDartDeferredLibrary(intptr_t loading_unit_id,
                                           void* user_data);
  static void ThreadDestructCallback(void* value);
  std::optional<RunConfiguration> BuildRunConfiguration(
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::vector<std::string>& entrypoint_args) const;
  bool IsNDKImageDecoderAvailable();

  const flutter::Settings settings_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  std::shared_ptr<AndroidTaskRunners> task_runners_;
  std::unique_ptr<Shell> shell_;
  bool is_valid_ = false;
  uint64_t next_pointer_flow_id_ = 0;
  std::unique_ptr<APKAssetProvider> apk_asset_provider_;
  const AndroidRenderingAPI android_rendering_api_;
  PlatformViewEmbedder* platform_view_embedder_ = nullptr;
  PlatformView* platform_view_ = nullptr;
  FlutterProjectArgs project_args_ = {};
  const FlutterAssetResolver* asset_resolvers_[1] = {nullptr};

  std::shared_ptr<AndroidContext> android_context_;
  std::shared_ptr<AndroidSurfaceFactoryImpl> surface_factory_;
  PlatformViewAndroidDelegate platform_view_android_delegate_;
  std::unique_ptr<AndroidSurface> android_surface_;
  std::unique_ptr<AndroidSurfaceLifecycle> surface_lifecycle_;
  std::shared_ptr<PlatformMessageHandlerAndroid> platform_message_handler_;
  bool android_meets_hcpp_criteria_ = false;
  std::shared_ptr<AndroidCompositorAdapter> compositor_adapter_;
  std::unique_ptr<AndroidExternalTextureAdapter> external_texture_adapter_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidEngine);
};

// Aliases for backwards compatibility during migration dissolution.
using AndroidShellHolder = AndroidEngine;
using PlatformViewAndroid = AndroidEngine;

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
