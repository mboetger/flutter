// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_LEGACY_ENGINE_BRIDGE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_LEGACY_ENGINE_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/fml/macros.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/android_engine_bridge.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/embedder_surface_android.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_view_android.h"

namespace flutter {

//----------------------------------------------------------------------------
/// @brief      Legacy implementation of AndroidEngineBridge wrapping
///             flutter::Shell.
///
class LegacyEngineBridge : public AndroidEngineBridge {
 public:
  LegacyEngineBridge(const flutter::Settings& settings,
                     std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                     AndroidRenderingAPI android_rendering_api);

  LegacyEngineBridge(const flutter::Settings& settings,
                     const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
                     const std::shared_ptr<ThreadHost>& thread_host,
                     std::unique_ptr<Shell> shell,
                     std::unique_ptr<APKAssetProvider> apk_asset_provider,
                     std::unique_ptr<PlatformViewAndroid> platform_view_android,
                     EmbedderSurfaceAndroid* embedder_surface,
                     AndroidRenderingAPI rendering_api);

  ~LegacyEngineBridge() override;

  bool IsValid() const override;

  std::unique_ptr<AndroidEngineBridge> Spawn(
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::string& initial_route,
      const std::vector<std::string>& entrypoint_args,
      int64_t engine_id) const override;

  void Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
              const std::string& entrypoint,
              const std::string& libraryUrl,
              const std::vector<std::string>& entrypoint_args,
              int64_t engine_id) override;

  const flutter::Settings& GetSettings() const override;

  fml::WeakPtr<PlatformViewAndroid> GetPlatformView() override;

  PlatformViewAndroid* GetPlatformViewAndroid() override;

  EmbedderSurfaceAndroid* GetEmbedderSurfaceAndroid() override;

  bool IsSurfaceControlEnabled() override;

  Rasterizer::Screenshot Screenshot(Rasterizer::ScreenshotType type,
                                    bool base64_encode) override;

  void NotifyLowMemoryWarning() override;

  const std::shared_ptr<PlatformMessageHandler>& GetPlatformMessageHandler()
      const override;

  void UpdateDisplayMetrics() override;

  const std::unique_ptr<Shell>& GetShellForTesting() const override {
    return shell_;
  }

 private:
  std::optional<RunConfiguration> BuildRunConfiguration(
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::vector<std::string>& entrypoint_args) const;

  const flutter::Settings settings_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  std::unique_ptr<PlatformViewAndroid> platform_view_android_;
  fml::WeakPtr<PlatformViewAndroid> platform_view_;
  EmbedderSurfaceAndroid* embedder_surface_ = nullptr;
  std::shared_ptr<ThreadHost> thread_host_;
  std::unique_ptr<Shell> shell_;
  bool is_valid_ = false;
  std::unique_ptr<APKAssetProvider> apk_asset_provider_;
  const AndroidRenderingAPI android_rendering_api_;

  FML_DISALLOW_COPY_AND_ASSIGN(LegacyEngineBridge);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_LEGACY_ENGINE_BRIDGE_H_
