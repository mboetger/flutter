// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/fml/task_runner.h"
#include "flutter/shell/platform/android/android_compositor.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_surface_manager.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

//------------------------------------------------------------------------------
/// @brief      Top-level C++ orchestrator for the Flutter Android embedding
///             powered entirely by the public C Embedder API (`embedder.h`).
///
///             `AndroidEngine` manages engine lifecycle, thread affinity,
///             surface presentation (via `AndroidCompositor`), asset resolution
///             (via `APKAssetProvider`), JNI bridge routing (via
///             `PlatformViewAndroidJNI`), and multi-engine spawning.
///
class AndroidEngine {
 public:
  AndroidEngine(const Settings& settings,
                std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                AndroidRenderingAPI rendering_api,
                fml::RefPtr<fml::TaskRunner> platform_task_runner = nullptr,
                fml::RefPtr<fml::TaskRunner> raster_task_runner = nullptr,
                fml::RefPtr<fml::TaskRunner> ui_task_runner = nullptr,
                fml::RefPtr<fml::TaskRunner> io_task_runner = nullptr);

  ~AndroidEngine();

  // Returns true if the engine proc table and internal resources are valid.
  bool IsValid() const;

  // Returns true if the engine has been launched and is actively running.
  bool IsRunning() const;

  // Launches the engine executing the specified Dart entrypoint and assets.
  bool Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
              const std::string& entrypoint,
              const std::string& library_url,
              const std::vector<std::string>& entrypoint_args,
              int64_t engine_id);

  // Spawns a lightweight sibling engine sharing the Dart VM and task runners.
  std::unique_ptr<AndroidEngine> Spawn(
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      const std::string& entrypoint,
      const std::string& library_url,
      const std::string& initial_route,
      const std::vector<std::string>& entrypoint_args,
      int64_t engine_id) const;

  // Surface management.
  void OnSurfaceCreated(fml::RefPtr<AndroidNativeWindow> native_window);
  void OnSurfaceWindowChanged(fml::RefPtr<AndroidNativeWindow> native_window);
  void OnSurfaceDestroyed();

  // Viewport metrics and display updates.
  void SetViewportMetrics(const FlutterWindowMetricsEvent& event);
  void UpdateDisplayMetrics();

  // Pointer input events.
  void DispatchPointerDataPacket(const uint8_t* data, size_t size);
  void DispatchPointerEvents(const FlutterPointerEvent* events, size_t count);

  // Platform messaging.
  void SendPlatformMessage(const char* channel,
                           const uint8_t* message,
                           size_t message_size,
                           int32_t response_id);
  void SendPlatformMessageResponse(int32_t response_id,
                                   const uint8_t* data,
                                   size_t data_size);

  // Semantics and accessibility.
  void SetSemanticsEnabled(bool enabled);
  void SetAccessibilityFeatures(int32_t flags);
  void DispatchSemanticsAction(int64_t view_id,
                               int32_t node_id,
                               FlutterSemanticsAction action,
                               const uint8_t* data,
                               size_t data_size);

  // External textures.
  bool RegisterExternalTexture(int64_t texture_id);
  bool UnregisterExternalTexture(int64_t texture_id);
  bool MarkExternalTextureFrameAvailable(int64_t texture_id);

  // Deferred libraries.
  bool LoadDartDeferredLibrary(intptr_t loading_unit_id,
                               const uint8_t* snapshot_data,
                               size_t snapshot_data_size,
                               const uint8_t* snapshot_instructions,
                               size_t snapshot_instructions_size);
  bool LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const char* error_message,
                                    bool transient);

  // Screenshots.
  FlutterEngineScreenshot Screenshot(FlutterEngineScreenshotType type,
                                     bool base64_encode);
  void ReleaseScreenshot(const FlutterEngineScreenshot* screenshot);

  // Vsync.
  void OnVsync(intptr_t baton,
               uint64_t frame_start_time_nanos,
               uint64_t frame_target_time_nanos);

  // Lifecycle & memory.
  void NotifyLowMemoryWarning();

  // Accessors.
  const Settings& GetSettings() const { return settings_; }
  AndroidRenderingAPI GetRenderingAPI() const { return rendering_api_; }
  std::shared_ptr<AndroidSurfaceManager> GetSurfaceManager() const {
    return surface_manager_;
  }
  AndroidCompositor* GetCompositor() const { return compositor_.get(); }
  FLUTTER_API_SYMBOL(FlutterEngine) GetEngineHandle() const { return engine_; }
  const FlutterEngineProcTable& GetProcTable() const { return embedder_api_; }

  // Expose semantics handler for unit test verification.
  void HandleSemanticsUpdate2ForTesting(const FlutterSemanticsUpdate2* update) {
    HandleSemanticsUpdate2(update);
  }

 private:
  struct TaskRunnerContext {
    fml::TaskRunner* runner = nullptr;
    std::shared_ptr<std::atomic<FLUTTER_API_SYMBOL(FlutterEngine)>>
        engine_holder;
  };

  // Constructor for spawned engine instances.
  AndroidEngine(const Settings& settings,
                std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                AndroidRenderingAPI rendering_api,
                FLUTTER_API_SYMBOL(FlutterEngine) engine_handle,
                std::shared_ptr<AndroidSurfaceManager> surface_manager,
                std::unique_ptr<AndroidCompositor> compositor,
                fml::RefPtr<fml::TaskRunner> platform_task_runner,
                fml::RefPtr<fml::TaskRunner> raster_task_runner,
                fml::RefPtr<fml::TaskRunner> ui_task_runner,
                fml::RefPtr<fml::TaskRunner> io_task_runner);

  // Internal C Embedder API callbacks.
  static void OnPlatformMessageThunk(const FlutterPlatformMessage* message,
                                     void* user_data);
  static void OnVsyncThunk(void* user_data, intptr_t baton);
  static void OnUpdateSemanticsThunk2(const FlutterSemanticsUpdate2* update,
                                      void* user_data);
  static void OnRequestDartDeferredLibraryThunk(intptr_t loading_unit_id,
                                                void* user_data);
  static void OnPreEngineRestartThunk(void* user_data);

  void HandlePlatformMessage(const FlutterPlatformMessage* message);
  void HandleVsyncRequest(intptr_t baton);
  void HandleSemanticsUpdate2(const FlutterSemanticsUpdate2* update);
  void HandleRequestDartDeferredLibrary(intptr_t loading_unit_id);
  void HandlePreEngineRestart();

  const Settings settings_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  const AndroidRenderingAPI rendering_api_;

  fml::RefPtr<fml::TaskRunner> platform_task_runner_;
  fml::RefPtr<fml::TaskRunner> raster_task_runner_;
  fml::RefPtr<fml::TaskRunner> ui_task_runner_;
  fml::RefPtr<fml::TaskRunner> io_task_runner_;

  std::shared_ptr<AndroidSurfaceManager> surface_manager_;
  std::unique_ptr<AndroidCompositor> compositor_;
  std::unique_ptr<APKAssetProvider> apk_asset_provider_;

  FlutterEngineProcTable embedder_api_ = {};
  FLUTTER_API_SYMBOL(FlutterEngine) engine_ = nullptr;
  std::shared_ptr<std::atomic<FLUTTER_API_SYMBOL(FlutterEngine)>>
      engine_handle_holder_ =
          std::make_shared<std::atomic<FLUTTER_API_SYMBOL(FlutterEngine)>>(
              nullptr);
  bool is_spawned_ = false;

  std::atomic<int32_t> next_response_id_ = 1;
  std::unordered_map<int32_t, const FlutterPlatformMessageResponseHandle*>
      pending_responses_;
  std::mutex pending_responses_mutex_;

  TaskRunnerContext platform_runner_ctx_;
  TaskRunnerContext render_runner_ctx_;
  TaskRunnerContext ui_runner_ctx_;

  FlutterTaskRunnerDescription platform_runner_desc_ = {};
  FlutterTaskRunnerDescription render_runner_desc_ = {};
  FlutterTaskRunnerDescription ui_runner_desc_ = {};
  FlutterCustomTaskRunners custom_task_runners_ = {};

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidEngine);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
