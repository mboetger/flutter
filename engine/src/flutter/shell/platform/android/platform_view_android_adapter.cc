// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android_adapter.h"

#include <utility>

namespace flutter {

PlatformViewAndroidAdapter::PlatformViewAndroidAdapter(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    AndroidRenderingAPI rendering_api)
    : PlatformView(delegate, task_runners),
      platform_view_android_(
          std::make_unique<PlatformViewAndroid>(*this,
                                                task_runners,
                                                jni_facade,
                                                rendering_api)) {}

PlatformViewAndroidAdapter::PlatformViewAndroidAdapter(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<flutter::AndroidContext>& android_context)
    : PlatformView(delegate, task_runners),
      platform_view_android_(
          std::make_unique<PlatformViewAndroid>(*this,
                                                task_runners,
                                                jni_facade,
                                                android_context)) {}

PlatformViewAndroidAdapter::~PlatformViewAndroidAdapter() = default;

void PlatformViewAndroidAdapter::NotifyDestroyed() {
  if (platform_view_android_) {
    platform_view_android_->NotifyDestroyed();
  }
}

void PlatformViewAndroidAdapter::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  if (platform_view_android_) {
    platform_view_android_->LoadDartDeferredLibrary(
        loading_unit_id, std::move(snapshot_data),
        std::move(snapshot_instructions));
  }
}

void PlatformViewAndroidAdapter::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  if (platform_view_android_) {
    platform_view_android_->LoadDartDeferredLibraryError(
        loading_unit_id, error_message, transient);
  }
}

void PlatformViewAndroidAdapter::UpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  if (platform_view_android_) {
    platform_view_android_->UpdateAssetResolverByType(
        std::move(updated_asset_resolver), type);
  }
}

std::shared_ptr<PlatformMessageHandler>
PlatformViewAndroidAdapter::GetPlatformMessageHandler() const {
  if (platform_view_android_) {
    return platform_view_android_->GetPlatformMessageHandler();
  }
  return nullptr;
}

void PlatformViewAndroidAdapter::SetupImpellerContext() {
  if (platform_view_android_) {
    platform_view_android_->SetupImpellerContext();
  }
}

// |PlatformViewAndroid::Delegate|
const Settings& PlatformViewAndroidAdapter::OnPlatformViewGetSettings() const {
  return delegate_.OnPlatformViewGetSettings();
}

std::shared_ptr<fml::BasicTaskRunner>
PlatformViewAndroidAdapter::OnPlatformViewGetShutdownSafeIOTaskRunner() const {
  return delegate_.OnPlatformViewGetShutdownSafeIOTaskRunner();
}

void PlatformViewAndroidAdapter::OnPlatformViewCreated() {
  PlatformView::NotifyCreated();
}

void PlatformViewAndroidAdapter::OnPlatformViewDestroyed() {
  PlatformView::NotifyDestroyed();
}

void PlatformViewAndroidAdapter::OnPlatformViewScheduleFrame() {
  PlatformView::ScheduleFrame();
}

void PlatformViewAndroidAdapter::OnPlatformViewDispatchPlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  PlatformView::DispatchPlatformMessage(std::move(message));
}

void PlatformViewAndroidAdapter::OnPlatformViewDispatchSemanticsAction(
    int64_t view_id,
    int32_t id,
    flutter::SemanticsAction action,
    fml::MallocMapping args) {
  PlatformView::DispatchSemanticsAction(view_id, id, action, std::move(args));
}

void PlatformViewAndroidAdapter::OnPlatformViewSetViewportMetrics(
    int64_t view_id,
    const ViewportMetrics& metrics) {
  PlatformView::SetViewportMetrics(view_id, metrics);
}

void PlatformViewAndroidAdapter::OnPlatformViewDispatchPointerDataPacket(
    std::unique_ptr<PointerDataPacket> packet) {
  PlatformView::DispatchPointerDataPacket(std::move(packet));
}

void PlatformViewAndroidAdapter::OnPlatformViewSetSemanticsEnabled(
    bool enabled) {
  PlatformView::SetSemanticsEnabled(enabled);
}

void PlatformViewAndroidAdapter::OnPlatformViewSetAccessibilityFeatures(
    int32_t flags) {
  PlatformView::SetAccessibilityFeatures(flags);
}

void PlatformViewAndroidAdapter::OnPlatformViewRegisterTexture(
    std::shared_ptr<flutter::Texture> texture) {
  PlatformView::RegisterTexture(std::move(texture));
}

void PlatformViewAndroidAdapter::OnPlatformViewUnregisterTexture(
    int64_t texture_id) {
  PlatformView::UnregisterTexture(texture_id);
}

void PlatformViewAndroidAdapter::OnPlatformViewMarkTextureFrameAvailable(
    int64_t texture_id) {
  PlatformView::MarkTextureFrameAvailable(texture_id);
}

void PlatformViewAndroidAdapter::OnPlatformViewSetNextFrameCallback(
    const fml::closure& closure) {
  PlatformView::SetNextFrameCallback(closure);
}

void PlatformViewAndroidAdapter::OnPlatformViewLoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  delegate_.LoadDartDeferredLibrary(loading_unit_id, std::move(snapshot_data),
                                    std::move(snapshot_instructions));
}

void PlatformViewAndroidAdapter::OnPlatformViewLoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  delegate_.LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                         transient);
}

void PlatformViewAndroidAdapter::OnPlatformViewUpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  delegate_.UpdateAssetResolverByType(std::move(updated_asset_resolver), type);
}

// |PlatformView| overrides:
void PlatformViewAndroidAdapter::UpdateSemantics(
    int64_t view_id,
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  if (platform_view_android_) {
    platform_view_android_->UpdateSemantics(view_id, std::move(update),
                                            std::move(actions));
  }
}

void PlatformViewAndroidAdapter::SetApplicationLocale(std::string locale) {
  if (platform_view_android_) {
    platform_view_android_->SetApplicationLocale(std::move(locale));
  }
}

void PlatformViewAndroidAdapter::SetSemanticsTreeEnabled(bool enabled) {
  if (platform_view_android_) {
    platform_view_android_->SetSemanticsTreeEnabled(enabled);
  }
}

void PlatformViewAndroidAdapter::HandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  if (platform_view_android_) {
    platform_view_android_->HandlePlatformMessage(std::move(message));
  }
}

void PlatformViewAndroidAdapter::OnPreEngineRestart() const {
  if (platform_view_android_) {
    platform_view_android_->OnPreEngineRestart();
  }
}

std::unique_ptr<VsyncWaiter> PlatformViewAndroidAdapter::CreateVSyncWaiter() {
  if (platform_view_android_) {
    return platform_view_android_->CreateVSyncWaiter();
  }
  return nullptr;
}

std::unique_ptr<Surface> PlatformViewAndroidAdapter::CreateRenderingSurface() {
  if (platform_view_android_) {
    return platform_view_android_->CreateRenderingSurface();
  }
  return nullptr;
}

std::shared_ptr<ExternalViewEmbedder>
PlatformViewAndroidAdapter::CreateExternalViewEmbedder() {
  if (platform_view_android_) {
    return platform_view_android_->CreateExternalViewEmbedder();
  }
  return nullptr;
}

std::unique_ptr<SnapshotSurfaceProducer>
PlatformViewAndroidAdapter::CreateSnapshotSurfaceProducer() {
  if (platform_view_android_) {
    return platform_view_android_->CreateSnapshotSurfaceProducer();
  }
  return nullptr;
}

sk_sp<GrDirectContext> PlatformViewAndroidAdapter::CreateResourceContext()
    const {
  if (platform_view_android_) {
    return platform_view_android_->CreateResourceContext();
  }
  return nullptr;
}

void PlatformViewAndroidAdapter::ReleaseResourceContext() const {
  if (platform_view_android_) {
    platform_view_android_->ReleaseResourceContext();
  }
}

std::shared_ptr<impeller::Context>
PlatformViewAndroidAdapter::GetImpellerContext() const {
  if (platform_view_android_) {
    return platform_view_android_->GetImpellerContext();
  }
  return nullptr;
}

std::unique_ptr<std::vector<std::string>>
PlatformViewAndroidAdapter::ComputePlatformResolvedLocales(
    const std::vector<std::string>& supported_locale_data) {
  if (platform_view_android_) {
    return platform_view_android_->ComputePlatformResolvedLocales(
        supported_locale_data);
  }
  return nullptr;
}

void PlatformViewAndroidAdapter::RequestDartDeferredLibrary(
    intptr_t loading_unit_id) {
  if (platform_view_android_) {
    platform_view_android_->RequestDartDeferredLibrary(loading_unit_id);
  }
}

double PlatformViewAndroidAdapter::GetScaledFontSize(
    double unscaled_font_size,
    int configuration_id) const {
  if (platform_view_android_) {
    return platform_view_android_->GetScaledFontSize(unscaled_font_size,
                                                     configuration_id);
  }
  return unscaled_font_size;
}

}  // namespace flutter
