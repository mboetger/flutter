// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include <android/hardware_buffer_jni.h>
#include "flutter/common/settings.h"
#include "flutter/flow/surface.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/fml/task_runner.h"
#include "flutter/lib/ui/window/platform_message.h"
#include "flutter/shell/common/snapshot_surface_producer.h"
#include "flutter/shell/common/vsync_waiter.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_message_handler_android.h"
#include "flutter/shell/platform/android/platform_view_android_delegate/platform_view_android_delegate.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "shell/platform/android/image_external_texture.h"

namespace flutter {

class APKAssetProvider;

class AndroidSurfaceFactoryImpl : public AndroidSurfaceFactory {
 public:
  AndroidSurfaceFactoryImpl(std::shared_ptr<AndroidContext> context,
                            bool enable_impeller,
                            bool lazy_shader_mode);

  ~AndroidSurfaceFactoryImpl() override;

  std::unique_ptr<AndroidSurface> CreateSurface() override;

 private:
  const std::shared_ptr<AndroidContext> android_context_;
  const bool enable_impeller_;
  const bool lazy_shader_mode_;
};

class PlatformViewAndroid final {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual const Settings& OnPlatformViewGetSettings() const = 0;
    virtual std::shared_ptr<fml::BasicTaskRunner>
    OnPlatformViewGetShutdownSafeIOTaskRunner() const = 0;
    virtual void OnPlatformViewCreated(std::unique_ptr<Surface> surface) = 0;
    virtual void OnPlatformViewDestroyed() = 0;
    virtual void OnPlatformViewScheduleFrame() = 0;
    virtual void OnPlatformViewSetNextFrameCallback(
        const fml::closure& closure) = 0;
    virtual void OnPlatformViewSetViewportMetrics(
        const FlutterWindowMetricsEvent& metrics) = 0;
    virtual void OnPlatformViewDispatchPlatformMessage(
        std::unique_ptr<flutter::PlatformMessage> message) = 0;
    virtual void OnPlatformViewDispatchPointerDataPacket(const uint8_t* data,
                                                         size_t size) = 0;
    virtual void OnPlatformViewDispatchSemanticsAction(
        int64_t view_id,
        int32_t node_id,
        FlutterSemanticsAction action,
        fml::MallocMapping args) = 0;
    virtual void OnPlatformViewSetSemanticsEnabled(bool enabled) = 0;
    virtual void OnPlatformViewSetAccessibilityFeatures(int32_t flags) = 0;
    virtual void OnPlatformViewRegisterTexture(
        std::shared_ptr<flutter::Texture> texture) = 0;
    virtual void OnPlatformViewUnregisterTexture(int64_t texture_id) = 0;
    virtual void OnPlatformViewMarkTextureFrameAvailable(
        int64_t texture_id) = 0;
    virtual void LoadDartDeferredLibrary(
        intptr_t loading_unit_id,
        std::unique_ptr<const fml::Mapping> snapshot_data,
        std::unique_ptr<const fml::Mapping> snapshot_instructions) = 0;
    virtual void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                              const std::string error_message,
                                              bool transient) = 0;
    virtual void UpdateAssetResolver(
        std::unique_ptr<APKAssetProvider> updated_asset_provider) = 0;
  };

  static bool Register(JNIEnv* env);

  PlatformViewAndroid(PlatformViewAndroid::Delegate& delegate,
                      const flutter::TaskRunners& task_runners,
                      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
                      AndroidRenderingAPI rendering_api);

  //----------------------------------------------------------------------------
  /// @brief      Creates a new PlatformViewAndroid but using an existing
  ///             Android GPU context to create new surfaces. This maximizes
  ///             resource sharing between 2 PlatformViewAndroids of 2 Shells.
  ///
  PlatformViewAndroid(
      PlatformViewAndroid::Delegate& delegate,
      const flutter::TaskRunners& task_runners,
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
      const std::shared_ptr<flutter::AndroidContext>& android_context);

  ~PlatformViewAndroid();

  fml::WeakPtr<PlatformViewAndroid> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  const flutter::TaskRunners& GetTaskRunners() const { return task_runners_; }

  void NotifyCreated(fml::RefPtr<AndroidNativeWindow> native_window);

  void NotifySurfaceWindowChanged(
      fml::RefPtr<AndroidNativeWindow> native_window);

  void NotifyChanged(const DlISize& size);

  void NotifyDestroyed();

  void SetViewportMetrics(const FlutterWindowMetricsEvent& metrics);

  void DispatchPlatformMessage(JNIEnv* env,
                               std::string name,
                               jobject message_data,
                               jint message_position,
                               jint response_id);

  void DispatchEmptyPlatformMessage(JNIEnv* env,
                                    std::string name,
                                    jint response_id);

  void DispatchPointerDataPacket(const uint8_t* data, size_t size);

  void DispatchSemanticsAction(JNIEnv* env,
                               jint id,
                               jint action,
                               jobject args,
                               jint args_position);

  void SetSemanticsEnabled(bool enabled);

  void SetAccessibilityFeatures(int32_t flags);

  std::unique_ptr<Texture> CreateSurfaceTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) const;

  std::unique_ptr<Texture> CreateImageTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
      ImageExternalTexture::ImageLifecycle lifecycle) const;

  void RegisterExternalTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture);

  void RegisterImageTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
      ImageExternalTexture::ImageLifecycle lifecycle);

  void UnregisterTexture(int64_t texture_id);

  void MarkTextureFrameAvailable(int64_t texture_id);

  void ScheduleFrame();

  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions);

  void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string error_message,
                                    bool transient);

  void UpdateAssetResolver(
      std::unique_ptr<APKAssetProvider> updated_asset_provider);

  const std::shared_ptr<AndroidContext>& GetAndroidContext() {
    return android_context_;
  }

  AndroidSurface* GetAndroidSurface() const { return android_surface_.get(); }

  std::shared_ptr<AndroidSurfaceFactoryImpl> GetSurfaceFactory() const {
    return surface_factory_;
  }

  std::shared_ptr<PlatformMessageHandler> GetPlatformMessageHandler() const {
    return platform_message_handler_;
  }

  /// @brief Whether the SurfaceControl based swapchain is enabled and active.
  bool IsSurfaceControlEnabled() const;

  void SetupImpellerContext();

  void UpdateSemantics(const FlutterSemanticsUpdate2* update);

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

  double GetScaledFontSize(double unscaled_font_size,
                           int configuration_id) const;

 private:
  void InstallFirstFrameCallback();

  void FireFirstFrameCallback();

  Delegate& delegate_;
  const TaskRunners task_runners_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  std::shared_ptr<AndroidContext> android_context_;
  std::shared_ptr<AndroidSurfaceFactoryImpl> surface_factory_;

  PlatformViewAndroidDelegate platform_view_android_delegate_;

  std::unique_ptr<AndroidSurface> android_surface_;
  std::shared_ptr<PlatformMessageHandlerAndroid> platform_message_handler_;
  bool android_meets_hcpp_criteria_ = false;

  fml::WeakPtrFactory<PlatformViewAndroid>
      weak_factory_;  // Must be the last member.

  FML_DISALLOW_COPY_AND_ASSIGN(PlatformViewAndroid);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_H_
