// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/platform/android/android_engine.h"

#include <android/api-level.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/system_properties.h>
#include <sys/time.h>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/settings.h"
#include "flutter/common/graphics/texture.h"
#include "flutter/fml/cpu_affinity.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/task_runner_util.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/shell_io_manager.h"
#include "flutter/shell/gpu/gpu_surface_gl_delegate.h"
#include "flutter/shell/platform/android/android_compositor_adapter.h"
#include "flutter/shell/platform/android/android_context_dynamic_impeller.h"
#include "flutter/shell/platform/android/android_context_gl_impeller.h"
#include "flutter/shell/platform/android/android_context_vk_impeller.h"
#include "flutter/shell/platform/android/android_display.h"
#include "flutter/shell/platform/android/android_external_texture_adapter.h"
#include "flutter/shell/platform/android/android_image_generator.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_surface_dynamic_impeller.h"
#include "flutter/shell/platform/android/android_surface_gl_impeller.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder_wrapper.h"
#include "flutter/shell/platform/android/image_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_message_response_android.h"
#include "flutter/shell/platform/android/surface/android_snapshot_surface_producer.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/shell/platform/android/surface/snapshot_surface_producer.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_vk_impeller.h"
#include "flutter/shell/platform/android/vsync_waiter_android.h"
#include "impeller/display_list/aiks_context.h"

#if IMPELLER_ENABLE_VULKAN
#include "flutter/shell/platform/android/android_surface_vk_impeller.h"
#include "flutter/shell/platform/android/image_external_texture_vk_impeller.h"
#endif

#if !SLIMPELLER
#include "flutter/shell/platform/android/android_context_gl_skia.h"
#include "flutter/shell/platform/android/android_surface_gl_skia.h"
#include "flutter/shell/platform/android/android_surface_software.h"
#include "flutter/shell/platform/android/image_external_texture_gl_skia.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_skia.h"
#endif  // !SLIMPELLER

namespace flutter {

static PlatformData GetDefaultPlatformData() {
  PlatformData platform_data;
  platform_data.lifecycle_state = "AppLifecycleState.detached";
  return platform_data;
}

namespace {

static constexpr int kMinAPILevelHCPP = 34;
static constexpr int64_t kImplicitViewId = 0;

AndroidContext::ContextSettings CreateContextSettings(
    const Settings& p_settings) {
  AndroidContext::ContextSettings settings;
  settings.enable_gpu_tracing = p_settings.enable_vulkan_gpu_tracing;
  settings.enable_validation = p_settings.enable_vulkan_validation;
  settings.enable_surface_control = p_settings.enable_surface_control;
  return settings;
}

#define ANDROID_SAFE_ACCESS(pointer, member, default_value)               \
  ([=]() {                                                                \
    if ((offsetof(std::remove_pointer<decltype(pointer)>::type, member) + \
             sizeof(pointer->member) <=                                   \
         pointer->struct_size)) {                                         \
      return pointer->member;                                             \
    }                                                                     \
    return static_cast<decltype(pointer->member)>((default_value));       \
  })()

flutter::PointerData::Change ToPointerDataChange(FlutterPointerPhase phase) {
  switch (phase) {
    case FlutterPointerPhase::kCancel:
      return flutter::PointerData::Change::kCancel;
    case FlutterPointerPhase::kUp:
      return flutter::PointerData::Change::kUp;
    case FlutterPointerPhase::kDown:
      return flutter::PointerData::Change::kDown;
    case FlutterPointerPhase::kMove:
      return flutter::PointerData::Change::kMove;
    case FlutterPointerPhase::kAdd:
      return flutter::PointerData::Change::kAdd;
    case FlutterPointerPhase::kRemove:
      return flutter::PointerData::Change::kRemove;
    case FlutterPointerPhase::kHover:
      return flutter::PointerData::Change::kHover;
    case FlutterPointerPhase::kPanZoomStart:
      return flutter::PointerData::Change::kPanZoomStart;
    case FlutterPointerPhase::kPanZoomUpdate:
      return flutter::PointerData::Change::kPanZoomUpdate;
    case FlutterPointerPhase::kPanZoomEnd:
      return flutter::PointerData::Change::kPanZoomEnd;
  }
  return flutter::PointerData::Change::kCancel;
}

FlutterPointerPhase ToFlutterPointerPhase(flutter::PointerData::Change change) {
  switch (change) {
    case flutter::PointerData::Change::kCancel:
      return FlutterPointerPhase::kCancel;
    case flutter::PointerData::Change::kUp:
      return FlutterPointerPhase::kUp;
    case flutter::PointerData::Change::kDown:
      return FlutterPointerPhase::kDown;
    case flutter::PointerData::Change::kMove:
      return FlutterPointerPhase::kMove;
    case flutter::PointerData::Change::kAdd:
      return FlutterPointerPhase::kAdd;
    case flutter::PointerData::Change::kRemove:
      return FlutterPointerPhase::kRemove;
    case flutter::PointerData::Change::kHover:
      return FlutterPointerPhase::kHover;
    case flutter::PointerData::Change::kPanZoomStart:
      return FlutterPointerPhase::kPanZoomStart;
    case flutter::PointerData::Change::kPanZoomUpdate:
      return FlutterPointerPhase::kPanZoomUpdate;
    case flutter::PointerData::Change::kPanZoomEnd:
      return FlutterPointerPhase::kPanZoomEnd;
  }
  return FlutterPointerPhase::kCancel;
}

flutter::PointerData::DeviceKind ToPointerDataKind(
    FlutterPointerDeviceKind device_kind) {
  switch (device_kind) {
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindMouse:
      return flutter::PointerData::DeviceKind::kMouse;
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch:
      return flutter::PointerData::DeviceKind::kTouch;
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindStylus:
      return flutter::PointerData::DeviceKind::kStylus;
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindInvertedStylus:
      return flutter::PointerData::DeviceKind::kInvertedStylus;
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindTrackpad:
      return flutter::PointerData::DeviceKind::kTrackpad;
  }
  return flutter::PointerData::DeviceKind::kTouch;
}

FlutterPointerDeviceKind ToFlutterPointerDeviceKind(
    flutter::PointerData::DeviceKind kind) {
  switch (kind) {
    case flutter::PointerData::DeviceKind::kMouse:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindMouse;
    case flutter::PointerData::DeviceKind::kTouch:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch;
    case flutter::PointerData::DeviceKind::kStylus:
    case flutter::PointerData::DeviceKind::kInvertedStylus:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindStylus;
    case flutter::PointerData::DeviceKind::kTrackpad:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTrackpad;
  }
  return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch;
}

flutter::PointerData::SignalKind ToPointerDataSignalKind(
    FlutterPointerSignalKind signal_kind) {
  switch (signal_kind) {
    case FlutterPointerSignalKind::kFlutterPointerSignalKindNone:
      return flutter::PointerData::SignalKind::kNone;
    case FlutterPointerSignalKind::kFlutterPointerSignalKindScroll:
      return flutter::PointerData::SignalKind::kScroll;
    case FlutterPointerSignalKind::kFlutterPointerSignalKindScrollInertiaCancel:
      return flutter::PointerData::SignalKind::kScrollInertiaCancel;
    case FlutterPointerSignalKind::kFlutterPointerSignalKindScale:
      return flutter::PointerData::SignalKind::kScale;
  }
  return flutter::PointerData::SignalKind::kNone;
}

FlutterPointerSignalKind ToFlutterPointerSignalKind(
    flutter::PointerData::SignalKind signal_kind) {
  switch (signal_kind) {
    case flutter::PointerData::SignalKind::kNone:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindNone;
    case flutter::PointerData::SignalKind::kScroll:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindScroll;
    case flutter::PointerData::SignalKind::kScrollInertiaCancel:
      return FlutterPointerSignalKind::
          kFlutterPointerSignalKindScrollInertiaCancel;
    case flutter::PointerData::SignalKind::kScale:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindScale;
  }
  return FlutterPointerSignalKind::kFlutterPointerSignalKindNone;
}

class AndroidSurfaceFactoryImpl : public AndroidSurfaceFactory {
 public:
  AndroidSurfaceFactoryImpl(const std::shared_ptr<AndroidContext>& context,
                            bool enable_impeller,
                            bool lazy_shader_mode)
      : android_context_(context),
        enable_impeller_(enable_impeller),
        lazy_shader_mode_(lazy_shader_mode) {}

  ~AndroidSurfaceFactoryImpl() override = default;

  std::unique_ptr<AndroidSurface> CreateSurface() override {
    if (android_context_->IsDynamicSelection()) {
      auto cast_ptr = std::static_pointer_cast<AndroidContextDynamicImpeller>(
          android_context_);
      return std::make_unique<AndroidSurfaceDynamicImpeller>(cast_ptr);
    }
    switch (android_context_->RenderingApi()) {
#if !SLIMPELLER
      case AndroidRenderingAPI::kSoftware:
        return std::make_unique<AndroidSurfaceSoftware>();
      case AndroidRenderingAPI::kSkiaOpenGLES:
        return std::make_unique<AndroidSurfaceGLSkia>(
            std::static_pointer_cast<AndroidContextGLSkia>(android_context_));
#endif  // !SLIMPELLER
      case AndroidRenderingAPI::kImpellerOpenGLES:
        return std::make_unique<AndroidSurfaceGLImpeller>(
            std::static_pointer_cast<AndroidContextGLImpeller>(
                android_context_));
      case AndroidRenderingAPI::kImpellerVulkan:
        return std::make_unique<AndroidSurfaceVKImpeller>(
            std::static_pointer_cast<AndroidContextVKImpeller>(
                android_context_));
      case AndroidRenderingAPI::kImpellerAutoselect: {
        auto cast_ptr = std::static_pointer_cast<AndroidContextDynamicImpeller>(
            android_context_);
        return std::make_unique<AndroidSurfaceDynamicImpeller>(cast_ptr);
      }
    }
    FML_UNREACHABLE();
  }

 private:
  const std::shared_ptr<AndroidContext>& android_context_;
  const bool enable_impeller_;
  const bool lazy_shader_mode_;
};

static std::shared_ptr<flutter::AndroidContext> CreateAndroidContext(
    const flutter::TaskRunners& task_runners,
    AndroidRenderingAPI android_rendering_api,
    bool enable_opengl_gpu_tracing,
    const AndroidContext::ContextSettings& settings,
    std::shared_ptr<fml::BasicTaskRunner> io_task_runner) {
  switch (android_rendering_api) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      return std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);
    case AndroidRenderingAPI::kSkiaOpenGLES:
      return std::make_unique<AndroidContextGLSkia>(
          fml::MakeRefCounted<AndroidEnvironmentGL>(), task_runners);
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerVulkan:
      return std::make_unique<AndroidContextVKImpeller>(settings);
    case AndroidRenderingAPI::kImpellerOpenGLES:
      return std::make_unique<AndroidContextGLImpeller>(
          std::make_unique<impeller::egl::Display>(), enable_opengl_gpu_tracing,
          std::move(io_task_runner));
    case AndroidRenderingAPI::kImpellerAutoselect:
      return std::make_unique<AndroidContextDynamicImpeller>(
          settings, std::move(io_task_runner));
  }
  FML_UNREACHABLE();
}

class AndroidEmbedderSurface : public EmbedderSurface {
 public:
  explicit AndroidEmbedderSurface(AndroidEngine* engine) : engine_(engine) {
    FML_DCHECK(engine_);
  }

  ~AndroidEmbedderSurface() override = default;

  bool IsValid() const override { return true; }

  std::unique_ptr<Surface> CreateGPUSurface() override {
    return engine_->CreateRenderingSurface();
  }

  sk_sp<GrDirectContext> CreateResourceContext() const override {
    return engine_->CreateResourceContext();
  }

  void ReleaseResourceContext() const override {
    engine_->ReleaseResourceContext();
  }

  std::shared_ptr<impeller::Context> CreateImpellerContext() const override {
    return engine_->GetImpellerContext();
  }

 private:
  AndroidEngine* engine_;
};

static std::unique_ptr<PlatformViewEmbedder> CreatePlatformViewEmbedder(
    Shell& shell,
    AndroidEngine* engine,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) {
  PlatformViewEmbedder::PlatformDispatchTable platform_dispatch_table;
  platform_dispatch_table.update_semantics_callback =
      [engine](int64_t view_id, flutter::SemanticsNodeUpdates update,
               flutter::CustomAccessibilityActionUpdates actions) {
        if (engine) {
          engine->UpdateSemantics(view_id, std::move(update),
                                  std::move(actions));
        }
      };
  platform_dispatch_table.platform_message_response_callback =
      [engine](std::unique_ptr<PlatformMessage> message) {
        if (engine) {
          engine->GetPlatformMessageHandler()->HandlePlatformMessage(
              std::move(message));
        }
      };
  platform_dispatch_table.compute_platform_resolved_locale_callback =
      [jni_facade](const std::vector<std::string>& supported_locale_data) {
        return jni_facade->FlutterViewComputePlatformResolvedLocale(
            supported_locale_data);
      };
  platform_dispatch_table.on_pre_engine_restart_callback = [jni_facade]() {
    jni_facade->FlutterViewOnPreEngineRestart();
  };
  platform_dispatch_table.request_dart_deferred_library_callback =
      [jni_facade](intptr_t loading_unit_id) {
        jni_facade->RequestDartDeferredLibrary(loading_unit_id);
      };
  platform_dispatch_table.set_application_locale_callback =
      [jni_facade](const std::string& locale) {
        jni_facade->FlutterViewSetApplicationLocale(locale);
      };
  platform_dispatch_table.get_scaled_font_size_callback =
      [jni_facade](double unscaled_font_size, int configuration_id) {
        return jni_facade->FlutterViewGetScaledFontSize(unscaled_font_size,
                                                        configuration_id);
      };

  auto embedder_surface = std::make_unique<AndroidEmbedderSurface>(engine);
  return std::make_unique<PlatformViewEmbedder>(
      shell, shell.GetTaskRunners(), std::move(embedder_surface),
      std::move(platform_dispatch_table), nullptr);
}

class PlatformViewProtectedAccessor : public PlatformView {
 public:
  using PlatformView::CreateRenderingSurface;
};

}  // namespace

AndroidEngine::AndroidEngine(const flutter::Settings& settings,
                             std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                             AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      android_rendering_api_(android_rendering_api),
      platform_view_android_delegate_(jni_facade_) {
  static size_t thread_host_count = 1;
  auto thread_label = std::to_string(thread_host_count++);

  task_runners_ = AndroidTaskRunners::Create(thread_label, settings);

  TRACE_EVENT1("flutter", "AndroidEngine::Initialize", "path", "embedder_api");
  InitializeEngine();
}

AndroidEngine::AndroidEngine(
    const Settings& settings,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<AndroidTaskRunners>& task_runners,
    std::unique_ptr<Shell> shell,
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    AndroidRenderingAPI rendering_api,
    PlatformViewEmbedder* platform_view_embedder,
    std::shared_ptr<AndroidContext> android_context)
    : settings_(settings),
      jni_facade_(jni_facade),
      task_runners_(task_runners),
      shell_(std::move(shell)),
      apk_asset_provider_(std::move(apk_asset_provider)),
      android_rendering_api_(rendering_api),
      platform_view_embedder_(platform_view_embedder),
      platform_view_(platform_view_embedder),
      platform_view_android_delegate_(jni_facade) {
  FML_DCHECK(jni_facade);
  FML_DCHECK(task_runners);
  is_valid_ = shell_ != nullptr;
  InitializeProjectArgs();
  InitializeSurfaceAndAdapters(std::move(android_context));
}

AndroidEngine::~AndroidEngine() {
  shell_.reset();
  surface_lifecycle_.reset();
  external_texture_adapter_.reset();
  android_surface_.reset();
  surface_factory_.reset();
  android_context_.reset();
  task_runners_.reset();
}

void AndroidEngine::InitializeSurfaceAndAdapters(
    std::shared_ptr<AndroidContext> android_context) {
  if (android_context) {
    android_context_ = std::move(android_context);
  } else if (task_runners_) {
    android_context_ = CreateAndroidContext(
        task_runners_->GetTaskRunners(), android_rendering_api_,
        settings_.enable_opengl_gpu_tracing, CreateContextSettings(settings_),
        task_runners_->GetIOTaskRunner());
  }

  if (android_context_) {
    FML_CHECK(android_context_->IsValid())
        << "Could not create surface from invalid Android context.";
    surface_factory_ = std::make_shared<AndroidSurfaceFactoryImpl>(
        android_context_, settings_.enable_impeller,
        settings_.impeller_enable_lazy_shader_mode);
    android_surface_ = surface_factory_->CreateSurface();
    android_meets_hcpp_criteria_ =
        settings_.enable_surface_control &&
        android_get_device_api_level() >= kMinAPILevelHCPP &&
        settings_.enable_impeller;
    FML_CHECK(android_surface_ && android_surface_->IsValid())
        << "Could not create an OpenGL, Vulkan or Software surface to set up "
           "rendering.";
  }

  if (task_runners_) {
    surface_lifecycle_ = std::make_unique<AndroidSurfaceLifecycle>(
        task_runners_->GetRasterTaskRunner(), jni_facade_,
        android_surface_.get(), this);
  }
  external_texture_adapter_ = std::make_unique<AndroidExternalTextureAdapter>(
      android_context_, jni_facade_, this);
  platform_message_handler_ =
      std::make_shared<PlatformMessageHandlerAndroid>(jni_facade_);
}

FlutterEngineResult AndroidEngine::InitializeEngine() {
  if (shell_) {
    return kSuccess;
  }

  InitializeSurfaceAndAdapters();

  PlatformViewEmbedder* raw_platform_view_embedder = nullptr;
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [this, &raw_platform_view_embedder](Shell& shell) {
        auto platform_view_embedder =
            CreatePlatformViewEmbedder(shell, this, jni_facade_);
        raw_platform_view_embedder = platform_view_embedder.get();
        platform_view_ = raw_platform_view_embedder;
        return platform_view_embedder;
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  shell_ =
      Shell::Create(GetDefaultPlatformData(), task_runners_->GetTaskRunners(),
                    settings_, on_create_platform_view, on_create_rasterizer);

  if (!shell_) {
    is_valid_ = false;
    return kInternalInconsistency;
  }

  shell_->GetDartVM()->GetConcurrentMessageLoop()->PostTaskToAllWorkers([]() {
    if (::setpriority(PRIO_PROCESS, gettid(), 1) != 0) {
      FML_LOG(ERROR) << "Failed to set Workers task runner priority";
    }
  });

  shell_->RegisterImageDecoder(
      [runner = task_runners_->GetTaskRunners().GetIOTaskRunner()](
          sk_sp<SkData> buffer) {
        return AndroidImageGenerator::MakeFromData(std::move(buffer), runner);
      },
      -1);

  platform_view_embedder_ = raw_platform_view_embedder;
  platform_view_ = raw_platform_view_embedder;
  is_valid_ = true;
  InitializeProjectArgs();
  return kSuccess;
}

bool AndroidEngine::IsValid() const {
  return is_valid_;
}

const flutter::Settings& AndroidEngine::GetSettings() const {
  return settings_;
}

std::unique_ptr<AndroidEngine> AndroidEngine::Spawn(
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::string& initial_route,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) const {
  TRACE_EVENT1("flutter", "AndroidEngine::Spawn", "path", "embedder_api");
  std::vector<const char*> argv;
  argv.reserve(entrypoint_args.size());
  for (const auto& arg : entrypoint_args) {
    argv.push_back(arg.c_str());
  }

  FlutterProjectArgs custom_project_args = CreateFlutterProjectArgs(
      entrypoint, libraryUrl, entrypoint_args, engine_id);

  FlutterEngineSpawnConfig spawn_config = {};
  spawn_config.struct_size = sizeof(FlutterEngineSpawnConfig);
  spawn_config.entrypoint = entrypoint.empty() ? nullptr : entrypoint.c_str();
  spawn_config.library_path = libraryUrl.empty() ? nullptr : libraryUrl.c_str();
  spawn_config.initial_route =
      initial_route.empty() ? nullptr : initial_route.c_str();
  spawn_config.argc = static_cast<int64_t>(argv.size());
  spawn_config.argv = argv.empty() ? nullptr : argv.data();
  spawn_config.custom_args = &custom_project_args;

  std::unique_ptr<AndroidEngine> spawned_engine;
  FlutterEngineResult result =
      SpawnEngine(&spawn_config, jni_facade, &spawned_engine);
  if (result != kSuccess) {
    return nullptr;
  }
  return spawned_engine;
}

void AndroidEngine::Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
                           const std::string& entrypoint,
                           const std::string& libraryUrl,
                           const std::vector<std::string>& entrypoint_args,
                           int64_t engine_id) {
  if (!IsValid()) {
    return;
  }

  apk_asset_provider_ = std::move(apk_asset_provider);
  if (apk_asset_provider_) {
    asset_resolvers_[0] = apk_asset_provider_->GetFlutterAssetResolver();
    project_args_.asset_resolvers = asset_resolvers_;
    project_args_.asset_resolvers_count = 1;
  }
  project_args_.engine_id = engine_id;

  TRACE_EVENT1("flutter", "AndroidEngine::Launch", "path", "embedder_api");
  RunEngine(entrypoint, libraryUrl, entrypoint_args, engine_id);
}

FlutterEngineResult AndroidEngine::RunEngine(
    const std::string& entrypoint,
    const std::string& library_url,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) {
  if (!IsValid() || !shell_) {
    return kInternalInconsistency;
  }
  auto config = BuildRunConfiguration(entrypoint, library_url, entrypoint_args);
  if (!config) {
    return kInvalidArguments;
  }
  config->SetEngineId(engine_id);
  UpdateDisplayMetrics();
  shell_->RunEngine(std::move(config.value()));
  return kSuccess;
}

FlutterEngineResult AndroidEngine::SpawnEngine(
    const FlutterEngineSpawnConfig* config,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    std::unique_ptr<AndroidEngine>* spawned_engine_out) const {
  if (spawned_engine_out == nullptr) {
    return kInvalidArguments;
  }
  *spawned_engine_out = nullptr;

  if (config == nullptr ||
      config->struct_size != sizeof(FlutterEngineSpawnConfig)) {
    return kInvalidArguments;
  }

  if (!IsValid() || !shell_ || !shell_->IsSetup()) {
    return kInternalInconsistency;
  }

  if (config->argc < 0) {
    return kInvalidArguments;
  }

  std::vector<std::string> entrypoint_args;
  if (config->argc > 0) {
    if (config->argv == nullptr) {
      return kInvalidArguments;
    }
    entrypoint_args.reserve(config->argc);
    for (int64_t i = 0; i < config->argc; ++i) {
      if (config->argv[i] == nullptr) {
        return kInvalidArguments;
      }
      entrypoint_args.emplace_back(config->argv[i]);
    }
  }

  std::string entrypoint = config->entrypoint ? config->entrypoint : "";
  std::string library_url = config->library_path ? config->library_path : "";
  std::string initial_route =
      (config->initial_route != nullptr && config->initial_route[0] != '\0')
          ? config->initial_route
          : "/";

  int64_t engine_id = 0;
  if (config->custom_args != nullptr) {
    engine_id = config->custom_args->engine_id;
    if (entrypoint.empty() &&
        config->custom_args->custom_dart_entrypoint != nullptr) {
      entrypoint = config->custom_args->custom_dart_entrypoint;
    }
  }

  PlatformViewEmbedder* raw_spawned_platform_view_embedder = nullptr;
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&raw_spawned_platform_view_embedder, jni_facade](Shell& shell) {
        auto platform_view_embedder =
            CreatePlatformViewEmbedder(shell, nullptr, jni_facade);
        raw_spawned_platform_view_embedder = platform_view_embedder.get();
        return platform_view_embedder;
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  auto run_config =
      BuildRunConfiguration(entrypoint, library_url, entrypoint_args);
  if (!run_config) {
    return kInvalidArguments;
  }
  run_config->SetEngineId(engine_id);

  std::unique_ptr<flutter::Shell> shell =
      shell_->Spawn(std::move(run_config.value()), initial_route,
                    on_create_platform_view, on_create_rasterizer);
  if (!shell) {
    return kInternalInconsistency;
  }

  auto spawned_engine = std::unique_ptr<AndroidEngine>(new AndroidEngine(
      GetSettings(), jni_facade, task_runners_, std::move(shell),
      apk_asset_provider_ ? apk_asset_provider_->Clone() : nullptr,
      android_rendering_api_, raw_spawned_platform_view_embedder,
      android_context_));
  if (!spawned_engine) {
    return kInternalInconsistency;
  }
  spawned_engine->project_args_.engine_id = engine_id;
  *spawned_engine_out = std::move(spawned_engine);
  return kSuccess;
}

Rasterizer::Screenshot AndroidEngine::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  auto surface_screenshot = ScreenshotSurface();
  Rasterizer::Screenshot screenshot;
  screenshot.data = surface_screenshot.data;
  screenshot.frame_size = surface_screenshot.frame_size;
  screenshot.format = "ScreenshotType::UncompressedImage";
  return screenshot;
}

void AndroidEngine::NotifyLowMemoryWarning() {
  if (shell_) {
    shell_->NotifyLowMemoryWarning();
  }
}

std::optional<RunConfiguration> AndroidEngine::BuildRunConfiguration(
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::vector<std::string>& entrypoint_args) const {
  std::unique_ptr<IsolateConfiguration> isolate_configuration;
  if (flutter::DartVM::IsRunningPrecompiledCode()) {
    isolate_configuration = IsolateConfiguration::CreateForAppSnapshot();
  } else {
    std::unique_ptr<fml::Mapping> kernel_blob =
        fml::FileMapping::CreateReadOnly(
            GetSettings().application_kernel_asset);
    if (!kernel_blob) {
      FML_DLOG(ERROR) << "Unable to load the kernel blob asset.";
      return std::nullopt;
    }
    isolate_configuration =
        IsolateConfiguration::CreateForKernel(std::move(kernel_blob));
  }

  RunConfiguration config(std::move(isolate_configuration));
  if (apk_asset_provider_) {
    config.AddAssetResolver(apk_asset_provider_->Clone());
  }

  if (!entrypoint.empty() && !libraryUrl.empty()) {
    config.SetEntrypointAndLibrary(entrypoint, libraryUrl);
  } else if (!entrypoint.empty()) {
    config.SetEntrypoint(entrypoint);
  }
  if (!entrypoint_args.empty()) {
    config.SetEntrypointArgs(entrypoint_args);
  }
  return config;
}

void AndroidEngine::UpdateDisplayMetrics() {
  if (!shell_) {
    return;
  }
  std::vector<std::unique_ptr<Display>> displays;
  displays.push_back(std::make_unique<AndroidDisplay>(jni_facade_));
  shell_->OnDisplayUpdates(std::move(displays));
}

bool AndroidEngine::IsSurfaceControlEnabled() const {
  return android_meets_hcpp_criteria_ && android_context_ &&
         android_context_->RenderingApi() ==
             AndroidRenderingAPI::kImpellerVulkan &&
         impeller::ContextVK::Cast(*android_context_->GetImpellerContext())
             .GetShouldEnableSurfaceControlSwapchain();
}

void AndroidEngine::InitializeProjectArgs() {
  project_args_.struct_size = sizeof(FlutterProjectArgs);
  project_args_.assets_path = settings_.assets_path.c_str();
  project_args_.icu_data_path = settings_.icu_data_path.c_str();

  project_args_.command_line_argc = 0;
  project_args_.command_line_argv = nullptr;

  if (task_runners_) {
    project_args_.custom_task_runners = task_runners_->GetCustomTaskRunners();
  }

  if (apk_asset_provider_) {
    asset_resolvers_[0] = apk_asset_provider_->GetFlutterAssetResolver();
    project_args_.asset_resolvers = asset_resolvers_;
    project_args_.asset_resolvers_count = 1;
  } else {
    asset_resolvers_[0] = nullptr;
    project_args_.asset_resolvers = nullptr;
    project_args_.asset_resolvers_count = 0;
  }

  project_args_.on_pre_engine_restart_callback = OnPreEngineRestart;
  project_args_.set_application_locale_callback = OnSetApplicationLocale;
  project_args_.get_scaled_font_size_callback = OnGetScaledFontSize;
  project_args_.dart_deferred_library_request_callback =
      OnRequestDartDeferredLibrary;
}

void AndroidEngine::OnPreEngineRestart(void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine && engine->jni_facade_) {
    engine->jni_facade_->FlutterViewOnPreEngineRestart();
  }
}

void AndroidEngine::OnSetApplicationLocale(const char* locale,
                                           void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine && engine->jni_facade_ && locale) {
    engine->jni_facade_->FlutterViewSetApplicationLocale(locale);
  }
}

double AndroidEngine::OnGetScaledFontSize(double unscaled_font_size,
                                          int configuration_id,
                                          void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine && engine->jni_facade_) {
    return engine->jni_facade_->FlutterViewGetScaledFontSize(unscaled_font_size,
                                                             configuration_id);
  }
  return unscaled_font_size;
}

void AndroidEngine::OnRequestDartDeferredLibrary(intptr_t loading_unit_id,
                                                 void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine && engine->jni_facade_) {
    engine->jni_facade_->RequestDartDeferredLibrary(loading_unit_id);
  }
}

FlutterProjectArgs AndroidEngine::CreateFlutterProjectArgs(
    const std::string& entrypoint,
    const std::string& library_url,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) const {
  FlutterProjectArgs args = project_args_;
  args.engine_id = engine_id;
  if (!entrypoint.empty()) {
    args.custom_dart_entrypoint = entrypoint.c_str();
  }
  return args;
}

// Surface lifecycle methods
void AndroidEngine::NotifyCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifyCreated(std::move(native_window));
  }
}

void AndroidEngine::NotifySurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifySurfaceWindowChanged(std::move(native_window));
  }
}

void AndroidEngine::NotifyDestroyed() {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifyDestroyed();
  }
}

void AndroidEngine::NotifyChanged(const DlISize& size) {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifyChanged(size);
  }
}

void AndroidEngine::SetGpuAvailability(FlutterGpuAvailability availability) {
  if (surface_lifecycle_) {
    surface_lifecycle_->SetGpuAvailability(availability);
  }
}

void AndroidEngine::OnSurfaceCreated() {
  if (platform_view_) {
    platform_view_->NotifyCreated();
  }
}

void AndroidEngine::OnSurfaceDestroyed() {
  if (platform_view_) {
    platform_view_->NotifyDestroyed();
  }
}

void AndroidEngine::OnScheduleFrame() {
  if (platform_view_) {
    platform_view_->ScheduleFrame();
  }
}

void AndroidEngine::OnInstallFirstFrameCallback() {
  InstallFirstFrameCallback();
}

void AndroidEngine::SetupImpellerContext() {
  if (platform_view_) {
    platform_view_->SetupImpellerContext();
  }
  if (android_context_) {
    android_context_->SetupImpellerContext();
  }
  if (android_surface_) {
    android_surface_->SetupImpellerSurface();
  }
}

AndroidSurface::ScreenshotResult AndroidEngine::ScreenshotSurface() {
  if (!android_surface_ || !task_runners_) {
    return {};
  }
  AndroidSurface::ScreenshotResult screenshot;
  fml::AutoResetWaitableEvent latch;
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_->GetRasterTaskRunner(),
      [&latch, surface = android_surface_.get(), &screenshot]() {
        screenshot = surface->Screenshot();
        latch.Signal();
      });
  latch.Wait();
  return screenshot;
}

// AndroidExternalTextureAdapter::Delegate
void AndroidEngine::OnRegisterTexture(
    std::shared_ptr<flutter::Texture> texture) {
  if (platform_view_) {
    platform_view_->RegisterTexture(std::move(texture));
  }
}

void AndroidEngine::OnUnregisterTexture(int64_t texture_id) {
  if (platform_view_) {
    platform_view_->UnregisterTexture(texture_id);
  }
}

void AndroidEngine::OnMarkTextureFrameAvailable(int64_t texture_id) {
  if (platform_view_) {
    platform_view_->MarkTextureFrameAvailable(texture_id);
  }
}

// Event & Seam Dispatch
void AndroidEngine::DispatchPlatformMessage(JNIEnv* env,
                                            std::string name,
                                            jobject java_message_data,
                                            jint java_message_position,
                                            jint response_id) {
  uint8_t* message_data =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(java_message_data));
  fml::MallocMapping message =
      fml::MallocMapping::Copy(message_data, java_message_position);

  fml::RefPtr<flutter::PlatformMessageResponse> response;
  if (response_id && task_runners_) {
    response = fml::MakeRefCounted<PlatformMessageResponseAndroid>(
        response_id, jni_facade_, task_runners_->GetPlatformTaskRunner());
  }

  if (platform_view_) {
    platform_view_->DispatchPlatformMessage(
        std::make_unique<flutter::PlatformMessage>(
            std::move(name), std::move(message), std::move(response)));
  }
}

void AndroidEngine::DispatchEmptyPlatformMessage(JNIEnv* env,
                                                 std::string name,
                                                 jint response_id) {
  fml::RefPtr<flutter::PlatformMessageResponse> response;
  if (response_id && task_runners_) {
    response = fml::MakeRefCounted<PlatformMessageResponseAndroid>(
        response_id, jni_facade_, task_runners_->GetPlatformTaskRunner());
  }

  if (platform_view_) {
    platform_view_->DispatchPlatformMessage(
        std::make_unique<flutter::PlatformMessage>(std::move(name),
                                                   std::move(response)));
  }
}

void AndroidEngine::DispatchPointerDataPacket(
    std::unique_ptr<PointerDataPacket> packet) {
  TRACE_EVENT1("flutter", "AndroidEngine::DispatchPointerDataPacket", "path",
               "embedder_api");
  if (!packet) {
    return;
  }
  size_t count = packet->GetLength();
  if (count == 0) {
    return;
  }
  std::vector<FlutterPointerEvent> events(count);
  for (size_t i = 0; i < count; ++i) {
    PointerData data = packet->GetPointerData(i);
    FlutterPointerEvent& event = events[i];
    event.struct_size = sizeof(FlutterPointerEvent);
    event.timestamp = data.time_stamp;
    event.phase = ToFlutterPointerPhase(data.change);
    event.x = data.physical_x;
    event.y = data.physical_y;
    event.device = data.device;
    event.signal_kind = ToFlutterPointerSignalKind(data.signal_kind);
    event.scroll_delta_x = data.scroll_delta_x;
    event.scroll_delta_y = data.scroll_delta_y;
    event.device_kind = ToFlutterPointerDeviceKind(data.kind);
    event.buttons = data.buttons;
    event.pan_x = data.pan_x;
    event.pan_y = data.pan_y;
    event.scale = data.scale;
    event.rotation = data.rotation;
    event.pressure = data.pressure;
    event.pressure_min = data.pressure_min;
    event.pressure_max = data.pressure_max;
    event.tilt = data.tilt;
    event.orientation = data.orientation;
    event.radius_major = data.radius_major;
    event.radius_minor = data.radius_minor;
    event.radius_min = data.radius_min;
    event.radius_max = data.radius_max;
    event.distance = data.distance;
    event.distance_max = data.distance_max;
    event.size = data.size;
    event.embedder_id = data.embedder_id;
    event.view_id = data.view_id;
  }
  SendPointerEvents(events.data(), events.size());
}

FlutterEngineResult AndroidEngine::SendPointerEvents(
    const FlutterPointerEvent* events,
    size_t count) {
  if (events == nullptr || count == 0) {
    return kInvalidArguments;
  }

  auto packet = std::make_unique<PointerDataPacket>(count);
  const FlutterPointerEvent* current = events;

  for (size_t i = 0; i < count; ++i) {
    PointerData pointer_data;
    pointer_data.Clear();
    pointer_data.embedder_id = ANDROID_SAFE_ACCESS(current, embedder_id, 0);
    pointer_data.time_stamp = ANDROID_SAFE_ACCESS(current, timestamp, 0);
    pointer_data.change = ToPointerDataChange(
        ANDROID_SAFE_ACCESS(current, phase, FlutterPointerPhase::kCancel));
    pointer_data.physical_x = ANDROID_SAFE_ACCESS(current, x, 0.0);
    pointer_data.physical_y = ANDROID_SAFE_ACCESS(current, y, 0.0);
    pointer_data.physical_delta_x = 0.0;
    pointer_data.physical_delta_y = 0.0;
    pointer_data.device = ANDROID_SAFE_ACCESS(current, device, 0);
    pointer_data.pointer_identifier = 0;
    pointer_data.signal_kind = ToPointerDataSignalKind(ANDROID_SAFE_ACCESS(
        current, signal_kind, kFlutterPointerSignalKindNone));
    pointer_data.scroll_delta_x =
        ANDROID_SAFE_ACCESS(current, scroll_delta_x, 0.0);
    pointer_data.scroll_delta_y =
        ANDROID_SAFE_ACCESS(current, scroll_delta_y, 0.0);
    FlutterPointerDeviceKind device_kind = ANDROID_SAFE_ACCESS(
        current, device_kind, kFlutterPointerDeviceKindTouch);
    pointer_data.kind = ToPointerDataKind(device_kind);
    pointer_data.buttons = ANDROID_SAFE_ACCESS(current, buttons, 0);
    pointer_data.pan_x = ANDROID_SAFE_ACCESS(current, pan_x, 0.0);
    pointer_data.pan_y = ANDROID_SAFE_ACCESS(current, pan_y, 0.0);
    pointer_data.pan_delta_x = 0.0;
    pointer_data.pan_delta_y = 0.0;
    pointer_data.scale = ANDROID_SAFE_ACCESS(current, scale, 0.0);
    pointer_data.rotation = ANDROID_SAFE_ACCESS(current, rotation, 0.0);
    pointer_data.pressure = ANDROID_SAFE_ACCESS(current, pressure, 0.0);
    pointer_data.pressure_min = ANDROID_SAFE_ACCESS(current, pressure_min, 0.0);
    pointer_data.pressure_max = ANDROID_SAFE_ACCESS(current, pressure_max, 0.0);
    pointer_data.tilt = ANDROID_SAFE_ACCESS(current, tilt, 0.0);
    pointer_data.orientation = ANDROID_SAFE_ACCESS(current, orientation, 0.0);
    pointer_data.radius_major = ANDROID_SAFE_ACCESS(current, radius_major, 0.0);
    pointer_data.radius_minor = ANDROID_SAFE_ACCESS(current, radius_minor, 0.0);
    pointer_data.radius_min = ANDROID_SAFE_ACCESS(current, radius_min, 0.0);
    pointer_data.radius_max = ANDROID_SAFE_ACCESS(current, radius_max, 0.0);
    pointer_data.distance = ANDROID_SAFE_ACCESS(current, distance, 0.0);
    pointer_data.distance_max = ANDROID_SAFE_ACCESS(current, distance_max, 0.0);
    pointer_data.size = ANDROID_SAFE_ACCESS(current, size, 0.0);
    pointer_data.view_id =
        ANDROID_SAFE_ACCESS(current, view_id, kImplicitViewId);
    packet->SetPointerData(i, pointer_data);
    current = reinterpret_cast<const FlutterPointerEvent*>(
        reinterpret_cast<const uint8_t*>(current) + current->struct_size);
  }

  if (platform_view_) {
    platform_view_->DispatchPointerDataPacket(std::move(packet));
  }
  return kSuccess;
}

void AndroidEngine::SetViewportMetrics(int64_t view_id,
                                       const ViewportMetrics& metrics) {
  TRACE_EVENT1("flutter", "AndroidEngine::SetViewportMetrics", "path",
               "embedder_api");
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.view_id = view_id;
  event.width = static_cast<size_t>(metrics.physical_width);
  event.height = static_cast<size_t>(metrics.physical_height);
  event.pixel_ratio = metrics.device_pixel_ratio;
  event.left = 0;
  event.top = 0;
  event.physical_view_inset_top = metrics.physical_view_inset_top;
  event.physical_view_inset_right = metrics.physical_view_inset_right;
  event.physical_view_inset_bottom = metrics.physical_view_inset_bottom;
  event.physical_view_inset_left = metrics.physical_view_inset_left;
  event.display_id = metrics.display_id;
  event.has_constraints =
      (metrics.physical_min_width_constraint != metrics.physical_width ||
       metrics.physical_max_width_constraint != metrics.physical_width ||
       metrics.physical_min_height_constraint != metrics.physical_height ||
       metrics.physical_max_height_constraint != metrics.physical_height);
  event.min_width_constraint =
      static_cast<size_t>(metrics.physical_min_width_constraint);
  event.max_width_constraint =
      static_cast<size_t>(metrics.physical_max_width_constraint);
  event.min_height_constraint =
      static_cast<size_t>(metrics.physical_min_height_constraint);
  event.max_height_constraint =
      static_cast<size_t>(metrics.physical_max_height_constraint);
  event.display_features_count = metrics.physical_display_features_type.size();
  event.display_features_bounds =
      metrics.physical_display_features_bounds.empty()
          ? nullptr
          : metrics.physical_display_features_bounds.data();
  event.display_features_type =
      metrics.physical_display_features_type.empty()
          ? nullptr
          : metrics.physical_display_features_type.data();
  event.display_features_state =
      metrics.physical_display_features_state.empty()
          ? nullptr
          : metrics.physical_display_features_state.data();
  event.physical_padding_top = metrics.physical_padding_top;
  event.physical_padding_right = metrics.physical_padding_right;
  event.physical_padding_bottom = metrics.physical_padding_bottom;
  event.physical_padding_left = metrics.physical_padding_left;
  event.physical_system_gesture_inset_top =
      metrics.physical_system_gesture_inset_top;
  event.physical_system_gesture_inset_right =
      metrics.physical_system_gesture_inset_right;
  event.physical_system_gesture_inset_bottom =
      metrics.physical_system_gesture_inset_bottom;
  event.physical_system_gesture_inset_left =
      metrics.physical_system_gesture_inset_left;
  event.physical_touch_slop = metrics.physical_touch_slop;
  event.physical_display_corner_radius_top_left =
      metrics.physical_display_corner_radius_top_left;
  event.physical_display_corner_radius_top_right =
      metrics.physical_display_corner_radius_top_right;
  event.physical_display_corner_radius_bottom_right =
      metrics.physical_display_corner_radius_bottom_right;
  event.physical_display_corner_radius_bottom_left =
      metrics.physical_display_corner_radius_bottom_left;

  SendWindowMetricsEvent(&event);
}

FlutterEngineResult AndroidEngine::SendWindowMetricsEvent(
    const FlutterWindowMetricsEvent* event) {
  if (event == nullptr) {
    return kInvalidArguments;
  }

  flutter::ViewportMetrics metrics;
  metrics.physical_width = ANDROID_SAFE_ACCESS(event, width, 0.0);
  metrics.physical_height = ANDROID_SAFE_ACCESS(event, height, 0.0);

  if (ANDROID_SAFE_ACCESS(event, has_constraints, false)) {
    metrics.physical_min_width_constraint = ANDROID_SAFE_ACCESS(
        event, min_width_constraint, metrics.physical_width);
    metrics.physical_max_width_constraint = ANDROID_SAFE_ACCESS(
        event, max_width_constraint, metrics.physical_width);
    metrics.physical_min_height_constraint = ANDROID_SAFE_ACCESS(
        event, min_height_constraint, metrics.physical_height);
    metrics.physical_max_height_constraint = ANDROID_SAFE_ACCESS(
        event, max_height_constraint, metrics.physical_height);
  } else {
    metrics.physical_min_width_constraint = metrics.physical_width;
    metrics.physical_max_width_constraint = metrics.physical_width;
    metrics.physical_min_height_constraint = metrics.physical_height;
    metrics.physical_max_height_constraint = metrics.physical_height;
  }

  if (metrics.physical_width < metrics.physical_min_width_constraint ||
      metrics.physical_width > metrics.physical_max_width_constraint ||
      metrics.physical_height < metrics.physical_min_height_constraint ||
      metrics.physical_height > metrics.physical_max_height_constraint) {
    return kInvalidArguments;
  }

  metrics.device_pixel_ratio = ANDROID_SAFE_ACCESS(event, pixel_ratio, 1.0);
  if (metrics.device_pixel_ratio <= 0.0) {
    return kInvalidArguments;
  }

  metrics.physical_view_inset_top =
      ANDROID_SAFE_ACCESS(event, physical_view_inset_top, 0.0);
  metrics.physical_view_inset_right =
      ANDROID_SAFE_ACCESS(event, physical_view_inset_right, 0.0);
  metrics.physical_view_inset_bottom =
      ANDROID_SAFE_ACCESS(event, physical_view_inset_bottom, 0.0);
  metrics.physical_view_inset_left =
      ANDROID_SAFE_ACCESS(event, physical_view_inset_left, 0.0);

  if (metrics.physical_view_inset_top < 0 ||
      metrics.physical_view_inset_right < 0 ||
      metrics.physical_view_inset_bottom < 0 ||
      metrics.physical_view_inset_left < 0) {
    return kInvalidArguments;
  }

  metrics.display_id = ANDROID_SAFE_ACCESS(event, display_id, 0);

  metrics.physical_padding_top =
      ANDROID_SAFE_ACCESS(event, physical_padding_top, 0.0);
  metrics.physical_padding_right =
      ANDROID_SAFE_ACCESS(event, physical_padding_right, 0.0);
  metrics.physical_padding_bottom =
      ANDROID_SAFE_ACCESS(event, physical_padding_bottom, 0.0);
  metrics.physical_padding_left =
      ANDROID_SAFE_ACCESS(event, physical_padding_left, 0.0);

  if (metrics.physical_padding_top < 0 || metrics.physical_padding_right < 0 ||
      metrics.physical_padding_bottom < 0 ||
      metrics.physical_padding_left < 0) {
    return kInvalidArguments;
  }

  metrics.physical_system_gesture_inset_top =
      ANDROID_SAFE_ACCESS(event, physical_system_gesture_inset_top, 0.0);
  metrics.physical_system_gesture_inset_right =
      ANDROID_SAFE_ACCESS(event, physical_system_gesture_inset_right, 0.0);
  metrics.physical_system_gesture_inset_bottom =
      ANDROID_SAFE_ACCESS(event, physical_system_gesture_inset_bottom, 0.0);
  metrics.physical_system_gesture_inset_left =
      ANDROID_SAFE_ACCESS(event, physical_system_gesture_inset_left, 0.0);

  metrics.physical_touch_slop =
      ANDROID_SAFE_ACCESS(event, physical_touch_slop, -1.0);

  metrics.physical_display_corner_radius_top_left =
      ANDROID_SAFE_ACCESS(event, physical_display_corner_radius_top_left, -1.0);
  metrics.physical_display_corner_radius_top_right = ANDROID_SAFE_ACCESS(
      event, physical_display_corner_radius_top_right, -1.0);
  metrics.physical_display_corner_radius_bottom_right = ANDROID_SAFE_ACCESS(
      event, physical_display_corner_radius_bottom_right, -1.0);
  metrics.physical_display_corner_radius_bottom_left = ANDROID_SAFE_ACCESS(
      event, physical_display_corner_radius_bottom_left, -1.0);

  size_t display_features_count =
      ANDROID_SAFE_ACCESS(event, display_features_count, 0);
  const double* display_features_bounds =
      ANDROID_SAFE_ACCESS(event, display_features_bounds, nullptr);
  const int* display_features_type =
      ANDROID_SAFE_ACCESS(event, display_features_type, nullptr);
  const int* display_features_state =
      ANDROID_SAFE_ACCESS(event, display_features_state, nullptr);

  constexpr size_t kBoundsPerDisplayFeature = 4;
  if (display_features_count > 0) {
    if (display_features_count > SIZE_MAX / kBoundsPerDisplayFeature) {
      return kInvalidArguments;
    }
    if (display_features_bounds == nullptr ||
        display_features_type == nullptr || display_features_state == nullptr) {
      return kInvalidArguments;
    }
    metrics.physical_display_features_bounds.assign(
        display_features_bounds,
        display_features_bounds +
            (display_features_count * kBoundsPerDisplayFeature));
    metrics.physical_display_features_type.assign(
        display_features_type, display_features_type + display_features_count);
    metrics.physical_display_features_state.assign(
        display_features_state,
        display_features_state + display_features_count);
  }

  int64_t view_id = ANDROID_SAFE_ACCESS(event, view_id, kImplicitViewId);
  if (platform_view_) {
    platform_view_->SetViewportMetrics(view_id, metrics);
  }
  return kSuccess;
}

void AndroidEngine::DispatchSemanticsAction(JNIEnv* env,
                                            jint node_id,
                                            jint action,
                                            jobject args,
                                            jint args_position) {
  TRACE_EVENT1("flutter", "AndroidEngine::DispatchSemanticsAction", "path",
               "embedder_api");
  const uint8_t* args_data = nullptr;
  size_t args_size = 0;
  if (args != nullptr && !env->IsSameObject(args, NULL)) {
    args_data = static_cast<const uint8_t*>(env->GetDirectBufferAddress(args));
    args_size = static_cast<size_t>(args_position);
  }
  DispatchSemanticsAction(node_id, static_cast<FlutterSemanticsAction>(action),
                          args_data, args_size);
}

FlutterEngineResult AndroidEngine::DispatchSemanticsAction(
    uint64_t node_id,
    FlutterSemanticsAction action,
    const uint8_t* data,
    size_t data_length) {
  if (data == nullptr && data_length > 0) {
    return kInvalidArguments;
  }

  auto args_vector = (data != nullptr && data_length > 0)
                         ? fml::MallocMapping::Copy(data, data_length)
                         : fml::MallocMapping();

  if (platform_view_) {
    platform_view_->DispatchSemanticsAction(
        kImplicitViewId, node_id, static_cast<flutter::SemanticsAction>(action),
        std::move(args_vector));
  }
  return kSuccess;
}

void AndroidEngine::SetSemanticsEnabled(bool enabled) {
  TRACE_EVENT1("flutter", "AndroidEngine::SetSemanticsEnabled", "path",
               "embedder_api");
  UpdateSemanticsEnabled(enabled);
}

FlutterEngineResult AndroidEngine::UpdateSemanticsEnabled(bool enabled) {
  if (platform_view_) {
    platform_view_->SetSemanticsEnabled(enabled);
  }
  return kSuccess;
}

void AndroidEngine::SetAccessibilityFeatures(int32_t flags) {
  TRACE_EVENT1("flutter", "AndroidEngine::SetAccessibilityFeatures", "path",
               "embedder_api");
  UpdateAccessibilityFeatures(static_cast<FlutterAccessibilityFeature>(flags));
}

FlutterEngineResult AndroidEngine::UpdateAccessibilityFeatures(
    FlutterAccessibilityFeature features) {
  if (platform_view_) {
    platform_view_->SetAccessibilityFeatures(static_cast<int32_t>(features));
  }
  return kSuccess;
}

void AndroidEngine::RegisterExternalTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) {
  TRACE_EVENT2("flutter", "AndroidEngine::RegisterExternalTexture", "mode",
               "SurfaceTexture", "path", "embedder_api");
  RegisterSurfaceExternalTexture(texture_id, surface_texture);
}

FlutterEngineResult AndroidEngine::RegisterSurfaceExternalTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) {
  if (texture_id <= 0 || surface_texture.is_null()) {
    return kInvalidArguments;
  }

  if (external_texture_adapter_) {
    bool success = external_texture_adapter_->RegisterSurfaceTexture(
        texture_id, surface_texture);
    return success ? kSuccess : kInternalInconsistency;
  }
  return kInternalInconsistency;
}

void AndroidEngine::RegisterImageTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    ImageExternalTexture::ImageLifecycle lifecycle) {
  TRACE_EVENT2("flutter", "AndroidEngine::RegisterImageTexture", "mode",
               "SurfaceProducer", "path", "embedder_api");
  RegisterImageExternalTexture(texture_id, image_texture_entry, lifecycle);
}

FlutterEngineResult AndroidEngine::RegisterImageExternalTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    ImageExternalTexture::ImageLifecycle lifecycle) {
  if (texture_id <= 0 || image_texture_entry.is_null()) {
    return kInvalidArguments;
  }

  if (external_texture_adapter_) {
    bool success = external_texture_adapter_->RegisterImageTexture(
        texture_id, image_texture_entry, lifecycle);
    return success ? kSuccess : kInternalInconsistency;
  }
  return kInternalInconsistency;
}

void AndroidEngine::UnregisterTexture(int64_t texture_id) {
  TRACE_EVENT1("flutter", "AndroidEngine::UnregisterTexture", "path",
               "embedder_api");
  UnregisterExternalTexture(texture_id);
}

FlutterEngineResult AndroidEngine::UnregisterExternalTexture(
    int64_t texture_id) {
  if (texture_id <= 0) {
    return kInvalidArguments;
  }
  if (external_texture_adapter_) {
    return external_texture_adapter_->UnregisterExternalTexture(texture_id);
  }
  if (platform_view_) {
    platform_view_->UnregisterTexture(texture_id);
  }
  return kSuccess;
}

void AndroidEngine::MarkTextureFrameAvailable(int64_t texture_id) {
  TRACE_EVENT1("flutter", "AndroidEngine::MarkTextureFrameAvailable", "path",
               "embedder_api");
  MarkExternalTextureFrameAvailable(texture_id);
}

FlutterEngineResult AndroidEngine::MarkExternalTextureFrameAvailable(
    int64_t texture_id) {
  if (texture_id <= 0) {
    return kInvalidArguments;
  }
  if (external_texture_adapter_) {
    return external_texture_adapter_->MarkExternalTextureFrameAvailable(
        texture_id);
  }
  if (platform_view_) {
    platform_view_->MarkTextureFrameAvailable(texture_id);
  }
  return kSuccess;
}

void AndroidEngine::OnDisplayPlatformView(int32_t view_id,
                                          int32_t x,
                                          int32_t y,
                                          int32_t width,
                                          int32_t height,
                                          int32_t view_width,
                                          int32_t view_height,
                                          MutatorsStack mutators_stack) {
  TRACE_EVENT2("flutter", "AndroidEngine::OnDisplayPlatformView", "mode",
               "TLHC", "path", "embedder_api");
  DisplayPlatformView(view_id, x, y, width, height, view_width, view_height,
                      std::move(mutators_stack));
}

FlutterEngineResult AndroidEngine::DisplayPlatformView(
    int32_t view_id,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t view_width,
    int32_t view_height,
    MutatorsStack mutators_stack) {
  if (view_id < 0 || width < 0 || height < 0 || view_width < 0 ||
      view_height < 0) {
    return kInvalidArguments;
  }

  if (jni_facade_) {
    jni_facade_->FlutterViewOnDisplayPlatformView(view_id, x, y, width, height,
                                                  view_width, view_height,
                                                  std::move(mutators_stack));
    return kSuccess;
  }
  return kInternalInconsistency;
}

FlutterEngineResult AndroidEngine::DisplayPlatformViewEmbedder(
    int32_t view_id,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t view_width,
    int32_t view_height,
    MutatorsStack mutators_stack) {
  return DisplayPlatformView(view_id, x, y, width, height, view_width,
                             view_height, std::move(mutators_stack));
}

void AndroidEngine::OnDisplayOverlaySurface(int32_t surface_id,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height) {
  TRACE_EVENT2("flutter", "AndroidEngine::OnDisplayOverlaySurface", "mode",
               "TLHC", "path", "embedder_api");
  DisplayOverlaySurface(surface_id, x, y, width, height);
}

FlutterEngineResult AndroidEngine::DisplayOverlaySurface(int32_t surface_id,
                                                         int32_t x,
                                                         int32_t y,
                                                         int32_t width,
                                                         int32_t height) {
  if (surface_id < 0 || width < 0 || height < 0) {
    return kInvalidArguments;
  }

  if (jni_facade_) {
    jni_facade_->FlutterViewDisplayOverlaySurface(surface_id, x, y, width,
                                                  height);
    return kSuccess;
  }
  return kInternalInconsistency;
}

void AndroidEngine::OnDisplayVirtualDisplayPlatformView(int32_t view_id,
                                                        int32_t x,
                                                        int32_t y,
                                                        int32_t width,
                                                        int32_t height) {
  TRACE_EVENT2("flutter", "AndroidEngine::OnDisplayVirtualDisplayPlatformView",
               "mode", "VD", "path", "embedder_api");
  DisplayVirtualDisplayPlatformView(view_id, x, y, width, height);
}

FlutterEngineResult AndroidEngine::DisplayVirtualDisplayPlatformView(
    int32_t view_id,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height) {
  if (view_id < 0 || width < 0 || height < 0) {
    return kInvalidArguments;
  }
  return kSuccess;
}

FlutterEngineResult AndroidEngine::DisplayVirtualDisplayPlatformViewEmbedder(
    int32_t view_id,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height) {
  return DisplayVirtualDisplayPlatformView(view_id, x, y, width, height);
}

void AndroidEngine::OnDisplayPlatformView2(int32_t view_id,
                                           int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height,
                                           int32_t view_width,
                                           int32_t view_height,
                                           MutatorsStack mutators_stack) {
  TRACE_EVENT2("flutter", "AndroidEngine::OnDisplayPlatformView2", "mode",
               "HCPP", "path", "embedder_api");
  DisplayPlatformView2(view_id, x, y, width, height, view_width, view_height,
                       std::move(mutators_stack));
}

FlutterEngineResult AndroidEngine::DisplayPlatformView2(
    int32_t view_id,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t view_width,
    int32_t view_height,
    MutatorsStack mutators_stack) {
  if (view_id < 0 || width < 0 || height < 0 || view_width < 0 ||
      view_height < 0) {
    return kInvalidArguments;
  }

  if (jni_facade_) {
    jni_facade_->onDisplayPlatformView2(view_id, x, y, width, height,
                                        view_width, view_height,
                                        std::move(mutators_stack));
    return kSuccess;
  }
  return kInternalInconsistency;
}

FlutterEngineResult AndroidEngine::DisplayPlatformView2Embedder(
    int32_t view_id,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t view_width,
    int32_t view_height,
    MutatorsStack mutators_stack) {
  return DisplayPlatformView2(view_id, x, y, width, height, view_width,
                              view_height, std::move(mutators_stack));
}

void AndroidEngine::OnHidePlatformView2(int32_t view_id) {
  TRACE_EVENT2("flutter", "AndroidEngine::OnHidePlatformView2", "mode", "HCPP",
               "path", "embedder_api");
  HidePlatformView2(view_id);
}

FlutterEngineResult AndroidEngine::HidePlatformView2(int32_t view_id) {
  if (view_id < 0) {
    return kInvalidArguments;
  }

  if (jni_facade_) {
    jni_facade_->hidePlatformView2(view_id);
    return kSuccess;
  }
  return kInternalInconsistency;
}

void AndroidEngine::BeginFrameHC() {
  TRACE_EVENT2("flutter", "AndroidEngine::BeginFrameHC", "mode", "HC", "path",
               "embedder_api");
  BeginFrameHCEmbedder();
}

FlutterEngineResult AndroidEngine::BeginFrameHCEmbedder() {
  if (jni_facade_) {
    jni_facade_->FlutterViewBeginFrame();
    return kSuccess;
  }
  return kInternalInconsistency;
}

void AndroidEngine::EndFrameHC() {
  TRACE_EVENT2("flutter", "AndroidEngine::EndFrameHC", "mode", "HC", "path",
               "embedder_api");
  EndFrameHCEmbedder();
}

FlutterEngineResult AndroidEngine::EndFrameHCEmbedder() {
  if (jni_facade_) {
    jni_facade_->FlutterViewEndFrame();
    return kSuccess;
  }
  return kInternalInconsistency;
}

std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>
AndroidEngine::CreateOverlaySurfaceHC() {
  TRACE_EVENT2("flutter", "AndroidEngine::CreateOverlaySurfaceHC", "mode", "HC",
               "path", "embedder_api");
  std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata> metadata;
  CreateOverlaySurfaceHCEmbedder(&metadata);
  return metadata;
}

FlutterEngineResult AndroidEngine::CreateOverlaySurfaceHCEmbedder(
    std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>* out_metadata) {
  if (!out_metadata) {
    return kInvalidArguments;
  }
  if (jni_facade_) {
    *out_metadata = jni_facade_->FlutterViewCreateOverlaySurface();
    return kSuccess;
  }
  return kInternalInconsistency;
}

void AndroidEngine::DestroyOverlaySurfacesHC() {
  TRACE_EVENT2("flutter", "AndroidEngine::DestroyOverlaySurfacesHC", "mode",
               "HC", "path", "embedder_api");
  DestroyOverlaySurfacesHCEmbedder();
}

FlutterEngineResult AndroidEngine::DestroyOverlaySurfacesHCEmbedder() {
  if (jni_facade_) {
    jni_facade_->FlutterViewDestroyOverlaySurfaces();
    return kSuccess;
  }
  return kInternalInconsistency;
}

void AndroidEngine::ScheduleFrame() {
  if (platform_view_) {
    platform_view_->ScheduleFrame();
  }
}

void AndroidEngine::UpdateSemantics(
    int64_t view_id,
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  platform_view_android_delegate_.UpdateSemantics(update, actions);
}

void AndroidEngine::SetApplicationLocale(std::string locale) {
  if (jni_facade_) {
    jni_facade_->FlutterViewSetApplicationLocale(std::move(locale));
  }
}

void AndroidEngine::SetSemanticsTreeEnabled(bool enabled) {
  if (jni_facade_) {
    jni_facade_->FlutterViewSetSemanticsTreeEnabled(enabled);
  }
}

void AndroidEngine::HandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  if (platform_message_handler_) {
    platform_message_handler_->HandlePlatformMessage(std::move(message));
  }
}

void AndroidEngine::OnPreEngineRestart() const {
  if (jni_facade_) {
    jni_facade_->FlutterViewOnPreEngineRestart();
  }
}

std::unique_ptr<VsyncWaiter> AndroidEngine::CreateVSyncWaiter() {
  if (!task_runners_) {
    return nullptr;
  }
  return std::make_unique<VsyncWaiterAndroid>(task_runners_->GetTaskRunners());
}

std::unique_ptr<Surface> AndroidEngine::CreateRenderingSurface() {
  if (!android_surface_ || !android_context_) {
    return nullptr;
  }
  return android_surface_->CreateGPUSurface(
      android_context_->GetMainSkiaContext().get());
}

std::shared_ptr<ExternalViewEmbedder>
AndroidEngine::CreateExternalViewEmbedder() {
  if (!android_context_ || !task_runners_) {
    return nullptr;
  }
  auto view_embedder = std::make_shared<AndroidExternalViewEmbedderWrapper>(
      android_meets_hcpp_criteria_, *android_context_, jni_facade_,
      surface_factory_, task_runners_->GetTaskRunners());
  compositor_adapter_ =
      std::make_shared<AndroidCompositorAdapter>(std::move(view_embedder));
  return compositor_adapter_;
}

std::unique_ptr<SnapshotSurfaceProducer>
AndroidEngine::CreateSnapshotSurfaceProducer() {
  if (!android_surface_) {
    return nullptr;
  }
  return std::make_unique<AndroidSnapshotSurfaceProducer>(*android_surface_);
}

sk_sp<GrDirectContext> AndroidEngine::CreateResourceContext() const {
  if (!android_surface_) {
    return nullptr;
  }
#if !SLIMPELLER
  sk_sp<GrDirectContext> resource_context;
  if (android_surface_->ResourceContextMakeCurrent()) {
    resource_context = ShellIOManager::CreateCompatibleResourceLoadingContext(
        GrBackendApi::kOpenGL,
        GPUSurfaceGLDelegate::GetDefaultPlatformGLInterface());
  } else {
    FML_DLOG(ERROR) << "Could not make the resource context current.";
  }
  return resource_context;
#else
  android_surface_->ResourceContextMakeCurrent();
  return nullptr;
#endif  // !SLIMPELLER
}

void AndroidEngine::ReleaseResourceContext() const {
  if (android_surface_) {
    android_surface_->ResourceContextClearCurrent();
  }
}

std::shared_ptr<impeller::Context> AndroidEngine::GetImpellerContext() const {
  if (android_surface_) {
    return android_surface_->GetImpellerContext();
  }
  if (android_context_) {
    return android_context_->GetImpellerContext();
  }
  return nullptr;
}

std::unique_ptr<std::vector<std::string>>
AndroidEngine::ComputePlatformResolvedLocales(
    const std::vector<std::string>& supported_locale_data) {
  if (jni_facade_) {
    return jni_facade_->FlutterViewComputePlatformResolvedLocale(
        supported_locale_data);
  }
  return nullptr;
}

void AndroidEngine::RequestDartDeferredLibrary(intptr_t loading_unit_id) {
  if (jni_facade_) {
    jni_facade_->RequestDartDeferredLibrary(loading_unit_id);
  }
}

void AndroidEngine::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  if (platform_view_) {
    platform_view_->LoadDartDeferredLibrary(loading_unit_id,
                                            std::move(snapshot_data),
                                            std::move(snapshot_instructions));
  }
}

void AndroidEngine::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  if (platform_view_) {
    platform_view_->LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                                 transient);
  }
}

void AndroidEngine::UpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  if (platform_view_) {
    platform_view_->UpdateAssetResolverByType(std::move(updated_asset_resolver),
                                              type);
  }
}

void AndroidEngine::InstallFirstFrameCallback() {
  if (platform_view_ && task_runners_) {
    platform_view_->SetNextFrameCallback(
        [engine = this,
         platform_task_runner =
             task_runners_->GetTaskRunners().GetPlatformTaskRunner()]() {
          platform_task_runner->PostTask([engine]() {
            if (engine) {
              engine->FireFirstFrameCallback();
            }
          });
        });
  }
}

void AndroidEngine::FireFirstFrameCallback() {
  if (jni_facade_) {
    jni_facade_->FlutterViewOnFirstFrame();
  }
}

double AndroidEngine::GetScaledFontSize(double unscaled_font_size,
                                        int configuration_id) const {
  if (jni_facade_) {
    return jni_facade_->FlutterViewGetScaledFontSize(unscaled_font_size,
                                                     configuration_id);
  }
  return unscaled_font_size;
}

}  // namespace flutter
