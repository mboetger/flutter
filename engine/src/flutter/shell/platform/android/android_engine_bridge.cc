// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_engine_bridge.h"

#include <memory>
#include <utility>

#include "flutter/shell/common/shell.h"
#include "flutter/shell/platform/android/embedder_engine_bridge.h"

namespace flutter {

std::unique_ptr<AndroidEngineBridge> AndroidEngineBridge::Create(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI android_rendering_api) {
  return std::make_unique<EmbedderEngineBridge>(settings, std::move(jni_facade),
                                                android_rendering_api);
}

const std::unique_ptr<Shell>& AndroidEngineBridge::GetShellForTesting() const {
  static const std::unique_ptr<Shell> kNullShell = nullptr;
  return kNullShell;
}

}  // namespace flutter
