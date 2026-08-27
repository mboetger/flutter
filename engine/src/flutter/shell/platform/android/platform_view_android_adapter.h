// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_ADAPTER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/shell/common/platform_view.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_view_android.h"

namespace flutter {

class PlatformViewAndroidAdapter : public PlatformView,
                                   public PlatformViewAndroid::Delegate {
 public:
  PlatformViewAndroidAdapter(
      PlatformView::Delegate& delegate,
      const flutter::TaskRunners& task_runners,
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
      AndroidRenderingAPI rendering_api);

  PlatformViewAndroidAdapter(
      PlatformView::Delegate& delegate,
      const flutter::TaskRunners& task_runners,
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
      const std::shared_ptr<flutter::AndroidContext>& android_context);

  ~PlatformViewAndroidAdapter() override;

  PlatformViewAndroid* GetPlatformViewAndroid() const {
    return platform_view_android_.get();
  }

  // |PlatformView|
  void NotifyDestroyed() override;

  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions) override;

  void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string error_message,
                                    bool transient) override;

  void UpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type) override;

  std::shared_ptr<PlatformMessageHandler> GetPlatformMessageHandler()
      const override;

  void SetupImpellerContext() override;

  // |PlatformViewAndroid::Delegate|
  const Settings& OnPlatformViewGetSettings() const override;

  std::shared_ptr<fml::BasicTaskRunner>
  OnPlatformViewGetShutdownSafeIOTaskRunner() const override;

  void OnPlatformViewCreated() override;

  void OnPlatformViewDestroyed() override;

  void OnPlatformViewScheduleFrame() override;

  void OnPlatformViewDispatchPlatformMessage(
      std::unique_ptr<flutter::PlatformMessage> message) override;

  void OnPlatformViewDispatchSemanticsAction(int64_t view_id,
                                             int32_t id,
                                             flutter::SemanticsAction action,
                                             fml::MallocMapping args) override;

  void OnPlatformViewSetViewportMetrics(
      int64_t view_id,
      const ViewportMetrics& metrics) override;

  void OnPlatformViewDispatchPointerDataPacket(
      std::unique_ptr<PointerDataPacket> packet) override;

  void OnPlatformViewSetSemanticsEnabled(bool enabled) override;

  void OnPlatformViewSetAccessibilityFeatures(int32_t flags) override;

  void OnPlatformViewRegisterTexture(
      std::shared_ptr<flutter::Texture> texture) override;

  void OnPlatformViewUnregisterTexture(int64_t texture_id) override;

  void OnPlatformViewMarkTextureFrameAvailable(int64_t texture_id) override;

  void OnPlatformViewSetNextFrameCallback(const fml::closure& closure) override;

  void OnPlatformViewLoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions) override;

  void OnPlatformViewLoadDartDeferredLibraryError(
      intptr_t loading_unit_id,
      const std::string error_message,
      bool transient) override;

  void OnPlatformViewUpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type) override;

 private:
  std::unique_ptr<PlatformViewAndroid> platform_view_android_;

  // |PlatformView| overrides:
  void UpdateSemantics(
      int64_t view_id,
      flutter::SemanticsNodeUpdates update,
      flutter::CustomAccessibilityActionUpdates actions) override;

  void SetApplicationLocale(std::string locale) override;

  void SetSemanticsTreeEnabled(bool enabled) override;

  void HandlePlatformMessage(
      std::unique_ptr<flutter::PlatformMessage> message) override;

  void OnPreEngineRestart() const override;

  std::unique_ptr<VsyncWaiter> CreateVSyncWaiter() override;

  std::unique_ptr<Surface> CreateRenderingSurface() override;

  std::shared_ptr<ExternalViewEmbedder> CreateExternalViewEmbedder() override;

  std::unique_ptr<SnapshotSurfaceProducer> CreateSnapshotSurfaceProducer()
      override;

  sk_sp<GrDirectContext> CreateResourceContext() const override;

  void ReleaseResourceContext() const override;

  std::shared_ptr<impeller::Context> GetImpellerContext() const override;

  std::unique_ptr<std::vector<std::string>> ComputePlatformResolvedLocales(
      const std::vector<std::string>& supported_locale_data) override;

  void RequestDartDeferredLibrary(intptr_t loading_unit_id) override;

  double GetScaledFontSize(double unscaled_font_size,
                           int configuration_id) const override;

  FML_DISALLOW_COPY_AND_ASSIGN(PlatformViewAndroidAdapter);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_ADAPTER_H_
