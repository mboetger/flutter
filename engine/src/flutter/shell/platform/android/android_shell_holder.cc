// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_shell_holder.h"

#include <memory>
#include <utility>

#include "flutter/shell/common/shell.h"
#include "flutter/shell/platform/android/android_engine_bridge.h"

namespace flutter {

AndroidShellHolder::AndroidShellHolder(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI android_rendering_api)
    : bridge_(AndroidEngineBridge::Create(settings,
                                          std::move(jni_facade),
                                          android_rendering_api)) {}

AndroidShellHolder::AndroidShellHolder(
    std::unique_ptr<AndroidEngineBridge> bridge)
    : bridge_(std::move(bridge)) {}

AndroidShellHolder::~AndroidShellHolder() = default;

bool AndroidShellHolder::IsValid() const {
  return bridge_ && bridge_->IsValid();
}

const flutter::Settings& AndroidShellHolder::GetSettings() const {
  FML_DCHECK(bridge_);
  return bridge_->GetSettings();
}

std::unique_ptr<AndroidShellHolder> AndroidShellHolder::Spawn(
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::string& initial_route,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) const {
  if (!bridge_) {
    return nullptr;
  }
  auto spawned_bridge =
      bridge_->Spawn(std::move(jni_facade), entrypoint, libraryUrl,
                     initial_route, entrypoint_args, engine_id);
  if (!spawned_bridge) {
    return nullptr;
  }
  return std::make_unique<AndroidShellHolder>(std::move(spawned_bridge));
}

void AndroidShellHolder::Launch(
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) {
  if (bridge_) {
    bridge_->Launch(std::move(apk_asset_provider), entrypoint, libraryUrl,
                    entrypoint_args, engine_id);
  }
}

Rasterizer::Screenshot AndroidShellHolder::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  if (!bridge_) {
    return {nullptr, DlISize(), "", Rasterizer::ScreenshotFormat::kUnknown};
  }
  return bridge_->Screenshot(type, base64_encode);
}

fml::WeakPtr<PlatformViewAndroid> AndroidShellHolder::GetPlatformView() {
  if (bridge_) {
    return bridge_->GetPlatformView();
  }
  return fml::WeakPtr<PlatformViewAndroid>();
}

PlatformViewAndroid* AndroidShellHolder::GetPlatformViewAndroid() {
  if (bridge_) {
    return bridge_->GetPlatformViewAndroid();
  }
  return nullptr;
}

EmbedderSurfaceAndroid* AndroidShellHolder::GetEmbedderSurfaceAndroid() {
  if (bridge_) {
    return bridge_->GetEmbedderSurfaceAndroid();
  }
  return nullptr;
}

void AndroidShellHolder::NotifyLowMemoryWarning() {
  if (bridge_) {
    bridge_->NotifyLowMemoryWarning();
  }
}

const std::shared_ptr<PlatformMessageHandler>&
AndroidShellHolder::GetPlatformMessageHandler() const {
  if (bridge_) {
    return bridge_->GetPlatformMessageHandler();
  }
  static const std::shared_ptr<PlatformMessageHandler> kNullHandler = nullptr;
  return kNullHandler;
}

void AndroidShellHolder::UpdateDisplayMetrics() {
  if (bridge_) {
    bridge_->UpdateDisplayMetrics();
  }
}

bool AndroidShellHolder::IsSurfaceControlEnabled() {
  if (bridge_) {
    return bridge_->IsSurfaceControlEnabled();
  }
  return false;
}

const std::unique_ptr<Shell>& AndroidShellHolder::GetShellForTesting() const {
  if (bridge_) {
    return bridge_->GetShellForTesting();
  }
  static const std::unique_ptr<Shell> kNullShell = nullptr;
  return kNullShell;
}

}  // namespace flutter
