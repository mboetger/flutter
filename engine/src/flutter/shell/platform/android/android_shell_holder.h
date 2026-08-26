// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SHELL_HOLDER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SHELL_HOLDER_H_

#include <memory>
#include <string>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/platform/android/android_engine_bridge.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"

namespace flutter {

class PlatformViewAndroid;
class EmbedderSurfaceAndroid;
class PlatformMessageHandler;
class Shell;

//----------------------------------------------------------------------------
/// @brief      This is the Android owner of the core engine Shell.
///
/// @details    This is the top orchestrator class on the C++ side for the
///             Android embedding. It corresponds to a FlutterEngine on the
///             Java side. This class delegates to an AndroidEngineBridge
///             implementation (e.g. LegacyEngineBridge or
///             EmbedderEngineBridge).
///
///             Technically, the FlutterJNI class owns this AndroidShellHolder
///             class instance, but the FlutterJNI class is meant to be mostly
///             static and has minimal state to perform the C++ pointer <->
///             Java class instance translation.
///
class AndroidShellHolder {
 public:
  AndroidShellHolder(const flutter::Settings& settings,
                     std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                     AndroidRenderingAPI android_rendering_api);

  explicit AndroidShellHolder(std::unique_ptr<AndroidEngineBridge> bridge);

  ~AndroidShellHolder();

  bool IsValid() const;

  std::unique_ptr<AndroidShellHolder> Spawn(
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

  fml::WeakPtr<PlatformViewAndroid> GetPlatformView();

  PlatformViewAndroid* GetPlatformViewAndroid();

  EmbedderSurfaceAndroid* GetEmbedderSurfaceAndroid();

  bool IsSurfaceControlEnabled();

  Rasterizer::Screenshot Screenshot(Rasterizer::ScreenshotType type,
                                    bool base64_encode);

  void NotifyLowMemoryWarning();

  const std::shared_ptr<PlatformMessageHandler>& GetPlatformMessageHandler()
      const;

  void UpdateDisplayMetrics();

  AndroidEngineBridge* GetBridgeForTesting() const { return bridge_.get(); }

  // Visible for testing.
  const std::unique_ptr<Shell>& GetShellForTesting() const;

 private:
  std::unique_ptr<AndroidEngineBridge> bridge_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidShellHolder);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SHELL_HOLDER_H_
