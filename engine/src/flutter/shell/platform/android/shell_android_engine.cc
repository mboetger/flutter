// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/shell_android_engine.h"

namespace flutter {

ShellAndroidEngine::ShellAndroidEngine(std::unique_ptr<Shell> shell)
    : shell_(std::move(shell)) {
  FML_DCHECK(shell_);
}

ShellAndroidEngine::~ShellAndroidEngine() = default;

bool ShellAndroidEngine::IsValid() const {
  return shell_ != nullptr;
}

bool ShellAndroidEngine::IsSetup() const {
  return shell_ && shell_->IsSetup();
}

void ShellAndroidEngine::RunEngine(RunConfiguration run_configuration) {
  FML_DCHECK(shell_);
  shell_->RunEngine(std::move(run_configuration));
}

std::unique_ptr<AndroidEngine> ShellAndroidEngine::Spawn(
    RunConfiguration run_configuration,
    const std::string& initial_route,
    Shell::CreateCallback<PlatformView> on_create_platform_view,
    Shell::CreateCallback<Rasterizer> on_create_rasterizer) const {
  FML_DCHECK(shell_);
  auto spawned_shell =
      shell_->Spawn(std::move(run_configuration), initial_route,
                    on_create_platform_view, on_create_rasterizer);
  if (!spawned_shell) {
    return nullptr;
  }
  return std::make_unique<ShellAndroidEngine>(std::move(spawned_shell));
}

Rasterizer::Screenshot ShellAndroidEngine::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  if (!shell_) {
    return {nullptr, DlISize(), "", Rasterizer::ScreenshotFormat::kUnknown};
  }
  return shell_->Screenshot(type, base64_encode);
}

void ShellAndroidEngine::NotifyLowMemoryWarning() {
  if (shell_) {
    shell_->NotifyLowMemoryWarning();
  }
}

void ShellAndroidEngine::OnDisplayUpdates(
    std::vector<std::unique_ptr<Display>> displays) {
  if (shell_) {
    shell_->OnDisplayUpdates(std::move(displays));
  }
}

std::shared_ptr<PlatformMessageHandler>
ShellAndroidEngine::GetPlatformMessageHandler() const {
  if (!shell_) {
    return nullptr;
  }
  return shell_->GetPlatformMessageHandler();
}

void ShellAndroidEngine::RegisterImageDecoder(ImageGeneratorFactory factory,
                                              int32_t priority) {
  if (shell_) {
    shell_->RegisterImageDecoder(std::move(factory), priority);
  }
}

const TaskRunners& ShellAndroidEngine::GetTaskRunners() const {
  FML_CHECK(shell_);
  return shell_->GetTaskRunners();
}

Shell& ShellAndroidEngine::GetShell() {
  FML_CHECK(shell_);
  return *shell_;
}

}  // namespace flutter
