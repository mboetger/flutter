// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/android_compositor.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

//------------------------------------------------------------------------------
/// @brief      Top-level C++ orchestrator for the Flutter Android platform
///             embedding via the Embedder C API (`embedder.h`).
///
/// @details    `AndroidEngine` owns the `FlutterEngine` handle and the
///             `FlutterEngineProcTable`. It implements
///             `PlatformViewAndroid::Delegate` to bridge `PlatformViewAndroid`,
///             `AndroidCompositor`, and Android system events with the Embedder
///             API.
///
class AndroidEngine : public PlatformViewAndroid::Delegate {
 public:
  AndroidEngine(const flutter::Settings& settings,
                std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                AndroidRenderingAPI android_rendering_api);

  ~AndroidEngine() override;

  bool IsValid() const;

  std::unique_ptr<AndroidEngine> Spawn(
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

  std::shared_ptr<AndroidCompositor> GetAndroidCompositor() const;

  bool IsSurfaceControlEnabled();

  void NotifyLowMemoryWarning();

  void UpdateDisplayMetrics();

  FLUTTER_API_SYMBOL(FlutterEngine) GetEmbedderEngine() const {
    return engine_;
  }
  const FlutterEngineProcTable& GetProcTable() const { return embedder_api_; }

  // |PlatformViewAndroid::Delegate|
  const Settings& OnPlatformViewGetSettings() const override;
  std::shared_ptr<fml::BasicTaskRunner>
  OnPlatformViewGetShutdownSafeIOTaskRunner() const override;
  void OnPlatformViewCreated(std::unique_ptr<Surface> surface) override;
  void OnPlatformViewDestroyed() override;
  void OnPlatformViewScheduleFrame() override;
  void OnPlatformViewSetNextFrameCallback(const fml::closure& closure) override;
  void OnPlatformViewSetViewportMetrics(
      int64_t view_id,
      const ViewportMetrics& metrics) override;
  void OnPlatformViewDispatchPlatformMessage(
      std::unique_ptr<flutter::PlatformMessage> message) override;
  void OnPlatformViewDispatchPointerDataPacket(
      std::unique_ptr<flutter::PointerDataPacket> packet) override;
  void OnPlatformViewDispatchSemanticsAction(int64_t view_id,
                                             int32_t node_id,
                                             flutter::SemanticsAction action,
                                             fml::MallocMapping args) override;
  void OnPlatformViewSetSemanticsEnabled(bool enabled) override;
  void OnPlatformViewSetAccessibilityFeatures(int32_t flags) override;
  void OnPlatformViewRegisterTexture(
      std::shared_ptr<flutter::Texture> texture) override;
  void OnPlatformViewUnregisterTexture(int64_t texture_id) override;
  void OnPlatformViewMarkTextureFrameAvailable(int64_t texture_id) override;
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

  struct TaskRunnerContext {
    fml::RefPtr<fml::TaskRunner> platform_runner;
    fml::RefPtr<fml::TaskRunner> ui_runner;
    fml::RefPtr<fml::TaskRunner> raster_runner;
    fml::RefPtr<fml::TaskRunner> io_runner;
    FlutterEngineProcTable embedder_api = {};
    std::atomic<FLUTTER_API_SYMBOL(FlutterEngine)> engine{nullptr};
  };

  struct TaskRunnerHandler {
    std::shared_ptr<TaskRunnerContext> context;
    fml::RefPtr<fml::TaskRunner> runner;
  };

 private:
  AndroidEngine(const flutter::Settings& settings,
                const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
                const std::shared_ptr<ThreadHost>& thread_host,
                const TaskRunners& task_runners,
                FLUTTER_API_SYMBOL(FlutterEngine) engine,
                std::unique_ptr<APKAssetProvider> apk_asset_provider,
                AndroidRenderingAPI rendering_api,
                std::shared_ptr<AndroidContext> android_context);

  void InitializeTaskRunners();
  void SetupEmbedderProcTable();
  bool InitializeEngine();

  const flutter::Settings settings_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  const AndroidRenderingAPI android_rendering_api_;

  FlutterEngineProcTable embedder_api_ = {};
  FLUTTER_API_SYMBOL(FlutterEngine) engine_ = nullptr;
  bool is_valid_ = false;

  std::shared_ptr<ThreadHost> thread_host_;
  std::optional<TaskRunners> task_runners_;
  std::shared_ptr<TaskRunnerContext> task_runner_context_;

  TaskRunnerHandler platform_handler_;
  TaskRunnerHandler ui_handler_;
  TaskRunnerHandler raster_handler_;
  TaskRunnerHandler io_handler_;

  FlutterTaskRunnerDescription platform_task_runner_description_ = {};
  FlutterTaskRunnerDescription ui_task_runner_description_ = {};
  FlutterTaskRunnerDescription raster_task_runner_description_ = {};
  FlutterTaskRunnerDescription io_task_runner_description_ = {};
  FlutterCustomTaskRunners custom_task_runners_ = {};

  std::unique_ptr<PlatformViewAndroid> platform_view_;
  std::shared_ptr<AndroidCompositor> compositor_;
  std::unique_ptr<APKAssetProvider> apk_asset_provider_;
  std::unordered_map<int64_t, std::shared_ptr<flutter::Texture>>
      external_textures_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidEngine);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
