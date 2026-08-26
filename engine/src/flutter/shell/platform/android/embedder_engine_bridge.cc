// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/embedder_engine_bridge.h"

#include <utility>

#include "flutter/common/constants.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/mapping.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/platform/android/android_display.h"
#include "flutter/shell/platform/android/android_image_generator.h"
#include "flutter/shell/platform/android/platform_message_response_android.h"
#include "flutter/shell/platform/embedder/embedder_engine.h"
#include "flutter/shell/platform/embedder/platform_view_embedder.h"

namespace flutter {

// PlatformView::Delegate implementation for EmbedderEngineBridge.
class EmbedderPlatformViewDelegate final : public PlatformView::Delegate {
 public:
  explicit EmbedderPlatformViewDelegate(const Settings& settings)
      : settings_(settings) {}

  ~EmbedderPlatformViewDelegate() = default;

  void OnPlatformViewCreated(std::unique_ptr<Surface> surface) override {}
  void OnPlatformViewDestroyed() override {}
  void OnPlatformViewScheduleFrame() override {}
  void OnPlatformViewAddView(int64_t view_id,
                             const ViewportMetrics& viewport_metrics,
                             AddViewCallback callback) override {
    if (callback) {
      callback(true);
    }
  }
  void OnPlatformViewRemoveView(int64_t view_id,
                                RemoveViewCallback callback) override {
    if (callback) {
      callback(true);
    }
  }
  void OnPlatformViewSendViewFocusEvent(const ViewFocusEvent& event) override {}
  void OnPlatformViewSetNextFrameCallback(
      const fml::closure& closure) override {}
  void OnPlatformViewSetViewportMetrics(
      int64_t view_id,
      const ViewportMetrics& metrics) override {}
  void OnPlatformViewDispatchPlatformMessage(
      std::unique_ptr<PlatformMessage> message) override {}
  void OnPlatformViewDispatchPointerDataPacket(
      std::unique_ptr<PointerDataPacket> packet) override {}
  HitTestResponse OnPlatformViewHitTest(
      int64_t view_id,
      const flutter::PointData offset) override {
    return HitTestResponse();
  }
  void OnPlatformViewDispatchSemanticsAction(int64_t view_id,
                                             int32_t node_id,
                                             SemanticsAction action,
                                             fml::MallocMapping args) override {
  }
  void OnPlatformViewSetSemanticsEnabled(bool enabled) override {}
  void OnPlatformViewSetAccessibilityFeatures(int32_t flags) override {}
  void OnPlatformViewRegisterTexture(
      std::shared_ptr<Texture> texture) override {}
  void OnPlatformViewUnregisterTexture(int64_t texture_id) override {}
  void OnPlatformViewMarkTextureFrameAvailable(int64_t texture_id) override {}
  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions) override {}
  void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string error_message,
                                    bool transient) override {}
  void UpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type) override {}
  const Settings& OnPlatformViewGetSettings() const override {
    return settings_;
  }
  std::shared_ptr<fml::BasicTaskRunner>
  OnPlatformViewGetShutdownSafeIOTaskRunner() const override {
    return nullptr;
  }

 private:
  const Settings& settings_;
};

// Callback that resolves assets from the APKAssetProvider via the public
// FlutterAssetResolver Embedder API.
static bool EmbedderAssetResolverGetAsset(const char* asset_name,
                                          FlutterMapping* mapping_out,
                                          void* user_data) {
  if (!user_data || !asset_name || !mapping_out) {
    return false;
  }
  auto* provider = static_cast<APKAssetProvider*>(user_data);
  auto mapping = provider->GetImpl()->GetAsMapping(asset_name);
  if (!mapping) {
    return false;
  }
  mapping_out->struct_size = sizeof(FlutterMapping);
  mapping_out->mapping = mapping->GetMapping();
  mapping_out->size = mapping->GetSize();
  mapping_out->user_data = mapping.release();
  mapping_out->release_callback = nullptr;
  return true;
}

EmbedderEngineBridge::EmbedderEngineBridge(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      android_rendering_api_(android_rendering_api) {
  FML_DCHECK(jni_facade_);
  InitializePlatformView();
}

EmbedderEngineBridge::EmbedderEngineBridge(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
    std::unique_ptr<EmbedderPlatformViewDelegate> platform_view_delegate,
    std::unique_ptr<PlatformViewAndroid> platform_view_android,
    std::unique_ptr<EmbedderSurfaceAndroid> embedder_surface,
    std::shared_ptr<AndroidSurfaceManager> surface_manager,
    std::unique_ptr<AndroidCompositor> compositor,
    AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      android_rendering_api_(android_rendering_api),
      engine_(engine),
      is_valid_(engine != nullptr),
      platform_view_delegate_(std::move(platform_view_delegate)),
      android_surface_manager_(std::move(surface_manager)),
      android_compositor_(std::move(compositor)),
      platform_view_android_(std::move(platform_view_android)),
      embedder_surface_(std::move(embedder_surface)) {
  FML_DCHECK(jni_facade_);
}

EmbedderEngineBridge::~EmbedderEngineBridge() {
  if (engine_) {
    FlutterEngineShutdown(engine_);
    engine_ = nullptr;
  }
  platform_view_android_.reset();
  embedder_surface_.reset();
  android_compositor_.reset();
  android_surface_manager_.reset();
  platform_view_delegate_.reset();
}

void EmbedderEngineBridge::InitializePlatformView() {
  auto context_settings = PlatformViewAndroid::CreateContextSettings(settings_);
  auto android_context = PlatformViewAndroid::CreateAndroidContext(
      TaskRunners("", nullptr, nullptr, nullptr, nullptr),
      android_rendering_api_, settings_.enable_opengl_gpu_tracing,
      context_settings, nullptr);

  bool enable_impeller =
      android_rendering_api_ == AndroidRenderingAPI::kImpellerOpenGLES ||
      android_rendering_api_ == AndroidRenderingAPI::kImpellerVulkan ||
      android_rendering_api_ == AndroidRenderingAPI::kImpellerAutoselect;

  embedder_surface_ = std::make_unique<EmbedderSurfaceAndroid>(
      android_context, enable_impeller, false);

  platform_view_delegate_ =
      std::make_unique<EmbedderPlatformViewDelegate>(settings_);

  platform_view_android_ = std::make_unique<PlatformViewAndroid>(
      *platform_view_delegate_,
      TaskRunners("", nullptr, nullptr, nullptr, nullptr), jni_facade_,
      android_context, embedder_surface_.get());

  android_surface_manager_ =
      std::make_shared<AndroidSurfaceManager>(android_context);

  android_compositor_ = std::make_unique<AndroidCompositor>(
      android_surface_manager_, jni_facade_);

  android_compositor_->AddView(kFlutterImplicitViewId);
}

FlutterRendererConfig EmbedderEngineBridge::CreateRendererConfig() {
  FlutterRendererConfig config = {};
  switch (android_rendering_api_) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware: {
      config.type = kSoftware;
      config.software.struct_size = sizeof(FlutterSoftwareRendererConfig);
      config.software.surface_present_callback =
          [](void* user_data, const void* allocation, size_t row_bytes,
             size_t height) -> bool { return true; };
      break;
    }
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerVulkan: {
      config.type = kVulkan;
      config.vulkan.struct_size = sizeof(FlutterVulkanRendererConfig);
      config.vulkan.version = VK_MAKE_VERSION(1, 1, 0);
      config.vulkan.instance = VK_NULL_HANDLE;
      config.vulkan.physical_device = VK_NULL_HANDLE;
      config.vulkan.device = VK_NULL_HANDLE;
      config.vulkan.queue_family_index = 0;
      config.vulkan.queue = VK_NULL_HANDLE;
      config.vulkan.get_instance_proc_address_callback =
          [](void*, FlutterVulkanInstanceHandle, const char*) -> void* {
        return nullptr;
      };
      config.vulkan.get_next_image_callback =
          [](void*, const FlutterFrameInfo*) -> FlutterVulkanImage {
        return {};
      };
      config.vulkan.present_image_callback =
          [](void*, const FlutterVulkanImage*) -> bool { return true; };
      break;
    }
#if !SLIMPELLER
    case AndroidRenderingAPI::kSkiaOpenGLES:
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
    case AndroidRenderingAPI::kImpellerAutoselect:
    default: {
      config.type = kOpenGL;
      config.open_gl.struct_size = sizeof(FlutterOpenGLRendererConfig);
      config.open_gl.make_current = [](void* user_data) -> bool {
        return true;
      };
      config.open_gl.clear_current = [](void* user_data) -> bool {
        return true;
      };
      config.open_gl.fbo_callback = [](void* user_data) -> uint32_t {
        return 0;
      };
      config.open_gl.present = [](void* user_data) -> bool { return true; };
      break;
    }
  }
  return config;
}

FlutterProjectArgs EmbedderEngineBridge::CreateProjectArgs(
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::vector<std::string>& entrypoint_args) {
  FlutterProjectArgs args = {};
  args.struct_size = sizeof(FlutterProjectArgs);
  args.assets_path = "";
  args.icu_data_path = "";

  if (!entrypoint.empty()) {
    args.custom_dart_entrypoint = entrypoint.c_str();
  }

  entrypoint_arg_strings_ = entrypoint_args;
  entrypoint_arg_c_strings_.clear();
  for (const auto& arg : entrypoint_arg_strings_) {
    entrypoint_arg_c_strings_.push_back(arg.c_str());
  }
  if (!entrypoint_arg_c_strings_.empty()) {
    args.dart_entrypoint_argc =
        static_cast<int>(entrypoint_arg_c_strings_.size());
    args.dart_entrypoint_argv = entrypoint_arg_c_strings_.data();
  }

  custom_task_runners_ = CreateAndroidCustomTaskRunners();
  args.custom_task_runners = &custom_task_runners_;

  if (android_compositor_) {
    flutter_compositor_ = android_compositor_->GetCompositor();
    args.compositor = &flutter_compositor_;
  }

  if (apk_asset_provider_) {
    asset_resolver_.struct_size = sizeof(FlutterAssetResolver);
    asset_resolver_.user_data = apk_asset_provider_.get();
    asset_resolver_.get_asset_callback = EmbedderAssetResolverGetAsset;
    asset_resolvers_array_[0] = &asset_resolver_;
    args.asset_resolvers = asset_resolvers_array_;
    args.asset_resolvers_count = 1;
  }

  args.vsync_callback = [](void* user_data, intptr_t baton) {
    auto* self = static_cast<EmbedderEngineBridge*>(user_data);
    if (self && self->platform_view_android_) {
      self->platform_view_android_->OnVsyncCallback(baton);
    }
  };

  args.dart_deferred_library_request_callback = [](intptr_t loading_unit_id,
                                                   void* user_data) {
    auto* self = static_cast<EmbedderEngineBridge*>(user_data);
    if (self && self->platform_view_android_) {
      self->platform_view_android_->RequestDartDeferredLibrary(loading_unit_id);
    }
  };

  return args;
}

bool EmbedderEngineBridge::IsValid() const {
  return is_valid_ && engine_ != nullptr;
}

void EmbedderEngineBridge::Launch(
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) {
  if (engine_) {
    FML_LOG(WARNING) << "Engine is already launched.";
    return;
  }

  apk_asset_provider_ = std::move(apk_asset_provider);
  FlutterRendererConfig config = CreateRendererConfig();
  FlutterProjectArgs args =
      CreateProjectArgs(entrypoint, libraryUrl, entrypoint_args);
  args.engine_id = engine_id;

  FlutterEngineResult init_result = FlutterEngineInitialize(
      FLUTTER_ENGINE_VERSION, &config, &args, this, &engine_);
  if (init_result != kSuccess || !engine_) {
    FML_LOG(ERROR) << "Failed to initialize Flutter engine via Embedder API.";
    is_valid_ = false;
    return;
  }

  FlutterEngineResult run_result = FlutterEngineRunInitialized(engine_);
  if (run_result != kSuccess) {
    FML_LOG(ERROR) << "Failed to run initialized Flutter engine.";
    FlutterEngineShutdown(engine_);
    engine_ = nullptr;
    is_valid_ = false;
    return;
  }

  is_valid_ = true;

  auto* embedder_engine = reinterpret_cast<flutter::EmbedderEngine*>(engine_);
  if (platform_view_android_) {
    platform_view_android_->SetPlatformView(
        embedder_engine->GetShell().GetPlatformView());
  }

  embedder_engine->GetShell().RegisterImageDecoder(
      [runner = embedder_engine->GetTaskRunners().GetIOTaskRunner()](
          sk_sp<SkData> buffer) {
        return AndroidImageGenerator::MakeFromData(std::move(buffer), runner);
      },
      -1);

  UpdateDisplayMetrics();
}

std::unique_ptr<AndroidEngineBridge> EmbedderEngineBridge::Spawn(
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::string& initial_route,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) const {
  if (!IsValid()) {
    FML_LOG(ERROR) << "Cannot spawn from an invalid engine.";
    return nullptr;
  }

  std::vector<const char*> c_entrypoint_args;
  for (const auto& arg : entrypoint_args) {
    c_entrypoint_args.push_back(arg.c_str());
  }

  FlutterEngineSpawnInfo spawn_info = {};
  spawn_info.struct_size = sizeof(FlutterEngineSpawnInfo);
  spawn_info.entrypoint = entrypoint.empty() ? nullptr : entrypoint.c_str();
  spawn_info.library_uri = libraryUrl.empty() ? nullptr : libraryUrl.c_str();
  spawn_info.initial_route =
      initial_route.empty() ? nullptr : initial_route.c_str();
  if (!c_entrypoint_args.empty()) {
    spawn_info.entrypoint_argc = static_cast<int>(c_entrypoint_args.size());
    spawn_info.entrypoint_argv = c_entrypoint_args.data();
  }

  FLUTTER_API_SYMBOL(FlutterEngine) child_engine = nullptr;
  FlutterEngineResult result =
      FlutterEngineSpawn(engine_, &spawn_info, &child_engine);
  if (result != kSuccess || !child_engine) {
    FML_LOG(ERROR) << "Failed to spawn child engine via FlutterEngineSpawn.";
    return nullptr;
  }

  auto child_context = platform_view_android_->GetAndroidContext();
  bool enable_impeller =
      android_rendering_api_ == AndroidRenderingAPI::kImpellerOpenGLES ||
      android_rendering_api_ == AndroidRenderingAPI::kImpellerVulkan ||
      android_rendering_api_ == AndroidRenderingAPI::kImpellerAutoselect;

  auto child_embedder_surface = std::make_unique<EmbedderSurfaceAndroid>(
      child_context, enable_impeller, false);

  auto child_platform_view_delegate =
      std::make_unique<EmbedderPlatformViewDelegate>(settings_);

  auto child_platform_view = std::make_unique<PlatformViewAndroid>(
      *child_platform_view_delegate,
      TaskRunners("", nullptr, nullptr, nullptr, nullptr), jni_facade,
      child_context, child_embedder_surface.get());

  auto child_surface_manager =
      std::make_shared<AndroidSurfaceManager>(child_context);

  auto child_compositor =
      std::make_unique<AndroidCompositor>(child_surface_manager, jni_facade);

  auto* child_embedder_engine =
      reinterpret_cast<flutter::EmbedderEngine*>(child_engine);
  child_platform_view->SetPlatformView(
      child_embedder_engine->GetShell().GetPlatformView());

  return std::unique_ptr<AndroidEngineBridge>(new EmbedderEngineBridge(
      settings_, std::move(jni_facade), child_engine,
      std::move(child_platform_view_delegate), std::move(child_platform_view),
      std::move(child_embedder_surface), std::move(child_surface_manager),
      std::move(child_compositor), android_rendering_api_));
}

const flutter::Settings& EmbedderEngineBridge::GetSettings() const {
  return settings_;
}

fml::WeakPtr<PlatformViewAndroid> EmbedderEngineBridge::GetPlatformView() {
  if (platform_view_android_) {
    return platform_view_android_->GetWeakPtr();
  }
  return {};
}

PlatformViewAndroid* EmbedderEngineBridge::GetPlatformViewAndroid() {
  return platform_view_android_.get();
}

EmbedderSurfaceAndroid* EmbedderEngineBridge::GetEmbedderSurfaceAndroid() {
  return embedder_surface_.get();
}

bool EmbedderEngineBridge::IsSurfaceControlEnabled() {
  if (platform_view_android_) {
    return platform_view_android_->IsSurfaceControlEnabled();
  }
  return false;
}

Rasterizer::Screenshot EmbedderEngineBridge::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  if (!IsValid()) {
    return {nullptr, DlISize(), "", Rasterizer::ScreenshotFormat::kUnknown};
  }
  auto* embedder_engine = reinterpret_cast<flutter::EmbedderEngine*>(engine_);
  return embedder_engine->GetShell().Screenshot(type, base64_encode);
}

void EmbedderEngineBridge::NotifyLowMemoryWarning() {
  if (engine_) {
    FlutterEngineNotifyLowMemoryWarning(engine_);
  }
}

const std::shared_ptr<PlatformMessageHandler>&
EmbedderEngineBridge::GetPlatformMessageHandler() const {
  FML_DCHECK(platform_view_android_);
  return platform_view_android_->GetPlatformMessageHandler();
}

void EmbedderEngineBridge::UpdateDisplayMetrics() {
  if (!IsValid() || !jni_facade_) {
    return;
  }
  std::vector<std::unique_ptr<Display>> displays;
  displays.push_back(std::make_unique<AndroidDisplay>(jni_facade_));
  auto* embedder_engine = reinterpret_cast<flutter::EmbedderEngine*>(engine_);
  embedder_engine->GetShell().OnDisplayUpdates(std::move(displays));
}

const std::unique_ptr<Shell>& EmbedderEngineBridge::GetShellForTesting() const {
  static const std::unique_ptr<Shell> kNullShell = nullptr;
  return kNullShell;
}

}  // namespace flutter
