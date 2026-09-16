// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include <android/hardware_buffer_jni.h>
#include "flutter/assets/asset_resolver.h"
#include "flutter/common/settings.h"
#include "flutter/common/task_runners.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/lib/ui/window/platform_message.h"
#include "flutter/shell/common/snapshot_surface_producer.h"
#include "flutter/shell/platform/android/android_external_texture_adapter.h"
#include "flutter/shell/platform/android/android_surface_lifecycle.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_message_handler_android.h"
#include "flutter/shell/platform/android/platform_view_android_delegate/platform_view_android_delegate.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "shell/platform/android/image_external_texture.h"

namespace flutter {

class AndroidCompositorAdapter;
class PlatformView;
class PointerDataPacket;
struct ViewportMetrics;
class VsyncWaiter;

class AndroidSurfaceFactoryImpl : public AndroidSurfaceFactory {
 public:
  AndroidSurfaceFactoryImpl(const std::shared_ptr<AndroidContext>& context,
                            bool enable_impeller,
                            bool lazy_shader_mode);

  ~AndroidSurfaceFactoryImpl() override;

  std::unique_ptr<AndroidSurface> CreateSurface() override;

 private:
  const std::shared_ptr<AndroidContext>& android_context_;
  const bool enable_impeller_;
  const bool lazy_shader_mode_;
};

class PlatformViewAndroid final
    : public AndroidSurfaceLifecycle::Delegate,
      public AndroidExternalTextureAdapter::Delegate {
 public:
  static bool Register(JNIEnv* env);

  PlatformViewAndroid(
      const flutter::Settings& settings,
      const flutter::TaskRunners& task_runners,
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
      AndroidRenderingAPI rendering_api,
      std::shared_ptr<fml::BasicTaskRunner> io_task_runner = nullptr);

  //----------------------------------------------------------------------------
  /// @brief      Creates a new PlatformViewAndroid but using an existing
  ///             Android GPU context to create new surfaces. This maximizes
  ///             resource sharing between 2 PlatformViewAndroids of 2 Shells.
  ///
  PlatformViewAndroid(
      const flutter::Settings& settings,
      const flutter::TaskRunners& task_runners,
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
      const std::shared_ptr<flutter::AndroidContext>& android_context);

  ~PlatformViewAndroid();

  void SetPlatformView(PlatformView* platform_view);

  PlatformView* GetPlatformViewDelegate() const;

  fml::WeakPtr<PlatformViewAndroid> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  const flutter::TaskRunners& GetTaskRunners() const { return task_runners_; }

  void NotifyCreated(fml::RefPtr<AndroidNativeWindow> native_window);

  void NotifySurfaceWindowChanged(
      fml::RefPtr<AndroidNativeWindow> native_window);

  void NotifyChanged(const DlISize& size);

  void NotifyDestroyed();

  void SetGpuAvailability(FlutterGpuAvailability availability);

  AndroidSurfaceLifecycle* GetSurfaceLifecycleForTesting() const {
    return surface_lifecycle_.get();
  }

  AndroidCompositorAdapter* GetCompositorAdapterForTesting() const {
    return compositor_adapter_.get();
  }

  AndroidExternalTextureAdapter* GetExternalTextureAdapterForTesting() const {
    return external_texture_adapter_.get();
  }

  bool IsAndroidEmbedderApiEnabled() const { return android_embedder_api_; }

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

  const std::shared_ptr<AndroidContext>& GetAndroidContext() {
    return android_context_;
  }

  std::shared_ptr<PlatformMessageHandler> GetPlatformMessageHandler() const {
    if (platform_view_) {
      platform_view_->GetPlatformMessageHandler();
    }
    return platform_message_handler_;
  }

  /// @brief Whether the SurfaceControl based swapchain is enabled and active.
  bool IsSurfaceControlEnabled() const;

  void SetupImpellerContext();

  AndroidSurface::ScreenshotResult Screenshot();

  std::unique_ptr<Surface> CreateGPUSurface();
  std::shared_ptr<impeller::Context> GetImpellerContextLegacy() const;
  sk_sp<GrDirectContext> CreateResourceContextLegacy() const;
  void ReleaseResourceContextLegacy() const;
  void UpdateSemanticsLegacy(flutter::SemanticsNodeUpdates update,
                             flutter::CustomAccessibilityActionUpdates actions);

 private:
  void RegisterTexture(std::shared_ptr<flutter::Texture> texture);

  const flutter::TaskRunners task_runners_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  std::shared_ptr<AndroidContext> android_context_;
  std::shared_ptr<AndroidSurfaceFactoryImpl> surface_factory_;

  PlatformViewAndroidDelegate platform_view_android_delegate_;

  std::unique_ptr<AndroidSurface> android_surface_;
  std::unique_ptr<AndroidSurfaceLifecycle> surface_lifecycle_;
  std::shared_ptr<PlatformMessageHandlerAndroid> platform_message_handler_;
  PlatformView* platform_view_ = nullptr;
  bool android_meets_hcpp_criteria_ = false;
  std::shared_ptr<AndroidCompositorAdapter> compositor_adapter_;
  std::unique_ptr<AndroidExternalTextureAdapter> external_texture_adapter_;
  const bool android_embedder_api_;
  fml::WeakPtrFactory<PlatformViewAndroid> weak_factory_{this};

 public:
  // |AndroidSurfaceLifecycle::Delegate|
  void OnSurfaceCreated() override;
  void OnSurfaceDestroyed() override;
  void OnScheduleFrame() override;
  void OnInstallFirstFrameCallback() override;

  // |AndroidExternalTextureAdapter::Delegate|
  void OnRegisterTexture(std::shared_ptr<flutter::Texture> texture) override;
  void OnUnregisterTexture(int64_t texture_id) override;
  void OnMarkTextureFrameAvailable(int64_t texture_id) override;

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

  FML_DISALLOW_COPY_AND_ASSIGN(PlatformViewAndroid);
};
}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_H_
