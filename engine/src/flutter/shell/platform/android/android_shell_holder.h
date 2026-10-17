// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SHELL_HOLDER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SHELL_HOLDER_H_

#include <memory>
#include <type_traits>

#include "flutter/fml/macros.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_message_handler_android.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/android/vsync_waiter_android.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

/// Viewport metrics reported by `FlutterJNI.setViewportMetrics`, expressed
/// without depending on `flutter/lib/ui/window/viewport_metrics.h` so
/// `AndroidShellHolder` can translate directly into `FlutterWindowMetricsEvent`
/// and call `FlutterEngineProcTable::SendWindowMetricsEvent`.
struct AndroidViewportMetrics {
  double device_pixel_ratio = 1.0;
  double physical_width = 0;
  double physical_height = 0;
  double physical_min_width_constraint = 0;
  double physical_max_width_constraint = 0;
  double physical_min_height_constraint = 0;
  double physical_max_height_constraint = 0;
  double physical_padding_top = 0;
  double physical_padding_right = 0;
  double physical_padding_bottom = 0;
  double physical_padding_left = 0;
  double physical_view_inset_top = 0;
  double physical_view_inset_right = 0;
  double physical_view_inset_bottom = 0;
  double physical_view_inset_left = 0;
  double physical_system_gesture_inset_top = 0;
  double physical_system_gesture_inset_right = 0;
  double physical_system_gesture_inset_bottom = 0;
  double physical_system_gesture_inset_left = 0;
  double physical_touch_slop = -1.0;
  std::vector<double> physical_display_features_bounds;
  std::vector<int> physical_display_features_type;
  std::vector<int> physical_display_features_state;
  size_t display_id = 0;
  double top_left_corner_radius = -1.0;
  double top_right_corner_radius = -1.0;
  double bottom_right_corner_radius = -1.0;
  double bottom_left_corner_radius = -1.0;
};

//----------------------------------------------------------------------------
/// @brief      This is the Android owner of the core engine Shell.
///
/// @details    This is the top orchestrator class on the C++ side for the
///             Android embedding. It corresponds to a FlutterEngine on the
///             Java side. This class is in C++ because the Shell is in
///             C++ and an Android orchestrator needs to exist to
///             compose it with other Android specific C++ components such as
///             the PlatformViewAndroid. This composition of many-to-one
///             C++ components would be difficult to do through JNI whereas
///             a FlutterEngine and AndroidShellHolder has a 1:1 relationship.
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

  ~AndroidShellHolder();

  bool IsValid() const;

  //----------------------------------------------------------------------------
  /// @brief      This is a factory for a derived AndroidShellHolder from an
  ///             existing AndroidShellHolder.
  ///
  /// @details    Creates one Shell from another Shell where the created
  ///             Shell takes the opportunity to share any internal components
  ///             it can. This results is a Shell that has a smaller startup
  ///             time cost and a smaller memory footprint than an Shell created
  ///             with a Create function.
  ///
  ///             The new Shell is returned in a new AndroidShellHolder
  ///             instance.
  ///
  ///             The new Shell's flutter::Settings cannot be changed from that
  ///             of the initial Shell. The RunConfiguration subcomponent can
  ///             be changed however in the spawned Shell to run a different
  ///             entrypoint than the existing shell.
  ///
  ///             Since the AndroidShellHolder both binds downwards to a Shell
  ///             and also upwards to JNI callbacks that the PlatformViewAndroid
  ///             makes, the JNI instance holding this AndroidShellHolder should
  ///             be created first to supply the jni_facade callback.
  ///
  /// @param[in]  jni_facade this argument should be the JNI callback facade of
  ///             a new JNI instance meant to hold this AndroidShellHolder.
  ///
  /// @returns    A new AndroidShellHolder containing a new Shell. Returns
  ///             nullptr when a new Shell can't be created.
  ///
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

  void NotifySurfaceCreated(fml::RefPtr<AndroidNativeWindow> native_window);

  void NotifySurfaceWindowChanged(
      fml::RefPtr<AndroidNativeWindow> native_window);

  void NotifySurfaceChanged(const DlISize& size);

  void NotifySurfaceDestroyed();

  void SetViewportMetrics(int64_t view_id,
                          const AndroidViewportMetrics& metrics);

  void DispatchPlatformMessage(const std::string& name,
                               const void* message_data,
                               int message_position,
                               int response_id);

  void DispatchEmptyPlatformMessage(const std::string& name, int response_id);

  void DispatchPointerDataPacket(const uint8_t* buffer, int size_bytes);

  void InvokePlatformMessageResponseCallback(int response_id,
                                             const uint8_t* data,
                                             size_t size);

  void InvokePlatformMessageEmptyResponseCallback(int response_id);

  void DispatchSemanticsAction(int32_t node_id,
                               int32_t action,
                               const uint8_t* args_data,
                               int32_t args_length);

  void SetSemanticsEnabled(bool enabled);

  void SetAccessibilityFeatures(int32_t flags);

  void RegisterExternalTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture);

  void RegisterImageTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
      ImageExternalTexture::ImageLifecycle lifecycle);

  void UnregisterTexture(int64_t texture_id);

  void MarkTextureFrameAvailable(int64_t texture_id);

  void ScheduleFrame();

  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions);

  void UpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type);

  bool IsSurfaceControlEnabled();

  Rasterizer::Screenshot Screenshot(Rasterizer::ScreenshotType type,
                                    bool base64_encode);

  void NotifyLowMemoryWarning();

  void UpdateDisplayMetrics();

  // Visible for testing.
  Shell* GetShellForTesting() const;

  // Visible for testing. Replacing an entry changes what the holder calls.
  FlutterEngineProcTable& GetEmbedderApiForTesting();

 private:
  // Destroys the engine behind an embedder API handle.
  struct EngineDeleter {
    void operator()(FLUTTER_API_SYMBOL(FlutterEngine) engine) const;
  };
  // Owns an engine through its embedder API handle.
  using UniqueEngine =
      std::unique_ptr<std::remove_pointer_t<FLUTTER_API_SYMBOL(FlutterEngine)>,
                      EngineDeleter>;

  const flutter::Settings settings_;
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  std::unique_ptr<PlatformViewAndroid> platform_view_android_;
  fml::WeakPtr<PlatformViewAndroid> platform_view_;
  std::shared_ptr<PlatformMessageHandlerAndroid> platform_message_handler_;
  std::shared_ptr<ThreadHost> thread_host_;
  UniqueEngine engine_;
  // The embedder API functions that the holder calls engine_ through.
  FlutterEngineProcTable embedder_api_;
  // Answers the vsync callback of the platform view. Detached before engine_
  // is destroyed.
  std::shared_ptr<VsyncWaiterAndroid> vsync_waiter_;
  bool is_valid_ = false;
  uint64_t next_pointer_flow_id_ = 0;
  std::unique_ptr<APKAssetProvider> apk_asset_provider_;
  const AndroidRenderingAPI android_rendering_api_;

  //----------------------------------------------------------------------------
  /// @brief      Constructor with its components injected.
  ///
  /// @details    This is similar to the standard constructor, except its
  ///             members were constructed elsewhere and injected.
  ///
  ///             All injected components must be non-null and valid.
  ///
  ///             Used when constructing the Shell from the inside out when
  ///             spawning from an existing Shell.
  ///
  AndroidShellHolder(
      const flutter::Settings& settings,
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
      const std::shared_ptr<ThreadHost>& thread_host,
      UniqueEngine engine,
      std::unique_ptr<APKAssetProvider> apk_asset_provider,
      std::unique_ptr<PlatformViewAndroid> platform_view_android,
      const fml::WeakPtr<PlatformViewAndroid>& platform_view,
      std::shared_ptr<PlatformMessageHandlerAndroid> platform_message_handler,
      std::shared_ptr<VsyncWaiterAndroid> vsync_waiter,
      AndroidRenderingAPI rendering_api);
  static void ThreadDestructCallback(void* value);
  std::optional<RunConfiguration> BuildRunConfiguration(
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::vector<std::string>& entrypoint_args) const;

  bool IsNDKImageDecoderAvailable();

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidShellHolder);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SHELL_HOLDER_H_
