// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_ENGINE_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_ENGINE_H_

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/runtime/dart_service_isolate.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/embedder/embedder_external_texture_resolver.h"
#include "flutter/shell/platform/embedder/embedder_impeller_device_context.h"
#include "flutter/shell/platform/embedder/embedder_thread_host.h"
#include "flutter/shell/platform/embedder/platform_view_embedder.h"
namespace flutter {

struct ShellArgs;
class AndroidRenderer;

// The object that is returned to the embedder as an opaque pointer to the
// instance of the Flutter engine.
class EmbedderEngine {
 public:
  // Creates the PlatformView for a Shell given only the PlatformView::Delegate
  // view of that Shell and its task runners, so callers do not need the full
  // Shell definition.
  using PlatformViewFactory =
      std::function<std::unique_ptr<PlatformView>(PlatformView::Delegate&,
                                                  const TaskRunners&)>;

  //----------------------------------------------------------------------------
  /// @param[in]  thread_host  The threads and custom task runners of the
  ///             engine. May be null when the embedder owns the threads behind
  ///             |task_runners|; RunTask() then fails.
  /// @param[in]  run_configuration  What RunRootIsolate() runs. May be
  ///             std::nullopt when the embedder starts the root isolate some
  ///             other way; RunRootIsolate() then fails.
  /// @param[in]  external_texture_resolver  May be null; RegisterTexture()
  ///             then fails.
  ///
  EmbedderEngine(
      std::unique_ptr<EmbedderThreadHost> thread_host,
      const TaskRunners& task_runners,
      const Settings& settings,
      std::optional<RunConfiguration> run_configuration,
      const Shell::CreateCallback<PlatformView>& on_create_platform_view,
      const Shell::CreateCallback<Rasterizer>& on_create_rasterizer,
      std::unique_ptr<EmbedderExternalTextureResolver>
          external_texture_resolver,
      bool defer_surface_creation = false,
      const PlatformData& platform_data = PlatformData());

  // Same as above, with |on_create_platform_view| taking a
  // PlatformView::Delegate& instead of a Shell&, and a default Rasterizer.
  EmbedderEngine(std::unique_ptr<EmbedderThreadHost> thread_host,
                 const TaskRunners& task_runners,
                 const Settings& settings,
                 std::optional<RunConfiguration> run_configuration,
                 const PlatformViewFactory& on_create_platform_view,
                 std::unique_ptr<EmbedderExternalTextureResolver>
                     external_texture_resolver,
                 bool defer_surface_creation = false,
                 const PlatformData& platform_data = PlatformData());

  ~EmbedderEngine();

  bool LaunchShell();

  //----------------------------------------------------------------------------
  /// @brief      Same as LaunchShell(), with |platform_data| as the initial
  ///             state of ui.PlatformDispatcher instead of the defaults.
  ///
  bool LaunchShell(const PlatformData& platform_data);

  //----------------------------------------------------------------------------
  /// @brief      Creates an engine from this one with Shell::Spawn. Must be
  ///             called on the platform thread.
  ///
  ///             The new engine shares this engine's task runners, and its
  ///             root isolate is already running |run_configuration| when this
  ///             returns. It owns no threads, run configuration or texture
  ///             resolver, so RunTask(), RunRootIsolate() and
  ///             RegisterTexture() fail on it. It does not keep this engine
  ///             alive, and it must be destroyed before the threads behind the
  ///             shared task runners are.
  ///
  /// @return     The new engine, or nullptr if this engine has no shell or the
  ///             spawn failed.
  ///
  std::unique_ptr<EmbedderEngine> Spawn(
      RunConfiguration run_configuration,
      const std::string& initial_route,
      const Shell::CreateCallback<PlatformView>& on_create_platform_view,
      const Shell::CreateCallback<Rasterizer>& on_create_rasterizer,
      std::unique_ptr<EmbedderExternalTextureResolver>
          external_texture_resolver = nullptr,
      bool defer_surface_creation = false) const;

  // Same as above, with |on_create_platform_view| taking a
  // PlatformView::Delegate& instead of a Shell&, and a default Rasterizer.
  std::unique_ptr<EmbedderEngine> Spawn(
      RunConfiguration run_configuration,
      const std::string& initial_route,
      const PlatformViewFactory& on_create_platform_view,
      std::unique_ptr<EmbedderExternalTextureResolver>
          external_texture_resolver = nullptr,
      bool defer_surface_creation = false) const;

  // Same as above, building the RunConfiguration from |config|. Returns
  // nullptr if |config| is invalid.
  std::unique_ptr<EmbedderEngine> Spawn(
      const FlutterEngineRunConfiguration* config,
      const std::string& initial_route,
      const Shell::CreateCallback<PlatformView>& on_create_platform_view,
      const Shell::CreateCallback<Rasterizer>& on_create_rasterizer,
      std::unique_ptr<EmbedderExternalTextureResolver>
          external_texture_resolver = nullptr,
      bool defer_surface_creation = false,
      std::string* error_out = nullptr) const;

  std::unique_ptr<EmbedderEngine> Spawn(
      const FlutterEngineRunConfiguration* config,
      const std::string& initial_route,
      const PlatformViewFactory& on_create_platform_view,
      std::unique_ptr<EmbedderExternalTextureResolver>
          external_texture_resolver = nullptr,
      bool defer_surface_creation = false) const;

  // Builds a RunConfiguration from |config|, or returns std::nullopt if
  // |config| is null, malformed, or its kernel blob cannot be mapped.
  static std::optional<RunConfiguration> BuildRunConfiguration(
      const FlutterEngineRunConfiguration* config,
      std::string* error_out = nullptr);

  bool CollectShell();

  void CollectThreadHost();

  const TaskRunners& GetTaskRunners() const;

  bool NotifyCreated();

  bool NotifyDestroyed();

  bool IsSurfaceCreated() const;

  bool DeferSurfaceCreation() const;

  bool RunRootIsolate();

  bool LaunchEngine(RunConfiguration run_configuration);

  bool UpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type);

  bool IsValid() const;

  bool SetViewportMetrics(int64_t view_id,
                          const flutter::ViewportMetrics& metrics);

  bool DispatchPointerDataPacket(
      std::unique_ptr<flutter::PointerDataPacket> packet);

  bool SendPlatformMessage(std::unique_ptr<PlatformMessage> message);

  bool RegisterTexture(int64_t texture);

  void RegisterExternalTexture(std::shared_ptr<Texture> texture);

  EmbedderImpellerDeviceContextGetter GetImpellerDeviceContextGetter() const;

  bool UnregisterTexture(int64_t texture);

  bool MarkTextureFrameAvailable(int64_t texture);

  bool SetSemanticsEnabled(bool enabled);

  bool SetAccessibilityFeatures(int32_t flags);

  bool DispatchSemanticsAction(int64_t view_id,
                               int node_id,
                               flutter::SemanticsAction action,
                               fml::MallocMapping args);

  bool OnVsyncEvent(intptr_t baton,
                    fml::TimePoint frame_start_time,
                    fml::TimePoint frame_target_time);

  bool ReloadSystemFonts();

  bool PostRenderThreadTask(const fml::closure& task);

  bool RunTask(const FlutterTask* task);

  bool PostTaskOnEngineManagedNativeThreads(
      const std::function<void(FlutterNativeThreadType)>& closure) const;

  bool ScheduleFrame();

  bool LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions);

  bool LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string& error_message,
                                    bool transient);

  void AddImageDecoder(ImageGeneratorFactory factory, int32_t priority);

  bool RegisterImageGenerator(ImageGeneratorFactory factory, int32_t priority);

  void SetVMServiceUriCallback(
      std::function<void(const std::string&)> callback);

  Rasterizer::Screenshot Screenshot(Rasterizer::ScreenshotType type,
                                    bool base64_encode);

  Shell& GetShell();

#if FML_OS_ANDROID
  void SetAndroidRenderer(std::shared_ptr<AndroidRenderer> renderer);

  const std::shared_ptr<AndroidRenderer>& GetAndroidRenderer() const;

  const std::shared_ptr<std::shared_ptr<AndroidRenderer>>&
  GetAndroidRendererSlot() const {
    return android_renderer_slot_;
  }

  void SetAndroidRendererSlot(
      std::shared_ptr<std::shared_ptr<AndroidRenderer>> slot) {
    android_renderer_slot_ = std::move(slot);
  }
#endif  // FML_OS_ANDROID

 private:
  // Adopts |spawned_shell|, a running shell returned by Shell::Spawn.
  EmbedderEngine(std::shared_ptr<EmbedderThreadHost> thread_host,
                 const TaskRunners& task_runners,
                 std::unique_ptr<Shell> spawned_shell,
                 std::unique_ptr<EmbedderExternalTextureResolver>
                     external_texture_resolver,
                 bool defer_surface_creation);

  void RegisterVMServiceUriCallbackIfNeeded();
  void ClearVMServiceUriCallback();

  std::shared_ptr<EmbedderThreadHost> thread_host_;
  TaskRunners task_runners_;
  std::optional<RunConfiguration> run_configuration_;
  std::unique_ptr<ShellArgs> shell_args_;
  std::unique_ptr<Shell> shell_;
  std::unique_ptr<EmbedderExternalTextureResolver> external_texture_resolver_;
#if FML_OS_ANDROID
  std::shared_ptr<std::shared_ptr<AndroidRenderer>> android_renderer_slot_ =
      std::make_shared<std::shared_ptr<AndroidRenderer>>();
#endif  // FML_OS_ANDROID
  bool defer_surface_creation_ = false;
  bool is_surface_created_ = false;
  std::vector<std::pair<ImageGeneratorFactory, int32_t>>
      pending_image_decoders_;
  std::function<void(const std::string&)> vm_service_uri_callback_;
  std::shared_ptr<std::atomic<bool>> vm_service_callback_alive_;
  DartServiceIsolate::CallbackHandle vm_service_uri_callback_handle_ = 0;

  FML_DISALLOW_COPY_AND_ASSIGN(EmbedderEngine);
};

//------------------------------------------------------------------------------
/// @brief      Builds the `PlatformViewEmbedder` platform-message callback
///             that wraps each incoming `PlatformMessage` in a
///             `FlutterPlatformMessageResponseHandle` and invokes `callback`.
///
///             Defined in `embedder.cc`. Shared with in-process embedders that
///             construct `PlatformViewEmbedder` directly but want to receive
///             messages through the C-API `FlutterPlatformMessage` contract.
///
PlatformViewEmbedder::PlatformMessageResponseCallback
MakePlatformMessageResponseCallback(FlutterPlatformMessageCallback callback,
                                    void* user_data);

//------------------------------------------------------------------------------
/// @brief      Builds the `PlatformViewEmbedder` semantics-update callback
///             that translates engine semantics node and custom-action updates
///             into a `FlutterSemanticsUpdate2` and invokes `callback`.
///
///             Defined in `embedder.cc`. Shared with in-process embedders that
///             construct `PlatformViewEmbedder` directly but want to receive
///             semantics updates through the C-API `FlutterSemanticsUpdate2`
///             contract.
///
PlatformViewEmbedder::UpdateSemanticsCallback MakeSemanticsUpdateCallback(
    FlutterUpdateSemanticsCallback2 callback,
    void* user_data);

//------------------------------------------------------------------------------
/// @brief      Builds the `ImageGeneratorFactory` that wraps a
///             `FlutterImageGeneratorRegistration` in an
///             `EmbedderCustomImageGenerator`.
///
///             Defined in `embedder.cc`. Shared with in-process embedders that
///             register custom image decoders through the C-API
///             `FlutterImageGeneratorRegistration` contract.
///
ImageGeneratorFactory MakeImageGeneratorFactory(
    const FlutterImageGeneratorRegistration& registration);

//------------------------------------------------------------------------------
/// @brief      Builds an `AssetResolver` of type `kApkAssetProvider` from a
///             `FlutterCustomAssetResolver` descriptor. Defined in
///             `embedder.cc`.
///
std::unique_ptr<AssetResolver> CreateEmbedderCustomAssetResolver(
    const FlutterCustomAssetResolver* desc);

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_ENGINE_H_
