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

// |PlatformViewAndroid::Delegate|
const Settings& PlatformViewAndroidAdapter::OnPlatformViewGetSettings() const {
  return delegate_.OnPlatformViewGetSettings();
}

std::shared_ptr<fml::BasicTaskRunner>
PlatformViewAndroidAdapter::OnPlatformViewGetShutdownSafeIOTaskRunner() const {
  return delegate_.OnPlatformViewGetShutdownSafeIOTaskRunner();
}

void PlatformViewAndroidAdapter::OnPlatformViewCreated(
    std::unique_ptr<Surface> surface) {
  delegate_.OnPlatformViewCreated(std::move(surface));
}

void PlatformViewAndroidAdapter::OnPlatformViewDestroyed() {
  delegate_.OnPlatformViewDestroyed();
}

void PlatformViewAndroidAdapter::OnPlatformViewScheduleFrame() {
  delegate_.OnPlatformViewScheduleFrame();
}

void PlatformViewAndroidAdapter::OnPlatformViewSetNextFrameCallback(
    const fml::closure& closure) {
  SetNextFrameCallback(closure);
}

void PlatformViewAndroidAdapter::OnPlatformViewSetViewportMetrics(
    int64_t view_id,
    const ViewportMetrics& metrics) {
  SetViewportMetrics(view_id, metrics);
}

void PlatformViewAndroidAdapter::OnPlatformViewDispatchPlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  DispatchPlatformMessage(std::move(message));
}

void PlatformViewAndroidAdapter::OnPlatformViewDispatchPointerDataPacket(
    std::unique_ptr<flutter::PointerDataPacket> packet) {
  DispatchPointerDataPacket(std::move(packet));
}

void PlatformViewAndroidAdapter::OnPlatformViewDispatchSemanticsAction(
    int64_t view_id,
    int32_t node_id,
    flutter::SemanticsAction action,
    fml::MallocMapping args) {
  DispatchSemanticsAction(view_id, node_id, action, std::move(args));
}

void PlatformViewAndroidAdapter::OnPlatformViewSetSemanticsEnabled(
    bool enabled) {
  SetSemanticsEnabled(enabled);
}

void PlatformViewAndroidAdapter::OnPlatformViewSetAccessibilityFeatures(
    int32_t flags) {
  SetAccessibilityFeatures(flags);
}

void PlatformViewAndroidAdapter::OnPlatformViewRegisterTexture(
    std::shared_ptr<flutter::Texture> texture) {
  RegisterTexture(std::move(texture));
}

void PlatformViewAndroidAdapter::OnPlatformViewUnregisterTexture(
    int64_t texture_id) {
  UnregisterTexture(texture_id);
}

void PlatformViewAndroidAdapter::OnPlatformViewMarkTextureFrameAvailable(
    int64_t texture_id) {
  MarkTextureFrameAvailable(texture_id);
}

void PlatformViewAndroidAdapter::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  delegate_.LoadDartDeferredLibrary(loading_unit_id, std::move(snapshot_data),
                                    std::move(snapshot_instructions));
}

void PlatformViewAndroidAdapter::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  delegate_.LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                         transient);
}

void PlatformViewAndroidAdapter::UpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  delegate_.UpdateAssetResolverByType(std::move(updated_asset_resolver), type);
}

// |PlatformView|
void PlatformViewAndroidAdapter::NotifyDestroyed() {
  platform_view_android_->NotifyDestroyed();
}

void PlatformViewAndroidAdapter::SetupImpellerContext() {
  platform_view_android_->SetupImpellerContext();
}

void PlatformViewAndroidAdapter::UpdateSemantics(
    int64_t view_id,
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  platform_view_android_->UpdateSemantics(view_id, std::move(update),
                                          std::move(actions));
}

void PlatformViewAndroidAdapter::SetApplicationLocale(std::string locale) {
  platform_view_android_->SetApplicationLocale(std::move(locale));
}

void PlatformViewAndroidAdapter::SetSemanticsTreeEnabled(bool enabled) {
  platform_view_android_->SetSemanticsTreeEnabled(enabled);
}

void PlatformViewAndroidAdapter::HandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  platform_view_android_->HandlePlatformMessage(std::move(message));
}

void PlatformViewAndroidAdapter::OnPreEngineRestart() const {
  platform_view_android_->OnPreEngineRestart();
}

std::unique_ptr<VsyncWaiter> PlatformViewAndroidAdapter::CreateVSyncWaiter() {
  return platform_view_android_->CreateVSyncWaiter();
}

std::unique_ptr<Surface> PlatformViewAndroidAdapter::CreateRenderingSurface() {
  return platform_view_android_->CreateRenderingSurface();
}

std::shared_ptr<ExternalViewEmbedder>
PlatformViewAndroidAdapter::CreateExternalViewEmbedder() {
  return platform_view_android_->CreateExternalViewEmbedder();
}

std::unique_ptr<SnapshotSurfaceProducer>
PlatformViewAndroidAdapter::CreateSnapshotSurfaceProducer() {
  return platform_view_android_->CreateSnapshotSurfaceProducer();
}

sk_sp<GrDirectContext> PlatformViewAndroidAdapter::CreateResourceContext()
    const {
  return platform_view_android_->CreateResourceContext();
}

void PlatformViewAndroidAdapter::ReleaseResourceContext() const {
  platform_view_android_->ReleaseResourceContext();
}

std::shared_ptr<impeller::Context>
PlatformViewAndroidAdapter::GetImpellerContext() const {
  return platform_view_android_->GetImpellerContext();
}

std::unique_ptr<std::vector<std::string>>
PlatformViewAndroidAdapter::ComputePlatformResolvedLocales(
    const std::vector<std::string>& supported_locale_data) {
  return platform_view_android_->ComputePlatformResolvedLocales(
      supported_locale_data);
}

void PlatformViewAndroidAdapter::RequestDartDeferredLibrary(
    intptr_t loading_unit_id) {
  platform_view_android_->RequestDartDeferredLibrary(loading_unit_id);
}

std::shared_ptr<PlatformMessageHandler>
PlatformViewAndroidAdapter::GetPlatformMessageHandler() const {
  return platform_view_android_->GetPlatformMessageHandler();
}

double PlatformViewAndroidAdapter::GetScaledFontSize(
    double unscaled_font_size,
    int configuration_id) const {
  return platform_view_android_->GetScaledFontSize(unscaled_font_size,
                                                   configuration_id);
}

}  // namespace flutter
