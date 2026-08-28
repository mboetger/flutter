// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_ADAPTER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/shell/common/platform_view.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_view_android.h"

namespace flutter {

//------------------------------------------------------------------------------
/// @brief      Adapts the standalone `PlatformViewAndroid` to the legacy
///             internal `flutter::PlatformView` interface required by `Shell`.
///
class PlatformViewAndroidAdapter final : public PlatformView,
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

  // |PlatformViewAndroid::Delegate|
  const flutter::TaskRunners& GetTaskRunners() const override {
    return task_runners_;
  }

  const flutter::Settings& OnPlatformViewGetSettings() const override {
    return delegate_.OnPlatformViewGetSettings();
  }

  std::shared_ptr<fml::BasicTaskRunner>
  OnPlatformViewGetShutdownSafeIOTaskRunner() const override {
    return delegate_.OnPlatformViewGetShutdownSafeIOTaskRunner();
  }

  void OnPlatformViewCreated(std::unique_ptr<Surface> surface) override {
    delegate_.OnPlatformViewCreated(std::move(surface));
  }

  void OnPlatformViewDestroyed() override {
    delegate_.OnPlatformViewDestroyed();
  }

  void OnPlatformViewScheduleFrame() override {
    delegate_.OnPlatformViewScheduleFrame();
  }

  void OnPlatformViewSetViewportMetrics(
      int64_t view_id,
      const flutter::ViewportMetrics& metrics) override {
    SetViewportMetrics(view_id, metrics);
  }

  void OnPlatformViewDispatchPointerDataPacket(
      std::unique_ptr<flutter::PointerDataPacket> packet) override {
    DispatchPointerDataPacket(std::move(packet));
  }

  void SetSemanticsEnabled(bool enabled) override {
    PlatformView::SetSemanticsEnabled(enabled);
  }

  void SetAccessibilityFeatures(int32_t flags) override {
    PlatformView::SetAccessibilityFeatures(flags);
  }

  void OnPlatformViewDispatchPlatformMessage(
      std::unique_ptr<flutter::PlatformMessage> message) override {
    PlatformView::DispatchPlatformMessage(std::move(message));
  }

  void OnPlatformViewSemanticsAction(int64_t view_id,
                                     int32_t node_id,
                                     flutter::SemanticsAction action,
                                     fml::MallocMapping args) override {
    PlatformView::DispatchSemanticsAction(view_id, node_id, action,
                                          std::move(args));
  }

  void OnPlatformViewRegisterTexture(
      std::shared_ptr<flutter::Texture> texture) override {
    RegisterTexture(std::move(texture));
  }

  void OnPlatformViewUnregisterTexture(int64_t texture_id) override {
    UnregisterTexture(texture_id);
  }

  void OnPlatformViewMarkTextureFrameAvailable(int64_t texture_id) override {
    MarkTextureFrameAvailable(texture_id);
  }

  void SetNextFrameCallback(const fml::closure& closure) override {
    PlatformView::SetNextFrameCallback(closure);
  }

  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions) override {
    delegate_.LoadDartDeferredLibrary(loading_unit_id, std::move(snapshot_data),
                                      std::move(snapshot_instructions));
  }

  void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string error_message,
                                    bool transient) override {
    delegate_.LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                           transient);
  }

  void UpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type) override {
    delegate_.UpdateAssetResolverByType(std::move(updated_asset_resolver),
                                        type);
  }

  // |PlatformView| overrides:
  void NotifyDestroyed() override {
    if (platform_view_android_) {
      platform_view_android_->NotifyDestroyed();
    }
  }

  void UpdateSemantics(
      int64_t view_id,
      flutter::SemanticsNodeUpdates update,
      flutter::CustomAccessibilityActionUpdates actions) override {
    if (platform_view_android_) {
      platform_view_android_->UpdateSemantics(view_id, std::move(update),
                                              std::move(actions));
    }
  }

  void SetApplicationLocale(std::string locale) override {
    if (platform_view_android_) {
      platform_view_android_->SetApplicationLocale(std::move(locale));
    }
  }

  void SetSemanticsTreeEnabled(bool enabled) override {
    if (platform_view_android_) {
      platform_view_android_->SetSemanticsTreeEnabled(enabled);
    }
  }

  void HandlePlatformMessage(
      std::unique_ptr<flutter::PlatformMessage> message) override {
    if (platform_view_android_) {
      platform_view_android_->HandlePlatformMessage(std::move(message));
    }
  }

  void OnPreEngineRestart() const override {
    if (platform_view_android_) {
      platform_view_android_->OnPreEngineRestart();
    }
  }

  std::unique_ptr<VsyncWaiter> CreateVSyncWaiter() override {
    return platform_view_android_ ? platform_view_android_->CreateVSyncWaiter()
                                  : nullptr;
  }

  std::unique_ptr<Surface> CreateRenderingSurface() override {
    return platform_view_android_
               ? platform_view_android_->CreateRenderingSurface()
               : nullptr;
  }

  std::shared_ptr<ExternalViewEmbedder> CreateExternalViewEmbedder() override {
    return platform_view_android_
               ? platform_view_android_->CreateExternalViewEmbedder()
               : nullptr;
  }

  std::unique_ptr<SnapshotSurfaceProducer> CreateSnapshotSurfaceProducer()
      override {
    return platform_view_android_
               ? platform_view_android_->CreateSnapshotSurfaceProducer()
               : nullptr;
  }

  sk_sp<GrDirectContext> CreateResourceContext() const override {
    return platform_view_android_
               ? platform_view_android_->CreateResourceContext()
               : nullptr;
  }

  void ReleaseResourceContext() const override {
    if (platform_view_android_) {
      platform_view_android_->ReleaseResourceContext();
    }
  }

  std::shared_ptr<impeller::Context> GetImpellerContext() const override {
    return platform_view_android_ ? platform_view_android_->GetImpellerContext()
                                  : nullptr;
  }

  std::unique_ptr<std::vector<std::string>> ComputePlatformResolvedLocales(
      const std::vector<std::string>& supported_locale_data) override {
    return platform_view_android_
               ? platform_view_android_->ComputePlatformResolvedLocales(
                     supported_locale_data)
               : nullptr;
  }

  void RequestDartDeferredLibrary(intptr_t loading_unit_id) override {
    if (platform_view_android_) {
      platform_view_android_->RequestDartDeferredLibrary(loading_unit_id);
    }
  }

  double GetScaledFontSize(double unscaled_font_size,
                           int configuration_id) const override {
    return platform_view_android_ ? platform_view_android_->GetScaledFontSize(
                                        unscaled_font_size, configuration_id)
                                  : unscaled_font_size;
  }

  void SetupImpellerContext() override {
    if (platform_view_android_) {
      platform_view_android_->SetupImpellerContext();
    }
  }

  std::shared_ptr<PlatformMessageHandler> GetPlatformMessageHandler()
      const override {
    return platform_view_android_
               ? platform_view_android_->GetPlatformMessageHandler()
               : nullptr;
  }

 private:
  std::unique_ptr<PlatformViewAndroid> platform_view_android_;

  FML_DISALLOW_COPY_AND_ASSIGN(PlatformViewAndroidAdapter);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_ADAPTER_H_
