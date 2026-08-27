// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/memory/ref_counted.h"
#include "flutter/fml/task_runner.h"
#include "flutter/shell/platform/android/android_compositor.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_surface_manager.h"
#include "flutter/shell/platform/android/android_thread_config.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

/// Viewport metrics data structure for AndroidEngine, decoupled from
/// //flutter/shell/common and //flutter/lib/ui to preserve GN boundary
/// isolation.
struct AndroidViewportMetrics {
  double device_pixel_ratio = 1.0;
  double physical_width = 0.0;
  double physical_height = 0.0;
  double physical_min_width = 0.0;
  double physical_max_width = 0.0;
  double physical_min_height = 0.0;
  double physical_max_height = 0.0;
  double physical_padding_top = 0.0;
  double physical_padding_right = 0.0;
  double physical_padding_bottom = 0.0;
  double physical_padding_left = 0.0;
  double physical_view_inset_top = 0.0;
  double physical_view_inset_right = 0.0;
  double physical_view_inset_bottom = 0.0;
  double physical_view_inset_left = 0.0;
  double physical_system_gesture_inset_top = 0.0;
  double physical_system_gesture_inset_right = 0.0;
  double physical_system_gesture_inset_bottom = 0.0;
  double physical_system_gesture_inset_left = 0.0;
  double physical_touch_slop = -1.0;
  std::vector<double> physical_display_features_bounds;
  std::vector<int> physical_display_features_type;
  std::vector<int> physical_display_features_state;
  int64_t display_id = 0;
  int64_t view_id = 0;
  double physical_display_corner_radius_top_left = 0.0;
  double physical_display_corner_radius_top_right = 0.0;
  double physical_display_corner_radius_bottom_right = 0.0;
  double physical_display_corner_radius_bottom_left = 0.0;
};

/// Orchestrates engine lifecycle, rendering pipeline, custom task runners,
/// platform message dispatching, semantics, and multi-engine spawning via the
/// public Flutter Embedder C API on Android.
class AndroidEngine {
 public:
  /// Standard production constructor.
  AndroidEngine(
      const flutter::Settings& settings,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      AndroidRenderingAPI rendering_api = AndroidRenderingAPI::kSoftware,
      fml::RefPtr<fml::TaskRunner> platform_task_runner = nullptr,
      fml::RefPtr<fml::TaskRunner> raster_task_runner = nullptr);

  /// Constructor for dependency injection and testing.
  AndroidEngine(const flutter::Settings& settings,
                std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                std::shared_ptr<AndroidSurfaceManager> surface_manager,
                std::unique_ptr<AndroidCompositor> compositor,
                const FlutterEngineProcTable* embedder_api_override = nullptr,
                fml::RefPtr<fml::TaskRunner> platform_task_runner = nullptr,
                fml::RefPtr<fml::TaskRunner> raster_task_runner = nullptr);

  /// Spawning constructor wrapping an already created/spawned FlutterEngine
  /// handle.
  AndroidEngine(FLUTTER_API_SYMBOL(FlutterEngine) engine,
                const flutter::Settings& settings,
                std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                std::shared_ptr<AndroidSurfaceManager> surface_manager,
                std::unique_ptr<AndroidCompositor> compositor,
                const FlutterEngineProcTable& embedder_api);

  ~AndroidEngine();

  /// Returns true if the engine was successfully initialized and is in a valid
  /// state.
  bool IsValid() const;

  /// Returns true if the engine has been launched and is running Dart code.
  bool IsRunning() const;

  /// Returns true if the Embedder API feature flag is enabled.
  bool IsEmbedderAPIEnabled() const;

  /// Returns the configured engine settings.
  const flutter::Settings& GetSettings() const;

  /// Returns the configured rendering API backend.
  AndroidRenderingAPI GetRenderingAPI() const;

  //----------------------------------------------------------------------------
  /// Lifecycle & Launching
  //----------------------------------------------------------------------------

  /// Launches the Flutter application with the specified APK asset provider,
  /// custom entrypoint, library URL, and arguments.
  bool Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
              const std::string& entrypoint,
              const std::string& library_url,
              const std::vector<std::string>& entrypoint_args,
              int64_t engine_id);

  /// Spawns a new engine instance from this running engine, sharing isolate
  /// group, custom task runners, and rendering resources.
  std::unique_ptr<AndroidEngine> Spawn(
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      const std::string& entrypoint,
      const std::string& library_url,
      const std::string& initial_route,
      const std::vector<std::string>& entrypoint_args,
      int64_t engine_id) const;

  //----------------------------------------------------------------------------
  /// Surface Lifecycle & Presentation
  //----------------------------------------------------------------------------

  /// Notifies the engine and compositor that the native rendering surface has
  /// been created.
  void OnSurfaceCreated(fml::RefPtr<AndroidNativeWindow> window);

  /// Notifies the engine and compositor that the underlying native window has
  /// changed.
  void OnSurfaceWindowChanged(fml::RefPtr<AndroidNativeWindow> window);

  /// Notifies the engine and compositor of a surface size change.
  void OnSurfaceResized(size_t width, size_t height);

  /// Synchronous Surface Detachment Barrier:
  /// Blocks the calling thread until rasterization ceases and the native window
  /// handle is released by the compositor and surface manager before Android
  /// destroys ANativeWindow.
  void OnSurfaceDestroyed();

  //----------------------------------------------------------------------------
  /// Viewport Metrics & Display
  //----------------------------------------------------------------------------

  /// Updates viewport metrics for the specified view.
  void SetViewportMetrics(int64_t view_id,
                          const AndroidViewportMetrics& metrics);

  /// Updates viewport metrics from a pre-populated FlutterWindowMetricsEvent.
  void SetViewportMetrics(const FlutterWindowMetricsEvent& event);

  /// Notifies display updates (e.g., refresh rate changes).
  void UpdateDisplayMetrics();

  //----------------------------------------------------------------------------
  /// Pointer Event Dispatching
  //----------------------------------------------------------------------------

  /// Unpacks and dispatches an Android touch packet buffer (36 8-byte fields
  /// per record).
  void DispatchPointerDataPacket(const uint8_t* buffer, size_t size);

  /// Dispatches an array of FlutterPointerEvents directly to the engine.
  void DispatchPointerEvents(const FlutterPointerEvent* events,
                             size_t events_count);

  //----------------------------------------------------------------------------
  /// Platform Messages
  //----------------------------------------------------------------------------

  /// Sends a platform message to Dart with optional response callback ID.
  void SendPlatformMessage(const char* channel,
                           const uint8_t* message,
                           size_t message_size,
                           int32_t response_id);

  /// Sends a response back to a Dart request for the given response ID.
  void SendPlatformMessageResponse(int32_t response_id,
                                   const uint8_t* data,
                                   size_t data_length);

  /// Completes an incoming Dart platform message with an empty response.
  void CompletePlatformMessageEmptyResponse(int32_t response_id);

  /// Handles incoming platform messages from Dart (routed to JNI facade).
  void HandlePlatformMessage(const FlutterPlatformMessage* message);

  /// Handles response from Dart for an outgoing platform message.
  void HandlePlatformMessageResponse(int32_t response_id,
                                     const uint8_t* data,
                                     size_t data_length);

  //----------------------------------------------------------------------------
  /// Semantics & Accessibility
  //----------------------------------------------------------------------------

  /// Enables or disables the accessibility tree.
  void SetSemanticsEnabled(bool enabled);

  /// Updates accessibility feature flags.
  void SetAccessibilityFeatures(int32_t flags);

  /// Dispatches a semantics action to Dart.
  void DispatchSemanticsAction(int32_t id,
                               int32_t action,
                               const uint8_t* data,
                               size_t data_length);

  /// Handles semantics updates received from the Dart framework.
  void HandleSemanticsUpdate(const FlutterSemanticsUpdate2* update);

  //----------------------------------------------------------------------------
  /// Dart Deferred Libraries
  //----------------------------------------------------------------------------

  /// Loads a Dart deferred library with snapshot data and instructions.
  bool LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions);

  /// Reports an error loading a Dart deferred library.
  bool LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string& error_message,
                                    bool transient);

  /// Handles a request from the Dart VM to load a deferred library.
  void HandleRequestDartDeferredLibrary(intptr_t loading_unit_id);

  //----------------------------------------------------------------------------
  /// External Textures & Frame Scheduling
  //----------------------------------------------------------------------------

  /// Registers an external texture identifier with the engine.
  void RegisterExternalTexture(int64_t texture_id);

  /// Registers an ImageTexture with the JNI facade.
  void RegisterImageTexture(int64_t texture_id,
                            JavaLocalRef image_texture_entry);

  /// Unregisters an external texture identifier.
  void UnregisterTexture(int64_t texture_id);

  /// Signals that a new frame is available for an external texture.
  void MarkTextureFrameAvailable(int64_t texture_id);

  /// Schedules a frame to be rendered.
  void ScheduleFrame();

  //----------------------------------------------------------------------------
  /// Low Memory & Screenshots
  //----------------------------------------------------------------------------

  /// Posts a low memory notification to the engine.
  void NotifyLowMemoryWarning();

  /// Captures a screenshot of the current frame.
  bool Screenshot(FlutterEngineScreenshotType type,
                  bool base64_encode,
                  FlutterEngineScreenshotCallback callback,
                  void* user_data);

  //----------------------------------------------------------------------------
  /// Component Getters
  //----------------------------------------------------------------------------

  std::shared_ptr<AndroidSurfaceManager> GetSurfaceManager() const {
    return surface_manager_;
  }

  AndroidCompositor* GetCompositor() const { return compositor_.get(); }

  std::shared_ptr<PlatformViewAndroidJNI> GetJNIFacade() const {
    return jni_facade_;
  }

  FLUTTER_API_SYMBOL(FlutterEngine) GetEngineHandle() const {
    std::lock_guard lock(engine_mutex_);
    return engine_;
  }

  const FlutterEngineProcTable& GetEmbedderAPI() const { return embedder_api_; }

  /// Attaches an already spawned engine handle.
  void AttachSpawnedEngine(FLUTTER_API_SYMBOL(FlutterEngine) engine);

 private:
  struct OutgoingResponseContext {
    std::weak_ptr<PlatformViewAndroidJNI> jni_facade;
    int32_t response_id;
  };

  void InitializeProcTable(const FlutterEngineProcTable* override_table);
  FlutterRendererConfig CreateRendererConfig();

  static void OnPlatformMessageCallback(const FlutterPlatformMessage* message,
                                        void* user_data);
  static void OnRootIsolateCreatedCallback(void* user_data);
  static void OnUpdateSemantics2Callback(const FlutterSemanticsUpdate2* update,
                                         void* user_data);
  static void OnRequestDartDeferredLibraryCallback(intptr_t loading_unit_id,
                                                   void* user_data);

  const flutter::Settings settings_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  const AndroidRenderingAPI rendering_api_;
  const fml::RefPtr<fml::TaskRunner> platform_task_runner_;
  const fml::RefPtr<fml::TaskRunner> raster_task_runner_;

  struct TaskRunnerContext {
    AndroidEngine* engine = nullptr;
    fml::RefPtr<fml::TaskRunner> runner = nullptr;
  };

  TaskRunnerContext platform_runner_context_;
  TaskRunnerContext raster_runner_context_;

  std::shared_ptr<AndroidSurfaceManager> surface_manager_;
  std::unique_ptr<AndroidCompositor> compositor_;
  std::unique_ptr<APKAssetProvider> apk_asset_provider_;

  FlutterEngineProcTable embedder_api_ = {};
  mutable std::mutex engine_mutex_;
  FLUTTER_API_SYMBOL(FlutterEngine) engine_ = nullptr;

  std::atomic<bool> is_valid_{false};
  std::atomic<bool> is_running_{false};
  std::atomic<bool> is_initialized_{false};

  mutable std::mutex pending_responses_mutex_;
  std::atomic<int32_t> next_response_id_{1};
  std::unordered_map<int32_t, const FlutterPlatformMessageResponseHandle*>
      pending_incoming_responses_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidEngine);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
