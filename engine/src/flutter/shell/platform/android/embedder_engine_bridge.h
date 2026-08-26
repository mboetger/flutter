// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_EMBEDDER_ENGINE_BRIDGE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_EMBEDDER_ENGINE_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "flutter/common/constants.h"
#include "flutter/common/settings.h"
#include "flutter/common/task_runners.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/shell/common/platform_view.h"
#include "flutter/shell/platform/android/android_compositor.h"
#include "flutter/shell/platform/android/android_engine_bridge.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_surface_manager.h"
#include "flutter/shell/platform/android/android_thread_priority.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/embedder_surface_android.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

class EmbedderPlatformViewDelegate;

//----------------------------------------------------------------------------
/// @brief      Implementation of AndroidEngineBridge that drives the Flutter
///             Engine using the public ABI-stable C Embedder API (embedder.h).
///
class EmbedderEngineBridge : public AndroidEngineBridge {
 public:
  EmbedderEngineBridge(const flutter::Settings& settings,
                       std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                       AndroidRenderingAPI android_rendering_api);

  ~EmbedderEngineBridge() override;

  // |AndroidEngineBridge|
  bool IsValid() const override;

  // |AndroidEngineBridge|
  std::unique_ptr<AndroidEngineBridge> Spawn(
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::string& initial_route,
      const std::vector<std::string>& entrypoint_args,
      int64_t engine_id) const override;

  // |AndroidEngineBridge|
  void Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
              const std::string& entrypoint,
              const std::string& libraryUrl,
              const std::vector<std::string>& entrypoint_args,
              int64_t engine_id) override;

  // |AndroidEngineBridge|
  const flutter::Settings& GetSettings() const override;

  // |AndroidEngineBridge|
  fml::WeakPtr<PlatformViewAndroid> GetPlatformView() override;

  // |AndroidEngineBridge|
  PlatformViewAndroid* GetPlatformViewAndroid() override;

  // |AndroidEngineBridge|
  EmbedderSurfaceAndroid* GetEmbedderSurfaceAndroid() override;

  // |AndroidEngineBridge|
  bool IsSurfaceControlEnabled() override;

  // |AndroidEngineBridge|
  Rasterizer::Screenshot Screenshot(Rasterizer::ScreenshotType type,
                                    bool base64_encode) override;

  // |AndroidEngineBridge|
  void NotifyLowMemoryWarning() override;

  // |AndroidEngineBridge|
  const std::shared_ptr<PlatformMessageHandler>& GetPlatformMessageHandler()
      const override;

  // |AndroidEngineBridge|
  void UpdateDisplayMetrics() override;

  // |AndroidEngineBridge|
  const std::unique_ptr<Shell>& GetShellForTesting() const override;

  // Returns the underlying FlutterEngine handle (for testing).
  FLUTTER_API_SYMBOL(FlutterEngine) GetEngineHandleForTesting() const {
    return engine_;
  }

  // Returns the AndroidCompositor instance.
  AndroidCompositor* GetAndroidCompositor() const {
    return android_compositor_.get();
  }

  // Returns the AndroidSurfaceManager instance.
  AndroidSurfaceManager* GetAndroidSurfaceManager() const {
    return android_surface_manager_.get();
  }

 private:
  // Constructor for spawned child engine bridges.
  EmbedderEngineBridge(
      const flutter::Settings& settings,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      FLUTTER_API_SYMBOL(FlutterEngine) engine,
      std::unique_ptr<EmbedderPlatformViewDelegate> platform_view_delegate,
      std::unique_ptr<PlatformViewAndroid> platform_view_android,
      std::unique_ptr<EmbedderSurfaceAndroid> embedder_surface,
      std::shared_ptr<AndroidSurfaceManager> surface_manager,
      std::unique_ptr<AndroidCompositor> compositor,
      AndroidRenderingAPI android_rendering_api);

  void InitializePlatformView();
  FlutterRendererConfig CreateRendererConfig();
  FlutterProjectArgs CreateProjectArgs(
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::vector<std::string>& entrypoint_args);

  flutter::Settings settings_;
  std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  AndroidRenderingAPI android_rendering_api_;
  FLUTTER_API_SYMBOL(FlutterEngine) engine_ = nullptr;
  bool is_valid_ = false;

  std::unique_ptr<EmbedderPlatformViewDelegate> platform_view_delegate_;
  std::shared_ptr<AndroidSurfaceManager> android_surface_manager_;
  std::unique_ptr<AndroidCompositor> android_compositor_;
  std::unique_ptr<PlatformViewAndroid> platform_view_android_;
  std::unique_ptr<EmbedderSurfaceAndroid> embedder_surface_;
  std::unique_ptr<APKAssetProvider> apk_asset_provider_;

  FlutterAssetResolver asset_resolver_ = {};
  const FlutterAssetResolver* asset_resolvers_array_[1] = {nullptr};
  FlutterCustomTaskRunners custom_task_runners_ = {};
  FlutterCompositor flutter_compositor_ = {};

  std::vector<std::string> entrypoint_arg_strings_;
  std::vector<const char*> entrypoint_arg_c_strings_;

  FML_DISALLOW_COPY_AND_ASSIGN(EmbedderEngineBridge);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_EMBEDDER_ENGINE_BRIDGE_H_
