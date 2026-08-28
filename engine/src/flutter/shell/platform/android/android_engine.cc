// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_engine.h"

#include <EGL/egl.h>
#include <cstring>
#include <utility>

#include "flutter/fml/logging.h"

namespace flutter {

namespace {

FlutterPointerPhase ToPointerPhase(int64_t change) {
  switch (change) {
    case 0:  // cancel
      return FlutterPointerPhase::kCancel;
    case 1:  // add
      return FlutterPointerPhase::kAdd;
    case 2:  // remove
      return FlutterPointerPhase::kRemove;
    case 3:  // hover
      return FlutterPointerPhase::kHover;
    case 4:  // down
      return FlutterPointerPhase::kDown;
    case 5:  // move
      return FlutterPointerPhase::kMove;
    case 6:  // up
      return FlutterPointerPhase::kUp;
    case 7:  // pan_zoom_start
      return FlutterPointerPhase::kPanZoomStart;
    case 8:  // pan_zoom_update
      return FlutterPointerPhase::kPanZoomUpdate;
    case 9:  // pan_zoom_end
      return FlutterPointerPhase::kPanZoomEnd;
    default:
      return FlutterPointerPhase::kCancel;
  }
}

FlutterPointerDeviceKind ToPointerDeviceKind(int64_t kind) {
  switch (kind) {
    case 0:  // touch
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch;
    case 1:  // mouse
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindMouse;
    case 2:  // stylus
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindStylus;
    case 3:  // inverted_stylus
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindInvertedStylus;
    case 4:  // trackpad
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTrackpad;
    default:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch;
  }
}

FlutterPointerSignalKind ToPointerSignalKind(int64_t signal_kind) {
  switch (signal_kind) {
    case 0:  // none
      return FlutterPointerSignalKind::kFlutterPointerSignalKindNone;
    case 1:  // scroll
      return FlutterPointerSignalKind::kFlutterPointerSignalKindScroll;
    case 2:  // scroll_inertia_cancel
      return FlutterPointerSignalKind::
          kFlutterPointerSignalKindScrollInertiaCancel;
    case 3:  // scale
      return FlutterPointerSignalKind::kFlutterPointerSignalKindScale;
    default:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindNone;
  }
}

static constexpr int kEmptyStringIndex = -1;
static constexpr size_t kBytesPerNode = 73 * sizeof(int32_t);
static constexpr size_t kBytesPerChild = sizeof(int32_t);
static constexpr size_t kBytesPerCustomAction = sizeof(int32_t);
static constexpr size_t kBytesPerAction = 4 * sizeof(int32_t);
static constexpr size_t kBytesPerStringAttribute = 4 * sizeof(int32_t);

static void PutStringIntoBuffer(const char* string,
                                int32_t* buffer,
                                size_t* position,
                                std::vector<std::string>& strings) {
  if (string == nullptr || string[0] == '\0') {
    buffer[(*position)++] = kEmptyStringIndex;
  } else {
    buffer[(*position)++] = static_cast<int32_t>(strings.size());
    strings.emplace_back(string);
  }
}

static void PutStringAttributesIntoBuffer(
    const FlutterStringAttribute** attributes,
    size_t attribute_count,
    int32_t* buffer,
    size_t* position,
    std::vector<std::vector<uint8_t>>& string_attribute_args) {
  if (attributes == nullptr || attribute_count == 0) {
    buffer[(*position)++] = kEmptyStringIndex;
    return;
  }
  buffer[(*position)++] = static_cast<int32_t>(attribute_count);
  for (size_t i = 0; i < attribute_count; ++i) {
    const FlutterStringAttribute* attribute = attributes[i];
    if (attribute == nullptr) {
      continue;
    }
    buffer[(*position)++] = static_cast<int32_t>(attribute->start);
    buffer[(*position)++] = static_cast<int32_t>(attribute->end);
    buffer[(*position)++] = static_cast<int32_t>(attribute->type);
    switch (attribute->type) {
      case kSpellOut:
        buffer[(*position)++] = kEmptyStringIndex;
        break;
      case kLocale: {
        buffer[(*position)++] =
            static_cast<int32_t>(string_attribute_args.size());
        if (attribute->locale != nullptr &&
            attribute->locale->locale != nullptr) {
          std::string loc(attribute->locale->locale);
          string_attribute_args.push_back({loc.begin(), loc.end()});
        } else {
          string_attribute_args.emplace_back();
        }
        break;
      }
    }
  }
}

static int64_t ConvertFlagsToInt64(const FlutterSemanticsFlags* flags2,
                                   FlutterSemanticsFlag legacy_flags) {
  if (flags2 != nullptr) {
    int64_t result = 0;
    if (flags2->is_checked != kFlutterCheckStateNone) {
      result |= (INT64_C(1) << 0);
    }
    if (flags2->is_checked == kFlutterCheckStateTrue) {
      result |= (INT64_C(1) << 1);
    }
    if (flags2->is_selected == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 2);
    }
    if (flags2->is_button) {
      result |= (INT64_C(1) << 3);
    }
    if (flags2->is_text_field) {
      result |= (INT64_C(1) << 4);
    }
    if (flags2->is_focused == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 5);
    }
    if (flags2->is_enabled != kFlutterTristateNone) {
      result |= (INT64_C(1) << 6);
    }
    if (flags2->is_enabled == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 7);
    }
    if (flags2->is_in_mutually_exclusive_group) {
      result |= (INT64_C(1) << 8);
    }
    if (flags2->is_header) {
      result |= (INT64_C(1) << 9);
    }
    if (flags2->is_obscured) {
      result |= (INT64_C(1) << 10);
    }
    if (flags2->scopes_route) {
      result |= (INT64_C(1) << 11);
    }
    if (flags2->names_route) {
      result |= (INT64_C(1) << 12);
    }
    if (flags2->is_hidden) {
      result |= (INT64_C(1) << 13);
    }
    if (flags2->is_image) {
      result |= (INT64_C(1) << 14);
    }
    if (flags2->is_live_region) {
      result |= (INT64_C(1) << 15);
    }
    if (flags2->is_toggled != kFlutterTristateNone) {
      result |= (INT64_C(1) << 16);
    }
    if (flags2->is_toggled == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 17);
    }
    if (flags2->has_implicit_scrolling) {
      result |= (INT64_C(1) << 18);
    }
    if (flags2->is_multiline) {
      result |= (INT64_C(1) << 19);
    }
    if (flags2->is_read_only) {
      result |= (INT64_C(1) << 20);
    }
    if (flags2->is_focused != kFlutterTristateNone) {
      result |= (INT64_C(1) << 21);
    }
    if (flags2->is_link) {
      result |= (INT64_C(1) << 22);
    }
    if (flags2->is_slider) {
      result |= (INT64_C(1) << 23);
    }
    if (flags2->is_keyboard_key) {
      result |= (INT64_C(1) << 24);
    }
    if (flags2->is_checked == kFlutterCheckStateMixed) {
      result |= (INT64_C(1) << 25);
    }
    if (flags2->is_expanded != kFlutterTristateNone) {
      result |= (INT64_C(1) << 26);
    }
    if (flags2->is_expanded == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 27);
    }
    if (flags2->is_selected != kFlutterTristateNone) {
      result |= (INT64_C(1) << 28);
    }
    if (flags2->is_required != kFlutterTristateNone) {
      result |= (INT64_C(1) << 29);
    }
    if (flags2->is_required == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 30);
    }
    if (flags2->is_accessibility_focus_blocked) {
      result |= (INT64_C(1) << 31);
    }
    return result;
  }
  return static_cast<int64_t>(legacy_flags);
}

}  // namespace

AndroidEngine::AndroidEngine(const flutter::Settings& settings,
                             std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                             AndroidRenderingAPI rendering_api,
                             fml::RefPtr<fml::TaskRunner> platform_task_runner,
                             fml::RefPtr<fml::TaskRunner> raster_task_runner)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      rendering_api_(rendering_api),
      platform_task_runner_(platform_task_runner),
      raster_task_runner_(raster_task_runner),
      platform_runner_context_{this, platform_task_runner_},
      raster_runner_context_{this, raster_task_runner_},
      surface_manager_(std::make_shared<AndroidSurfaceManager>(rendering_api_)),
      compositor_(std::make_unique<AndroidCompositor>(
          surface_manager_,
          jni_facade_,
          std::move(raster_task_runner),
          std::move(platform_task_runner))) {
  InitializeProcTable(nullptr);
  is_valid_.store(embedder_api_.Initialize != nullptr);
}

AndroidEngine::AndroidEngine(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    std::shared_ptr<AndroidSurfaceManager> surface_manager,
    std::unique_ptr<AndroidCompositor> compositor,
    const FlutterEngineProcTable* embedder_api_override,
    fml::RefPtr<fml::TaskRunner> platform_task_runner,
    fml::RefPtr<fml::TaskRunner> raster_task_runner)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      rendering_api_(surface_manager ? surface_manager->GetRenderingAPI()
                                     : AndroidRenderingAPI::kSoftware),
      platform_task_runner_(std::move(platform_task_runner)),
      raster_task_runner_(std::move(raster_task_runner)),
      platform_runner_context_{this, platform_task_runner_},
      raster_runner_context_{this, raster_task_runner_},
      surface_manager_(std::move(surface_manager)),
      compositor_(std::move(compositor)) {
  InitializeProcTable(embedder_api_override);
  is_valid_.store(embedder_api_.Initialize != nullptr);
}

AndroidEngine::AndroidEngine(
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    std::shared_ptr<AndroidSurfaceManager> surface_manager,
    std::unique_ptr<AndroidCompositor> compositor,
    const FlutterEngineProcTable& embedder_api)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      rendering_api_(surface_manager ? surface_manager->GetRenderingAPI()
                                     : AndroidRenderingAPI::kSoftware),
      platform_runner_context_{this, nullptr},
      raster_runner_context_{this, nullptr},
      surface_manager_(std::move(surface_manager)),
      compositor_(std::move(compositor)),
      embedder_api_(embedder_api),
      engine_(engine),
      is_valid_(engine != nullptr),
      is_running_(engine != nullptr),
      is_initialized_(engine != nullptr) {}

AndroidEngine::~AndroidEngine() {
  std::lock_guard engine_lock(engine_mutex_);
  is_running_.store(false);
  is_valid_.store(false);

  {
    std::lock_guard lock(pending_responses_mutex_);
    for (const auto& [id, handle] : pending_incoming_responses_) {
      if (engine_ != nullptr &&
          embedder_api_.SendPlatformMessageResponse != nullptr) {
        embedder_api_.SendPlatformMessageResponse(engine_, handle, nullptr, 0);
      }
    }
    pending_incoming_responses_.clear();
  }

  if (engine_ != nullptr) {
    if (embedder_api_.Shutdown != nullptr) {
      embedder_api_.Shutdown(engine_);
    } else if (embedder_api_.Deinitialize != nullptr) {
      embedder_api_.Deinitialize(engine_);
    }
    engine_ = nullptr;
  }
  is_initialized_.store(false);
}

void AndroidEngine::InitializeProcTable(
    const FlutterEngineProcTable* override_table) {
  if (override_table != nullptr) {
    embedder_api_ = *override_table;
  } else {
    memset(&embedder_api_, 0, sizeof(FlutterEngineProcTable));
    embedder_api_.struct_size = sizeof(FlutterEngineProcTable);
    FlutterEngineGetProcAddresses(&embedder_api_);
  }
}

bool AndroidEngine::IsValid() const {
  return is_valid_.load();
}

bool AndroidEngine::IsRunning() const {
  return is_running_.load();
}

bool AndroidEngine::IsEmbedderAPIEnabled() const {
  return FlutterMain::IsEmbedderAPIEnabled();
}

const flutter::Settings& AndroidEngine::GetSettings() const {
  return settings_;
}

AndroidRenderingAPI AndroidEngine::GetRenderingAPI() const {
  return rendering_api_;
}

FlutterRendererConfig AndroidEngine::CreateRendererConfig() {
  FlutterRendererConfig config = {};
  switch (rendering_api_) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      config.type = kSoftware;
      config.software.struct_size = sizeof(FlutterSoftwareRendererConfig);
      config.software.surface_present_callback =
          [](void* user_data, const void* allocation, size_t row_bytes,
             size_t height) -> bool { return true; };
      break;
    case AndroidRenderingAPI::kSkiaOpenGLES:
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
    case AndroidRenderingAPI::kImpellerAutoselect:
    case AndroidRenderingAPI::kImpellerVulkan:
      config.type = kOpenGL;
      config.open_gl.struct_size = sizeof(FlutterOpenGLRendererConfig);
      config.open_gl.make_current = [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine && engine->GetSurfaceManager() &&
               engine->GetSurfaceManager()->MakeCurrent();
      };
      config.open_gl.clear_current = [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine && engine->GetSurfaceManager() &&
               engine->GetSurfaceManager()->ClearCurrent();
      };
      config.open_gl.make_resource_current = [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine && engine->GetSurfaceManager() &&
               engine->GetSurfaceManager()->MakeResourceCurrent();
      };
      config.open_gl.present = [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine && engine->GetSurfaceManager() &&
               engine->GetSurfaceManager()->SwapBuffers();
      };
      config.open_gl.fbo_callback = [](void* user_data) -> uint32_t {
        return 0;
      };
      config.open_gl.gl_proc_resolver = [](void* user_data,
                                           const char* name) -> void* {
        return reinterpret_cast<void*>(eglGetProcAddress(name));
      };
      config.open_gl.setup_callback = [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine && engine->GetSurfaceManager() &&
               engine->GetSurfaceManager()->MakeResourceCurrent();
      };
      config.open_gl.teardown_callback = [](void* user_data) {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        if (engine && engine->GetSurfaceManager()) {
          engine->GetSurfaceManager()->ClearResourceCurrent();
        }
      };
      break;
  }
  return config;
}

bool AndroidEngine::Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
                           const std::string& entrypoint,
                           const std::string& library_url,
                           const std::vector<std::string>& entrypoint_args,
                           int64_t engine_id) {
  std::lock_guard engine_lock(engine_mutex_);
  if (is_running_.load()) {
    FML_LOG(WARNING) << "AndroidEngine is already running.";
    return true;
  }

  apk_asset_provider_ = std::move(apk_asset_provider);

  FlutterRendererConfig renderer_config = CreateRendererConfig();
  FlutterProjectArgs args = {};
  args.struct_size = sizeof(FlutterProjectArgs);

  FlutterAssetResolver asset_resolver;
  const FlutterAssetResolver* resolvers[1] = {nullptr};
  if (apk_asset_provider_ != nullptr) {
    asset_resolver = apk_asset_provider_->GetAssetResolverConfig();
    resolvers[0] = &asset_resolver;
    args.asset_resolvers = resolvers;
    args.asset_resolvers_count = 1;
  }

  FlutterCustomTaskRunners task_runners = {};
  task_runners.struct_size = sizeof(FlutterCustomTaskRunners);
  task_runners.thread_priority_setter = &AndroidSetThreadPriority;

  FlutterTaskRunnerDescription platform_runner_desc = {};
  FlutterTaskRunnerDescription raster_runner_desc = {};

  if (platform_task_runner_) {
    platform_runner_desc.struct_size = sizeof(FlutterTaskRunnerDescription);
    platform_runner_desc.user_data = &platform_runner_context_;
    platform_runner_desc.runs_task_on_current_thread_callback =
        [](void* user_data) -> bool {
      auto* ctx = static_cast<TaskRunnerContext*>(user_data);
      return ctx && ctx->runner ? ctx->runner->RunsTasksOnCurrentThread()
                                : false;
    };
    platform_runner_desc.post_task_callback =
        [](FlutterTask task, uint64_t target_time_nanos, void* user_data) {
          auto* ctx = static_cast<TaskRunnerContext*>(user_data);
          if (ctx && ctx->runner && ctx->engine) {
            fml::TimePoint target_time = fml::TimePoint::FromEpochDelta(
                fml::TimeDelta::FromNanoseconds(target_time_nanos));
            ctx->runner->PostTaskForTime(
                [engine = ctx->engine, task]() {
                  auto handle = engine->GetEngineHandle();
                  if (handle != nullptr) {
                    if (engine->GetEmbedderAPI().RunTask != nullptr) {
                      engine->GetEmbedderAPI().RunTask(handle, &task);
                    } else {
                      FlutterEngineRunTask(handle, &task);
                    }
                  }
                },
                target_time);
          }
        };
    platform_runner_desc.identifier =
        reinterpret_cast<size_t>(platform_task_runner_.get());
    task_runners.platform_task_runner = &platform_runner_desc;
  }

  if (raster_task_runner_) {
    raster_runner_desc.struct_size = sizeof(FlutterTaskRunnerDescription);
    raster_runner_desc.user_data = &raster_runner_context_;
    raster_runner_desc.runs_task_on_current_thread_callback =
        [](void* user_data) -> bool {
      auto* ctx = static_cast<TaskRunnerContext*>(user_data);
      return ctx && ctx->runner ? ctx->runner->RunsTasksOnCurrentThread()
                                : false;
    };
    raster_runner_desc.post_task_callback =
        [](FlutterTask task, uint64_t target_time_nanos, void* user_data) {
          auto* ctx = static_cast<TaskRunnerContext*>(user_data);
          if (ctx && ctx->runner && ctx->engine) {
            fml::TimePoint target_time = fml::TimePoint::FromEpochDelta(
                fml::TimeDelta::FromNanoseconds(target_time_nanos));
            ctx->runner->PostTaskForTime(
                [engine = ctx->engine, task]() {
                  auto handle = engine->GetEngineHandle();
                  if (handle != nullptr) {
                    if (engine->GetEmbedderAPI().RunTask != nullptr) {
                      engine->GetEmbedderAPI().RunTask(handle, &task);
                    } else {
                      FlutterEngineRunTask(handle, &task);
                    }
                  }
                },
                target_time);
          }
        };
    raster_runner_desc.identifier =
        reinterpret_cast<size_t>(raster_task_runner_.get());
    task_runners.render_task_runner = &raster_runner_desc;
  }

  args.custom_task_runners = &task_runners;

  FlutterCompositor compositor_config;
  if (compositor_ != nullptr) {
    compositor_config = compositor_->GetCompositorConfig();
    args.compositor = &compositor_config;
  }

  args.platform_message_callback = &AndroidEngine::OnPlatformMessageCallback;
  args.root_isolate_create_callback =
      &AndroidEngine::OnRootIsolateCreatedCallback;
  args.update_semantics_callback2 = &AndroidEngine::OnUpdateSemantics2Callback;
  args.request_dart_deferred_library_callback =
      &AndroidEngine::OnRequestDartDeferredLibraryCallback;

  if (!entrypoint.empty()) {
    args.custom_dart_entrypoint = entrypoint.c_str();
  }

  std::vector<const char*> argv_ptrs;
  argv_ptrs.reserve(entrypoint_args.size());
  for (const auto& arg : entrypoint_args) {
    argv_ptrs.push_back(arg.c_str());
  }
  if (!argv_ptrs.empty()) {
    args.dart_entrypoint_argc = static_cast<int>(argv_ptrs.size());
    args.dart_entrypoint_argv = argv_ptrs.data();
  }

  args.engine_id = engine_id;

  std::string icu_path = settings_.icu_data_path;
  if (!icu_path.empty()) {
    args.icu_data_path = icu_path.c_str();
  }

  std::vector<std::string> cmd_args = FlutterMain::Get().GetCommandLineArgs();
  std::vector<const char*> cmd_ptrs;
  cmd_ptrs.reserve(cmd_args.size());
  for (const auto& a : cmd_args) {
    cmd_ptrs.push_back(a.c_str());
  }
  if (!cmd_ptrs.empty()) {
    args.command_line_argc = static_cast<int>(cmd_ptrs.size());
    args.command_line_argv = cmd_ptrs.data();
  }

  if (embedder_api_.Initialize == nullptr) {
    FML_LOG(ERROR) << "Embedder API Initialize pointer is null.";
    is_valid_.store(false);
    is_running_.store(false);
    return false;
  }

  auto init_result = embedder_api_.Initialize(
      FLUTTER_ENGINE_VERSION, &renderer_config, &args, this, &engine_);
  if (init_result != kSuccess || engine_ == nullptr) {
    FML_LOG(ERROR) << "FlutterEngineInitialize failed with error code: "
                   << init_result;
    is_valid_.store(false);
    is_running_.store(false);
    return false;
  }

  is_initialized_.store(true);

  FlutterEngineResult run_result = kSuccess;
  if (embedder_api_.RunInitialized != nullptr) {
    run_result = embedder_api_.RunInitialized(engine_);
  }

  if (run_result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineRunInitialized failed with error code: "
                   << run_result;
    is_valid_.store(false);
    is_running_.store(false);
    return false;
  }

  is_running_.store(true);
  is_valid_.store(true);
  return true;
}

std::unique_ptr<AndroidEngine> AndroidEngine::Spawn(
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    const std::string& entrypoint,
    const std::string& library_url,
    const std::string& initial_route,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) const {
  std::lock_guard engine_lock(engine_mutex_);
  if (!is_running_.load() || engine_ == nullptr ||
      embedder_api_.Spawn == nullptr) {
    FML_LOG(ERROR) << "Cannot spawn from uninitialized or non-running engine.";
    return nullptr;
  }

  auto spawned_surface_manager =
      std::make_shared<AndroidSurfaceManager>(rendering_api_);
  auto spawned_compositor =
      std::make_unique<AndroidCompositor>(spawned_surface_manager, jni_facade);

  auto spawned_engine = std::make_unique<AndroidEngine>(
      settings_, jni_facade, spawned_surface_manager,
      std::move(spawned_compositor), &embedder_api_);

  FlutterEngineSpawnInfo spawn_info = {};
  spawn_info.struct_size = sizeof(FlutterEngineSpawnInfo);
  spawn_info.entrypoint = entrypoint.empty() ? nullptr : entrypoint.c_str();
  spawn_info.library_uri = library_url.empty() ? nullptr : library_url.c_str();
  spawn_info.initial_route =
      initial_route.empty() ? nullptr : initial_route.c_str();

  std::vector<const char*> argv_ptrs;
  argv_ptrs.reserve(entrypoint_args.size());
  for (const auto& arg : entrypoint_args) {
    argv_ptrs.push_back(arg.c_str());
  }
  spawn_info.entrypoint_argc = static_cast<int64_t>(argv_ptrs.size());
  spawn_info.entrypoint_argv = argv_ptrs.empty() ? nullptr : argv_ptrs.data();
  spawn_info.engine_id = engine_id;
  spawn_info.user_data = spawned_engine.get();

  FlutterAssetResolver asset_resolver;
  const FlutterAssetResolver* resolvers[1] = {nullptr};
  if (apk_asset_provider_ != nullptr) {
    asset_resolver = apk_asset_provider_->GetAssetResolverConfig();
    resolvers[0] = &asset_resolver;
    spawn_info.asset_resolvers = resolvers;
    spawn_info.asset_resolvers_count = 1;
  }

  FLUTTER_API_SYMBOL(FlutterEngine) spawned_handle = nullptr;
  auto result = embedder_api_.Spawn(engine_, &spawn_info, &spawned_handle);
  if (result != kSuccess || spawned_handle == nullptr) {
    FML_LOG(ERROR) << "FlutterEngineSpawn failed with error code: " << result;
    return nullptr;
  }

  spawned_engine->AttachSpawnedEngine(spawned_handle);
  return spawned_engine;
}

void AndroidEngine::AttachSpawnedEngine(FLUTTER_API_SYMBOL(FlutterEngine)
                                            engine) {
  std::lock_guard lock(engine_mutex_);
  engine_ = engine;
  is_valid_.store(engine != nullptr);
  is_running_.store(engine != nullptr);
  is_initialized_.store(engine != nullptr);
}

void AndroidEngine::OnSurfaceCreated(fml::RefPtr<AndroidNativeWindow> window) {
  if (compositor_ != nullptr) {
    compositor_->OnSurfaceCreated(std::move(window));
  }
}

void AndroidEngine::OnSurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> window) {
  if (compositor_ != nullptr) {
    compositor_->OnSurfaceWindowChanged(std::move(window));
  }
}

void AndroidEngine::OnSurfaceResized(size_t width, size_t height) {
  if (compositor_ != nullptr) {
    compositor_->OnSurfaceResized(
        FlutterSize{static_cast<double>(width), static_cast<double>(height)});
  }
}

void AndroidEngine::OnSurfaceDestroyed() {
  if (compositor_ != nullptr) {
    compositor_->OnSurfaceDestroyed();
  }
}

void AndroidEngine::SetViewportMetrics(int64_t view_id,
                                       const AndroidViewportMetrics& metrics) {
  if (compositor_ != nullptr) {
    compositor_->SetDevicePixelRatio(metrics.device_pixel_ratio);
  }

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.view_id = view_id;
  event.width = static_cast<size_t>(metrics.physical_width);
  event.height = static_cast<size_t>(metrics.physical_height);
  event.pixel_ratio = metrics.device_pixel_ratio;
  event.physical_view_inset_top = metrics.physical_view_inset_top;
  event.physical_view_inset_right = metrics.physical_view_inset_right;
  event.physical_view_inset_bottom = metrics.physical_view_inset_bottom;
  event.physical_view_inset_left = metrics.physical_view_inset_left;
  event.display_id = metrics.display_id;

  event.has_constraints =
      (metrics.physical_min_width > 0 || metrics.physical_max_width > 0 ||
       metrics.physical_min_height > 0 || metrics.physical_max_height > 0);
  event.min_width_constraint = static_cast<size_t>(metrics.physical_min_width);
  event.max_width_constraint =
      metrics.physical_max_width > 0
          ? static_cast<size_t>(metrics.physical_max_width)
          : event.width;
  event.min_height_constraint =
      static_cast<size_t>(metrics.physical_min_height);
  event.max_height_constraint =
      metrics.physical_max_height > 0
          ? static_cast<size_t>(metrics.physical_max_height)
          : event.height;

  SetViewportMetrics(event);
}

void AndroidEngine::SetViewportMetrics(const FlutterWindowMetricsEvent& event) {
  if (compositor_ != nullptr) {
    compositor_->SetDevicePixelRatio(event.pixel_ratio);
  }
  std::lock_guard engine_lock(engine_mutex_);
  if (is_running_.load() && engine_ != nullptr &&
      embedder_api_.SendWindowMetricsEvent != nullptr) {
    embedder_api_.SendWindowMetricsEvent(engine_, &event);
  }
}

void AndroidEngine::UpdateDisplayMetrics() {
  // Display updates can be communicated via NotifyDisplayUpdate if needed.
}

void AndroidEngine::DispatchPointerDataPacket(const uint8_t* buffer,
                                              size_t size) {
  constexpr size_t kBytesPerRecord = 288;
  if (buffer == nullptr || size == 0 || size % kBytesPerRecord != 0) {
    return;
  }

  size_t count = size / kBytesPerRecord;
  std::vector<FlutterPointerEvent> events(count);

  for (size_t i = 0; i < count; ++i) {
    const uint8_t* ptr = buffer + i * kBytesPerRecord;

    int64_t time_stamp = 0;
    int64_t change = 0;
    int64_t kind = 0;
    int64_t signal_kind = 0;
    int64_t device = 0;
    double physical_x = 0.0;
    double physical_y = 0.0;
    int64_t buttons = 0;
    double pressure = 0.0;
    double pressure_min = 0.0;
    double pressure_max = 1.0;
    double scroll_delta_x = 0.0;
    double scroll_delta_y = 0.0;
    double pan_x = 0.0;
    double pan_y = 0.0;
    double scale = 1.0;
    double rotation = 0.0;
    int64_t view_id = 0;

    memcpy(&time_stamp, ptr + 8, sizeof(int64_t));
    memcpy(&change, ptr + 16, sizeof(int64_t));
    memcpy(&kind, ptr + 24, sizeof(int64_t));
    memcpy(&signal_kind, ptr + 32, sizeof(int64_t));
    memcpy(&device, ptr + 40, sizeof(int64_t));
    memcpy(&physical_x, ptr + 56, sizeof(double));
    memcpy(&physical_y, ptr + 64, sizeof(double));
    memcpy(&buttons, ptr + 88, sizeof(int64_t));
    memcpy(&pressure, ptr + 112, sizeof(double));
    memcpy(&pressure_min, ptr + 120, sizeof(double));
    memcpy(&pressure_max, ptr + 128, sizeof(double));
    memcpy(&scroll_delta_x, ptr + 216, sizeof(double));
    memcpy(&scroll_delta_y, ptr + 224, sizeof(double));
    memcpy(&pan_x, ptr + 232, sizeof(double));
    memcpy(&pan_y, ptr + 240, sizeof(double));
    memcpy(&scale, ptr + 264, sizeof(double));
    memcpy(&rotation, ptr + 272, sizeof(double));
    memcpy(&view_id, ptr + 280, sizeof(int64_t));

    events[i].struct_size = sizeof(FlutterPointerEvent);
    events[i].timestamp = time_stamp;
    events[i].phase = ToPointerPhase(change);
    events[i].device_kind = ToPointerDeviceKind(kind);
    events[i].signal_kind = ToPointerSignalKind(signal_kind);
    events[i].device = device;
    events[i].x = physical_x;
    events[i].y = physical_y;
    events[i].buttons = buttons;
    events[i].pressure = pressure;
    events[i].pressure_min = pressure_min;
    events[i].pressure_max = pressure_max;
    events[i].scroll_delta_x = scroll_delta_x;
    events[i].scroll_delta_y = scroll_delta_y;
    events[i].pan_x = pan_x;
    events[i].pan_y = pan_y;
    events[i].scale = scale;
    events[i].rotation = rotation;
    events[i].view_id = view_id;
  }

  DispatchPointerEvents(events.data(), events.size());
}

void AndroidEngine::DispatchPointerEvents(const FlutterPointerEvent* events,
                                          size_t events_count) {
  std::lock_guard engine_lock(engine_mutex_);
  if (is_running_.load() && engine_ != nullptr &&
      embedder_api_.SendPointerEvent != nullptr && events != nullptr &&
      events_count > 0) {
    embedder_api_.SendPointerEvent(engine_, events, events_count);
  }
}

void AndroidEngine::SendPlatformMessage(const char* channel,
                                        const uint8_t* message,
                                        size_t message_size,
                                        int32_t response_id) {
  std::lock_guard engine_lock(engine_mutex_);
  if (!is_running_.load() || engine_ == nullptr ||
      embedder_api_.SendPlatformMessage == nullptr) {
    return;
  }

  FlutterPlatformMessageResponseHandle* response_handle = nullptr;
  if (response_id != 0 &&
      embedder_api_.PlatformMessageCreateResponseHandle != nullptr) {
    auto* ctx = new OutgoingResponseContext{jni_facade_, response_id};
    embedder_api_.PlatformMessageCreateResponseHandle(
        engine_,
        [](const uint8_t* data, size_t size, void* baton) {
          std::unique_ptr<OutgoingResponseContext> context(
              static_cast<OutgoingResponseContext*>(baton));
          if (!context) {
            return;
          }
          if (auto jni = context->jni_facade.lock()) {
            std::unique_ptr<fml::Mapping> mapping;
            if (data != nullptr && size > 0) {
              mapping = std::make_unique<fml::MallocMapping>(
                  fml::MallocMapping::Copy(data, size));
            }
            jni->FlutterViewHandlePlatformMessageResponse(context->response_id,
                                                          std::move(mapping));
          }
        },
        ctx, &response_handle);
  }

  FlutterPlatformMessage flutter_message = {};
  flutter_message.struct_size = sizeof(FlutterPlatformMessage);
  flutter_message.channel = channel;
  flutter_message.message = message;
  flutter_message.message_size = message_size;
  flutter_message.response_handle = response_handle;

  embedder_api_.SendPlatformMessage(engine_, &flutter_message);

  if (response_handle != nullptr &&
      embedder_api_.PlatformMessageReleaseResponseHandle != nullptr) {
    embedder_api_.PlatformMessageReleaseResponseHandle(engine_,
                                                       response_handle);
  }
}

void AndroidEngine::SendPlatformMessageResponse(int32_t response_id,
                                                const uint8_t* data,
                                                size_t data_length) {
  const FlutterPlatformMessageResponseHandle* handle = nullptr;
  {
    std::lock_guard lock(pending_responses_mutex_);
    auto it = pending_incoming_responses_.find(response_id);
    if (it != pending_incoming_responses_.end()) {
      handle = it->second;
      pending_incoming_responses_.erase(it);
    }
  }

  if (handle == nullptr) {
    return;
  }

  std::lock_guard engine_lock(engine_mutex_);
  if (!is_running_.load() || engine_ == nullptr ||
      embedder_api_.SendPlatformMessageResponse == nullptr) {
    return;
  }

  embedder_api_.SendPlatformMessageResponse(engine_, handle, data, data_length);
}

void AndroidEngine::CompletePlatformMessageEmptyResponse(int32_t response_id) {
  SendPlatformMessageResponse(response_id, nullptr, 0);
}

void AndroidEngine::HandlePlatformMessage(
    const FlutterPlatformMessage* message) {
  if (message == nullptr || jni_facade_ == nullptr) {
    return;
  }

  int32_t response_id = 0;
  if (message->response_handle != nullptr) {
    response_id = next_response_id_.fetch_add(1);
    std::lock_guard lock(pending_responses_mutex_);
    pending_incoming_responses_[response_id] = message->response_handle;
  }

  std::string channel_str(message->channel ? message->channel : "");
  jni_facade_->FlutterViewHandlePlatformMessage(
      channel_str, message->message, message->message_size, response_id);
}

void AndroidEngine::HandlePlatformMessageResponse(int32_t response_id,
                                                  const uint8_t* data,
                                                  size_t data_length) {
  if (jni_facade_ != nullptr) {
    std::unique_ptr<fml::Mapping> mapping;
    if (data != nullptr && data_length > 0) {
      mapping = std::make_unique<fml::NonOwnedMapping>(data, data_length);
    }
    jni_facade_->FlutterViewHandlePlatformMessageResponse(response_id,
                                                          std::move(mapping));
  }
}

void AndroidEngine::SetSemanticsEnabled(bool enabled) {
  {
    std::lock_guard engine_lock(engine_mutex_);
    if (is_running_.load() && engine_ != nullptr &&
        embedder_api_.UpdateSemanticsEnabled != nullptr) {
      embedder_api_.UpdateSemanticsEnabled(engine_, enabled);
    }
  }
  if (jni_facade_ != nullptr) {
    jni_facade_->FlutterViewSetSemanticsTreeEnabled(enabled);
  }
}

void AndroidEngine::SetAccessibilityFeatures(int32_t flags) {
  std::lock_guard engine_lock(engine_mutex_);
  if (is_running_.load() && engine_ != nullptr &&
      embedder_api_.UpdateAccessibilityFeatures != nullptr) {
    embedder_api_.UpdateAccessibilityFeatures(
        engine_, static_cast<FlutterAccessibilityFeature>(flags));
  }
}

void AndroidEngine::DispatchSemanticsAction(int32_t id,
                                            int32_t action,
                                            const uint8_t* data,
                                            size_t data_length) {
  std::lock_guard engine_lock(engine_mutex_);
  if (is_running_.load() && engine_ != nullptr &&
      embedder_api_.DispatchSemanticsAction != nullptr) {
    embedder_api_.DispatchSemanticsAction(
        engine_, id, static_cast<FlutterSemanticsAction>(action), data,
        data_length);
  }
}

void AndroidEngine::HandleSemanticsUpdate(
    const FlutterSemanticsUpdate2* update) {
  if (update == nullptr || jni_facade_ == nullptr) {
    return;
  }

  size_t num_bytes = 0;
  for (size_t i = 0; i < update->node_count; ++i) {
    const FlutterSemanticsNode2* node = update->nodes[i];
    if (node == nullptr) {
      continue;
    }
    num_bytes += kBytesPerNode;
    num_bytes += node->child_count * kBytesPerChild;  // traversal order
    num_bytes += node->child_count * kBytesPerChild;  // hit test order
    num_bytes +=
        node->custom_accessibility_actions_count * kBytesPerCustomAction;
    num_bytes += node->label_attribute_count * kBytesPerStringAttribute;
    num_bytes += node->value_attribute_count * kBytesPerStringAttribute;
    num_bytes +=
        node->increased_value_attribute_count * kBytesPerStringAttribute;
    num_bytes +=
        node->decreased_value_attribute_count * kBytesPerStringAttribute;
    num_bytes += node->hint_attribute_count * kBytesPerStringAttribute;
  }

  std::vector<uint8_t> buffer(num_bytes);
  std::vector<std::string> strings;
  std::vector<std::vector<uint8_t>> string_attribute_args;

  if (!buffer.empty()) {
    int32_t* buffer_int32 = reinterpret_cast<int32_t*>(buffer.data());
    float* buffer_float32 = reinterpret_cast<float*>(buffer.data());
    size_t position = 0;

    for (size_t i = 0; i < update->node_count; ++i) {
      const FlutterSemanticsNode2* node = update->nodes[i];
      if (node == nullptr) {
        continue;
      }
      buffer_int32[position++] = node->id;
      int64_t flags =
          ConvertFlagsToInt64(node->flags2, node->flags__deprecated__);
      std::memcpy(&buffer_int32[position], &flags, 8);
      position += 2;
      buffer_int32[position++] = static_cast<int32_t>(node->actions);
      buffer_int32[position++] = node->max_value_length;
      buffer_int32[position++] = node->current_value_length;
      buffer_int32[position++] = node->text_selection_base;
      buffer_int32[position++] = node->text_selection_extent;
      buffer_int32[position++] = static_cast<int32_t>(node->platform_view_id);
      buffer_int32[position++] = node->scroll_child_count;
      buffer_int32[position++] = node->scroll_index;
      buffer_int32[position++] = node->traversal_parent;
      buffer_float32[position++] = static_cast<float>(node->scroll_position);
      buffer_float32[position++] = static_cast<float>(node->scroll_extent_max);
      buffer_float32[position++] = static_cast<float>(node->scroll_extent_min);
      buffer_int32[position++] = static_cast<int32_t>(node->role);

      PutStringIntoBuffer(node->identifier, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->label, buffer_int32, &position, strings);
      PutStringAttributesIntoBuffer(node->label_attributes,
                                    node->label_attribute_count, buffer_int32,
                                    &position, string_attribute_args);

      PutStringIntoBuffer(node->value, buffer_int32, &position, strings);
      PutStringAttributesIntoBuffer(node->value_attributes,
                                    node->value_attribute_count, buffer_int32,
                                    &position, string_attribute_args);

      PutStringIntoBuffer(node->increased_value, buffer_int32, &position,
                          strings);
      PutStringAttributesIntoBuffer(node->increased_value_attributes,
                                    node->increased_value_attribute_count,
                                    buffer_int32, &position,
                                    string_attribute_args);

      PutStringIntoBuffer(node->decreased_value, buffer_int32, &position,
                          strings);
      PutStringAttributesIntoBuffer(node->decreased_value_attributes,
                                    node->decreased_value_attribute_count,
                                    buffer_int32, &position,
                                    string_attribute_args);

      PutStringIntoBuffer(node->hint, buffer_int32, &position, strings);
      PutStringAttributesIntoBuffer(node->hint_attributes,
                                    node->hint_attribute_count, buffer_int32,
                                    &position, string_attribute_args);

      PutStringIntoBuffer(node->tooltip, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->link_url, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->locale, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->min_value, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->max_value, buffer_int32, &position, strings);

      buffer_int32[position++] = node->heading_level;
      buffer_int32[position++] = static_cast<int32_t>(node->text_direction);
      buffer_float32[position++] = static_cast<float>(node->rect.left);
      buffer_float32[position++] = static_cast<float>(node->rect.top);
      buffer_float32[position++] = static_cast<float>(node->rect.right);
      buffer_float32[position++] = static_cast<float>(node->rect.bottom);

      // 16 floats col-major transform
      float transform_floats[16] = {
          static_cast<float>(node->transform.scaleX),
          static_cast<float>(node->transform.skewY),
          static_cast<float>(node->transform.transX),
          static_cast<float>(node->transform.pers0),
          static_cast<float>(node->transform.skewX),
          static_cast<float>(node->transform.scaleY),
          static_cast<float>(node->transform.transY),
          static_cast<float>(node->transform.pers1),
          0.0f,
          0.0f,
          1.0f,
          static_cast<float>(node->transform.pers2),
          0.0f,
          0.0f,
          0.0f,
          1.0f,
      };
      std::memcpy(&buffer_float32[position], transform_floats,
                  sizeof(transform_floats));
      position += 16;

      float hit_test_transform_floats[16] = {
          static_cast<float>(node->hit_test_transform.scaleX),
          static_cast<float>(node->hit_test_transform.skewY),
          static_cast<float>(node->hit_test_transform.transX),
          static_cast<float>(node->hit_test_transform.pers0),
          static_cast<float>(node->hit_test_transform.skewX),
          static_cast<float>(node->hit_test_transform.scaleY),
          static_cast<float>(node->hit_test_transform.transY),
          static_cast<float>(node->hit_test_transform.pers1),
          0.0f,
          0.0f,
          1.0f,
          static_cast<float>(node->hit_test_transform.pers2),
          0.0f,
          0.0f,
          0.0f,
          1.0f,
      };
      std::memcpy(&buffer_float32[position], hit_test_transform_floats,
                  sizeof(hit_test_transform_floats));
      position += 16;

      buffer_int32[position++] = static_cast<int32_t>(node->child_count);
      if (node->children_in_traversal_order != nullptr) {
        for (size_t c = 0; c < node->child_count; ++c) {
          buffer_int32[position++] = node->children_in_traversal_order[c];
        }
      }

      buffer_int32[position++] = static_cast<int32_t>(node->child_count);
      if (node->children_in_hit_test_order != nullptr) {
        for (size_t c = 0; c < node->child_count; ++c) {
          buffer_int32[position++] = node->children_in_hit_test_order[c];
        }
      }

      buffer_int32[position++] =
          static_cast<int32_t>(node->custom_accessibility_actions_count);
      if (node->custom_accessibility_actions != nullptr) {
        for (size_t a = 0; a < node->custom_accessibility_actions_count; ++a) {
          buffer_int32[position++] = node->custom_accessibility_actions[a];
        }
      }
    }

    jni_facade_->FlutterViewUpdateSemantics(std::move(buffer),
                                            std::move(strings),
                                            std::move(string_attribute_args));
  }

  // Custom accessibility actions
  size_t num_action_bytes = update->custom_action_count * kBytesPerAction;
  std::vector<uint8_t> actions_buffer(num_action_bytes);

  if (!actions_buffer.empty()) {
    int32_t* actions_buffer_int32 =
        reinterpret_cast<int32_t*>(actions_buffer.data());
    std::vector<std::string> action_strings;
    size_t actions_position = 0;

    for (size_t i = 0; i < update->custom_action_count; ++i) {
      const FlutterSemanticsCustomAction2* action = update->custom_actions[i];
      if (action == nullptr) {
        continue;
      }
      actions_buffer_int32[actions_position++] = action->id;
      actions_buffer_int32[actions_position++] = action->override_action;
      PutStringIntoBuffer(action->label, actions_buffer_int32,
                          &actions_position, action_strings);
      PutStringIntoBuffer(action->hint, actions_buffer_int32, &actions_position,
                          action_strings);
    }

    jni_facade_->FlutterViewUpdateCustomAccessibilityActions(
        std::move(actions_buffer), std::move(action_strings));
  }
}

bool AndroidEngine::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<fml::Mapping> snapshot_data,
    std::unique_ptr<fml::Mapping> snapshot_instructions) {
  std::lock_guard engine_lock(engine_mutex_);
  if (!is_running_.load() || engine_ == nullptr ||
      embedder_api_.LoadDartDeferredLibrary == nullptr) {
    return false;
  }

  const uint8_t* data_ptr =
      snapshot_data ? snapshot_data->GetMapping() : nullptr;
  size_t data_size = snapshot_data ? snapshot_data->GetSize() : 0;
  const uint8_t* inst_ptr =
      snapshot_instructions ? snapshot_instructions->GetMapping() : nullptr;
  size_t inst_size =
      snapshot_instructions ? snapshot_instructions->GetSize() : 0;

  auto result = embedder_api_.LoadDartDeferredLibrary(
      engine_, loading_unit_id, data_ptr, data_size, inst_ptr, inst_size);
  return result == kSuccess;
}

bool AndroidEngine::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string& error_message,
    bool transient) {
  std::lock_guard engine_lock(engine_mutex_);
  if (!is_running_.load() || engine_ == nullptr ||
      embedder_api_.LoadDartDeferredLibraryError == nullptr) {
    return false;
  }

  auto result = embedder_api_.LoadDartDeferredLibraryError(
      engine_, loading_unit_id, error_message.c_str(), transient);
  return result == kSuccess;
}

void AndroidEngine::HandleRequestDartDeferredLibrary(intptr_t loading_unit_id) {
  if (jni_facade_ != nullptr) {
    jni_facade_->RequestDartDeferredLibrary(loading_unit_id);
  }
}

void AndroidEngine::RegisterExternalTexture(int64_t texture_id) {
  std::lock_guard engine_lock(engine_mutex_);
  if (is_running_.load() && engine_ != nullptr &&
      embedder_api_.RegisterExternalTexture != nullptr) {
    embedder_api_.RegisterExternalTexture(engine_, texture_id);
  }
}

void AndroidEngine::RegisterImageTexture(int64_t texture_id,
                                         JavaLocalRef image_texture_entry) {
  RegisterExternalTexture(texture_id);
}

void AndroidEngine::UnregisterTexture(int64_t texture_id) {
  std::lock_guard engine_lock(engine_mutex_);
  if (is_running_.load() && engine_ != nullptr &&
      embedder_api_.UnregisterExternalTexture != nullptr) {
    embedder_api_.UnregisterExternalTexture(engine_, texture_id);
  }
}

void AndroidEngine::MarkTextureFrameAvailable(int64_t texture_id) {
  std::lock_guard engine_lock(engine_mutex_);
  if (!is_running_.load() || engine_ == nullptr ||
      embedder_api_.MarkExternalTextureFrameAvailable == nullptr) {
    return;
  }
  embedder_api_.MarkExternalTextureFrameAvailable(engine_, texture_id);
}

void AndroidEngine::ScheduleFrame() {
  std::lock_guard engine_lock(engine_mutex_);
  if (is_running_.load() && engine_ != nullptr &&
      embedder_api_.ScheduleFrame != nullptr) {
    embedder_api_.ScheduleFrame(engine_);
  }
}

void AndroidEngine::NotifyLowMemoryWarning() {
  std::lock_guard engine_lock(engine_mutex_);
  if (is_running_.load() && engine_ != nullptr &&
      embedder_api_.NotifyLowMemoryWarning != nullptr) {
    embedder_api_.NotifyLowMemoryWarning(engine_);
  }
}

bool AndroidEngine::Screenshot(FlutterEngineScreenshotType type,
                               bool base64_encode,
                               FlutterEngineScreenshotCallback callback,
                               void* user_data) {
  std::lock_guard engine_lock(engine_mutex_);
  if (!is_running_.load() || engine_ == nullptr ||
      embedder_api_.Screenshot == nullptr || callback == nullptr) {
    return false;
  }
  return embedder_api_.Screenshot(engine_, type, base64_encode, callback,
                                  user_data) == kSuccess;
}

//------------------------------------------------------------------------------
// Static Callbacks
//------------------------------------------------------------------------------

void AndroidEngine::OnPlatformMessageCallback(
    const FlutterPlatformMessage* message,
    void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine != nullptr) {
    engine->HandlePlatformMessage(message);
  }
}

void AndroidEngine::OnRootIsolateCreatedCallback(void* user_data) {
  // Root isolate creation hook. First frame notification is triggered on
  // actual presentation in AndroidCompositor.
}

void AndroidEngine::OnUpdateSemantics2Callback(
    const FlutterSemanticsUpdate2* update,
    void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine != nullptr) {
    engine->HandleSemanticsUpdate(update);
  }
}

void AndroidEngine::OnRequestDartDeferredLibraryCallback(
    intptr_t loading_unit_id,
    void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine != nullptr) {
    engine->HandleRequestDartDeferredLibrary(loading_unit_id);
  }
}

}  // namespace flutter
