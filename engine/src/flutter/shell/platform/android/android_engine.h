// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/memory/ref_counted.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/shell/platform/android/android_compositor.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_surface_manager.h"
#include "flutter/shell/platform/android/android_task_runners.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/embedder/embedder.h"

#if FML_OS_ANDROID
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#include <jni.h>
#else
typedef void* jobject;
typedef void* JNIEnv;
typedef void* jclass;
typedef void* jmethodID;
typedef int32_t jint;
typedef int64_t jlong;
typedef float jfloat;
typedef uint8_t jboolean;
typedef void* jstring;
typedef void* jintArray;
typedef void* jobjectArray;
#ifndef EGL_NO_IMAGE_KHR
typedef void* EGLImageKHR;
#define EGL_NO_IMAGE_KHR ((EGLImageKHR)0)
#endif
typedef void AHardwareBuffer;
#endif

namespace flutter {

/// @brief Orchestrates the Flutter Embedder C-API for the Android platform.
///
/// AndroidEngine corresponds 1:1 with an active FlutterEngine instance on the
/// Java/JNI side. It owns the C embedder engine handle (`FlutterEngine`), the
/// surface manager, the compositor, the task runners, and mediates all JNI
/// and embedder callbacks.
class AndroidEngine {
 public:
  enum class ScreenshotType {
    kSkiaPicture,
    kUncompressedImage,
    kCompressedImage,
    kSurfaceData,
  };

  struct Screenshot {
    struct FrameSize {
      size_t width = 0;
      size_t height = 0;
    };
    FrameSize frame_size;
    std::unique_ptr<fml::Mapping> data;
  };

  struct ViewportMetrics {
    double device_pixel_ratio = 1.0;
    double physical_width = 0.0;
    double physical_height = 0.0;
    double physical_min_width_constraint = 0.0;
    double physical_max_width_constraint = 0.0;
    double physical_min_height_constraint = 0.0;
    double physical_max_height_constraint = 0.0;
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
    double physical_display_corner_radius_top_left = 0.0;
    double physical_display_corner_radius_top_right = 0.0;
    double physical_display_corner_radius_bottom_right = 0.0;
    double physical_display_corner_radius_bottom_left = 0.0;
  };

  AndroidEngine(const flutter::AndroidSettings& settings,
                std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                AndroidRenderingAPI android_rendering_api);

  virtual ~AndroidEngine();

  /// Returns true if the underlying embedder engine has been initialized and is
  /// valid.
  bool IsValid() const;

  /// Launches the engine with an entrypoint, library URI, entrypoint arguments,
  /// and asset provider.
  bool Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
              const std::string& entrypoint,
              const std::string& library_url,
              const std::vector<std::string>& entrypoint_args,
              int64_t engine_id);

  /// Spawns a child AndroidEngine sharing VM/AOT resources and task runners.
  std::unique_ptr<AndroidEngine> Spawn(
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      const std::string& entrypoint,
      const std::string& library_url,
      const std::string& initial_route,
      const std::vector<std::string>& entrypoint_args,
      int64_t engine_id) const;

  // ---------------------------------------------------------------------------
  // Surface Lifecycle
  // ---------------------------------------------------------------------------
  void NotifySurfaceCreated(ANativeWindow* native_window,
                            bool is_fake_window = false);
  void NotifySurfaceWindowChanged(ANativeWindow* native_window,
                                  bool is_fake_window = false);
  void NotifySurfaceChanged(int width, int height);
  void NotifySurfaceDestroyed();

  // ---------------------------------------------------------------------------
  // Metrics & Display
  // ---------------------------------------------------------------------------
  void SetViewportMetrics(int64_t view_id, const ViewportMetrics& metrics);
  void UpdateDisplayMetrics();
  bool IsSurfaceControlEnabled() const;

  // ---------------------------------------------------------------------------
  // Screenshots
  // ---------------------------------------------------------------------------
  Screenshot Screenshot(ScreenshotType type, bool base64_encode);

  // ---------------------------------------------------------------------------
  // Platform Messages & Events
  // ---------------------------------------------------------------------------
  void DispatchPlatformMessage(JNIEnv* env,
                               const std::string& name,
                               jobject message_data,
                               jint message_position,
                               jint response_id);
  void DispatchEmptyPlatformMessage(JNIEnv* env,
                                    const std::string& name,
                                    jint response_id);
  void DispatchPointerDataPacket(const uint8_t* buffer, size_t position);

  void SendPlatformMessageResponse(int32_t response_id,
                                   std::unique_ptr<fml::Mapping> mapping);
  void SendEmptyPlatformMessageResponse(int32_t response_id);

  // ---------------------------------------------------------------------------
  // Accessibility & Semantics
  // ---------------------------------------------------------------------------
  void SetSemanticsEnabled(bool enabled);
  void SetAccessibilityFeatures(int32_t flags);
  void DispatchSemanticsAction(JNIEnv* env,
                               jint id,
                               jint action,
                               jobject args,
                               jint args_position);

  // ---------------------------------------------------------------------------
  // Textures
  // ---------------------------------------------------------------------------
  void RegisterExternalTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture);
  void RegisterImageTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
      int32_t lifecycle);
  void UnregisterTexture(int64_t texture_id);
  void MarkTextureFrameAvailable(int64_t texture_id);
  void ScheduleFrame();
  bool OnGetGLTexture(int64_t texture_id,
                      size_t width,
                      size_t height,
                      FlutterOpenGLTexture* texture_out);

  // ---------------------------------------------------------------------------
  // Deferred Components
  // ---------------------------------------------------------------------------
  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions);
  void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string& error_message,
                                    bool transient);

  // ---------------------------------------------------------------------------
  // Asset Resolvers & System
  // ---------------------------------------------------------------------------
  void UpdateAssetResolverByType(
      std::unique_ptr<APKAssetProvider> updated_asset_resolver,
      int32_t type);
  void NotifyLowMemoryWarning();

  // ---------------------------------------------------------------------------
  // Getters
  // ---------------------------------------------------------------------------
  const flutter::AndroidSettings& GetSettings() const { return settings_; }
  AndroidRenderingAPI GetRenderingAPI() const { return android_rendering_api_; }
  FLUTTER_API_SYMBOL(FlutterEngine) GetEmbedderEngineHandle() const {
    return engine_;
  }
  std::shared_ptr<AndroidSurfaceManager> GetSurfaceManager() const {
    return surface_manager_;
  }
  std::shared_ptr<AndroidCompositor> GetCompositor() const {
    return compositor_;
  }
  std::shared_ptr<AndroidTaskRunners> GetTaskRunners() const {
    return task_runners_;
  }
  const std::shared_ptr<PlatformViewAndroidJNI>& GetJNI() const {
    return jni_facade_;
  }

  // ---------------------------------------------------------------------------
  // Compositor Delegation Handlers
  // ---------------------------------------------------------------------------
  void OnBeginFrame();
  void OnPlatformViewPresented(int64_t view_id,
                               const FlutterPoint& offset,
                               const FlutterSize& size,
                               size_t mutations_count,
                               const FlutterPlatformViewMutation** mutations);
  void OnFramePresented();

  // ---------------------------------------------------------------------------
  // Conversion & Serialization Helpers (public for direct verification)
  // ---------------------------------------------------------------------------
  static FlutterPointerPhase ToFlutterPointerPhase(int64_t change);
  static FlutterPointerDeviceKind ToFlutterPointerDeviceKind(int64_t kind);
  static FlutterPointerSignalKind ToFlutterPointerSignalKind(
      int64_t signal_kind);
  static std::vector<FlutterPointerEvent> UnpackPointerDataPacket(
      const uint8_t* buffer,
      size_t position);
  static void SerializeSemanticsUpdate(
      const FlutterSemanticsUpdate2* update,
      std::vector<uint8_t>& buffer,
      std::vector<std::string>& strings,
      std::vector<std::vector<uint8_t>>& string_attribute_args,
      std::vector<uint8_t>& actions_buffer,
      std::vector<std::string>& action_strings);

 private:
  class CompositorDelegate;

  // Constructor used for spawned child engines.
  AndroidEngine(const flutter::AndroidSettings& settings,
                std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                std::shared_ptr<AndroidTaskRunners> task_runners,
                AndroidRenderingAPI android_rendering_api);

  static void OnPlatformMessageCallback(const FlutterPlatformMessage* message,
                                        void* user_data);
  static void OnUpdateSemantics2Callback(const FlutterSemanticsUpdate2* update,
                                         void* user_data);
  static void OnDartDeferredLibraryRequestCallback(int64_t loading_unit_id,
                                                   void* user_data);
  static double OnGetScaledFontSizeCallback(double unscaled_font_size,
                                            int configuration_id,
                                            void* user_data);
  static void OnRasterContextSetupCallback(void* user_data);
  static void OnRasterContextTeardownCallback(void* user_data);

  int32_t GenerateNextResponseId();
  const FlutterPlatformMessageResponseHandle* TakePendingResponse(
      int32_t response_id);

  const flutter::AndroidSettings settings_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  const AndroidRenderingAPI android_rendering_api_;

  std::shared_ptr<AndroidTaskRunners> task_runners_;
  std::shared_ptr<AndroidSurfaceManager> surface_manager_;
  std::shared_ptr<CompositorDelegate> compositor_delegate_;
  std::shared_ptr<AndroidCompositor> compositor_;
  std::unique_ptr<APKAssetProvider> apk_asset_provider_;
  FlutterAssetResolver asset_resolver_ = {};
  const FlutterAssetResolver* asset_resolvers_array_[1] = {nullptr};

  FLUTTER_API_SYMBOL(FlutterEngine) engine_ = nullptr;
  bool is_valid_ = false;
  bool surface_attached_ = false;
  std::atomic<bool> first_frame_presented_{false};
  int64_t engine_id_ = 0;

  FlutterRendererConfig renderer_config_ = {};
  FlutterCompositor embedder_compositor_ = {};
  FlutterProjectArgs project_args_ = {};

  mutable std::mutex pending_responses_mutex_;
  int32_t next_response_id_ = 1;
  std::unordered_map<int32_t, const FlutterPlatformMessageResponseHandle*>
      pending_responses_;

  struct TextureRecord {
    enum class Type {
      kSurfaceTexture,
      kImageReader,
    };
    Type type = Type::kSurfaceTexture;
    fml::jni::ScopedJavaGlobalRef<jobject> java_object;
    uint32_t gl_texture_id = 0;
    bool is_attached = false;
    EGLImageKHR egl_image = EGL_NO_IMAGE_KHR;
    AHardwareBuffer* current_ahb = nullptr;
  };

  mutable std::mutex textures_mutex_;
  std::unordered_map<int64_t, std::shared_ptr<TextureRecord>> textures_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidEngine);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
