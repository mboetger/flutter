// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <pthread.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <memory>
#include <optional>

#include <string>
#include <utility>

#include "common/settings.h"
#include "flutter/fml/cpu_affinity.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/message_loop.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/platform/android/android_display.h"
#include "flutter/shell/platform/android/android_image_generator.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/embedder/platform_view_embedder.h"

namespace flutter {

static PlatformData GetDefaultPlatformData() {
  PlatformData platform_data;
  platform_data.lifecycle_state = "AppLifecycleState.detached";
  return platform_data;
}

namespace {

class AndroidEmbedderSurface : public EmbedderSurface {
 public:
  explicit AndroidEmbedderSurface(PlatformViewAndroid* platform_view_android)
      : platform_view_android_(platform_view_android) {
    FML_DCHECK(platform_view_android_);
  }

  ~AndroidEmbedderSurface() override = default;

  bool IsValid() const override { return true; }

  std::unique_ptr<Surface> CreateGPUSurface() override {
    return platform_view_android_->CreateGPUSurface();
  }

  sk_sp<GrDirectContext> CreateResourceContext() const override {
    return platform_view_android_->CreateResourceContextLegacy();
  }

  void ReleaseResourceContext() const override {
    platform_view_android_->ReleaseResourceContextLegacy();
  }

  std::shared_ptr<impeller::Context> CreateImpellerContext() const override {
    return platform_view_android_->GetImpellerContextLegacy();
  }

 private:
  PlatformViewAndroid* platform_view_android_;
};

static std::unique_ptr<PlatformViewEmbedder> CreatePlatformViewEmbedder(
    Shell& shell,
    PlatformViewAndroid* platform_view_android,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    fml::WeakPtr<PlatformViewAndroid> weak_platform_view) {
  PlatformViewEmbedder::PlatformDispatchTable platform_dispatch_table;
  platform_dispatch_table.update_semantics_callback =
      [weak = weak_platform_view](
          int64_t view_id, flutter::SemanticsNodeUpdates update,
          flutter::CustomAccessibilityActionUpdates actions) {
        if (weak) {
          weak->UpdateSemanticsLegacy(std::move(update), std::move(actions));
        }
      };
  platform_dispatch_table.platform_message_response_callback =
      [weak = weak_platform_view](std::unique_ptr<PlatformMessage> message) {
        if (weak) {
          weak->GetPlatformMessageHandler()->HandlePlatformMessage(
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

  auto embedder_surface =
      std::make_unique<AndroidEmbedderSurface>(platform_view_android);
  return std::make_unique<PlatformViewEmbedder>(
      shell, shell.GetTaskRunners(), std::move(embedder_surface),
      std::move(platform_dispatch_table), nullptr);
}

}  // namespace

AndroidShellHolder::AndroidShellHolder(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(jni_facade),
      android_rendering_api_(android_rendering_api) {
  static size_t thread_host_count = 1;
  auto thread_label = std::to_string(thread_host_count++);

  task_runners_ = AndroidTaskRunners::Create(thread_label, settings);

  if (settings_.android_embedder_api) {
    TRACE_EVENT1("flutter", "AndroidShellHolder::Initialize", "path",
                 "embedder_api");
    InitializeEngine();
    return;
  }

  TRACE_EVENT1("flutter", "AndroidShellHolder::Initialize", "path", "legacy");
  std::unique_ptr<PlatformViewAndroid> platform_view_android;
  PlatformViewEmbedder* raw_platform_view_embedder = nullptr;
  AndroidRenderingAPI rendering_api = android_rendering_api_;
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&jni_facade, &platform_view_android, &raw_platform_view_embedder,
       rendering_api](Shell& shell) {
        platform_view_android = std::make_unique<PlatformViewAndroid>(
            shell.GetSettings(),     // settings
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            rendering_api,           // rendering API
            shell.GetShutdownSafeIOTaskRunner());
        auto platform_view_embedder = CreatePlatformViewEmbedder(
            shell, platform_view_android.get(), jni_facade,
            platform_view_android->GetWeakPtr());
        raw_platform_view_embedder = platform_view_embedder.get();
        platform_view_android->SetPlatformView(raw_platform_view_embedder);
        return platform_view_embedder;
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  shell_ =
      Shell::Create(GetDefaultPlatformData(),         // window data
                    task_runners_->GetTaskRunners(),  // task runners
                    settings_,                        // settings
                    on_create_platform_view,  // platform view create callback
                    on_create_rasterizer      // rasterizer create callback
      );

  if (shell_) {
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
    FML_DLOG(INFO) << "Registered Android SDK image decoder (API level 28+)";
  }

  platform_view_android_ = std::move(platform_view_android);
  platform_view_ = platform_view_android_ ? platform_view_android_->GetWeakPtr()
                                          : fml::WeakPtr<PlatformViewAndroid>();
  platform_view_embedder_ = raw_platform_view_embedder;
  FML_DCHECK(platform_view_);
  is_valid_ = shell_ != nullptr;
  InitializeProjectArgs();
}

FlutterEngineResult AndroidShellHolder::InitializeEngine() {
  if (shell_) {
    return kSuccess;
  }

  std::unique_ptr<PlatformViewAndroid> platform_view_android;
  PlatformViewEmbedder* raw_platform_view_embedder = nullptr;
  AndroidRenderingAPI rendering_api = android_rendering_api_;
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [this, &platform_view_android, &raw_platform_view_embedder,
       rendering_api](Shell& shell) {
        platform_view_android = std::make_unique<PlatformViewAndroid>(
            shell.GetSettings(),     // settings
            shell.GetTaskRunners(),  // task runners
            jni_facade_,             // JNI interop
            rendering_api,           // rendering API
            shell.GetShutdownSafeIOTaskRunner());
        auto platform_view_embedder = CreatePlatformViewEmbedder(
            shell, platform_view_android.get(), jni_facade_,
            platform_view_android->GetWeakPtr());
        raw_platform_view_embedder = platform_view_embedder.get();
        platform_view_android->SetPlatformView(raw_platform_view_embedder);
        return platform_view_embedder;
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  shell_ =
      Shell::Create(GetDefaultPlatformData(),         // window data
                    task_runners_->GetTaskRunners(),  // task runners
                    settings_,                        // settings
                    on_create_platform_view,  // platform view create callback
                    on_create_rasterizer      // rasterizer create callback
      );

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

  platform_view_android_ = std::move(platform_view_android);
  platform_view_ = platform_view_android_ ? platform_view_android_->GetWeakPtr()
                                          : fml::WeakPtr<PlatformViewAndroid>();
  platform_view_embedder_ = raw_platform_view_embedder;
  FML_DCHECK(platform_view_);
  is_valid_ = true;
  InitializeProjectArgs();
  return kSuccess;
}

AndroidShellHolder::AndroidShellHolder(
    const Settings& settings,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<AndroidTaskRunners>& task_runners,
    std::unique_ptr<Shell> shell,
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    std::unique_ptr<PlatformViewAndroid> platform_view_android,
    AndroidRenderingAPI rendering_api,
    PlatformViewEmbedder* platform_view_embedder)
    : settings_(settings),
      jni_facade_(jni_facade),
      platform_view_android_(std::move(platform_view_android)),
      platform_view_(platform_view_android_
                         ? platform_view_android_->GetWeakPtr()
                         : fml::WeakPtr<PlatformViewAndroid>()),
      task_runners_(task_runners),
      shell_(std::move(shell)),
      apk_asset_provider_(std::move(apk_asset_provider)),
      android_rendering_api_(rendering_api),
      platform_view_embedder_(platform_view_embedder) {
  FML_DCHECK(jni_facade);
  FML_DCHECK(shell_);
  FML_DCHECK(shell_->IsSetup());
  FML_DCHECK(platform_view_);
  FML_DCHECK(task_runners_);
  is_valid_ = shell_ != nullptr;
  InitializeProjectArgs();
}

AndroidShellHolder::~AndroidShellHolder() {
  shell_.reset();
  platform_view_android_.reset();
  task_runners_.reset();
}

bool AndroidShellHolder::IsValid() const {
  return is_valid_;
}

const flutter::Settings& AndroidShellHolder::GetSettings() const {
  return settings_;
}

std::unique_ptr<AndroidShellHolder> AndroidShellHolder::Spawn(
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::string& initial_route,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) const {
  if (settings_.android_embedder_api) {
    TRACE_EVENT1("flutter", "AndroidShellHolder::Spawn", "path",
                 "embedder_api");
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
    spawn_config.library_path =
        libraryUrl.empty() ? nullptr : libraryUrl.c_str();
    spawn_config.initial_route =
        initial_route.empty() ? nullptr : initial_route.c_str();
    spawn_config.argc = static_cast<int64_t>(argv.size());
    spawn_config.argv = argv.empty() ? nullptr : argv.data();
    spawn_config.custom_args = &custom_project_args;

    std::unique_ptr<AndroidShellHolder> spawned_holder;
    FlutterEngineResult result =
        SpawnEngine(&spawn_config, jni_facade, &spawned_holder);
    if (result != kSuccess) {
      return nullptr;
    }
    return spawned_holder;
  }

  TRACE_EVENT1("flutter", "AndroidShellHolder::Spawn", "path", "legacy");
  FML_DCHECK(shell_ && shell_->IsSetup())
      << "A new Shell can only be spawned "
         "if the current Shell is properly constructed";

  // Pull out the new PlatformViewAndroid from the new Shell to feed to it to
  // the new AndroidShellHolder.
  //
  // It's a weak pointer because it's owned by the Shell (which we're also)
  // making below. And the AndroidShellHolder then owns the Shell.
  std::unique_ptr<PlatformViewAndroid> spawned_platform_view_android;
  PlatformViewEmbedder* raw_spawned_platform_view_embedder = nullptr;

  // Take out the old AndroidContext to reuse inside the PlatformViewAndroid
  // of the new Shell.
  PlatformViewAndroid* android_platform_view = platform_view_.get();
  // There's some indirection with platform_view_ being a weak pointer but
  // we just checked that the shell_ exists above and a valid shell is the
  // owner of the platform view so this weak pointer always exists.
  FML_DCHECK(android_platform_view);
  std::shared_ptr<flutter::AndroidContext> android_context =
      android_platform_view->GetAndroidContext();
  FML_DCHECK(android_context);

  // This is a synchronous call, so the captures don't have race checks.
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&jni_facade, android_context, &spawned_platform_view_android,
       &raw_spawned_platform_view_embedder](Shell& shell) {
        spawned_platform_view_android = std::make_unique<PlatformViewAndroid>(
            shell.GetSettings(),     // settings
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            android_context          // Android context
        );
        auto platform_view_embedder = CreatePlatformViewEmbedder(
            shell, spawned_platform_view_android.get(), jni_facade,
            spawned_platform_view_android->GetWeakPtr());
        raw_spawned_platform_view_embedder = platform_view_embedder.get();
        spawned_platform_view_android->SetPlatformView(
            raw_spawned_platform_view_embedder);
        return platform_view_embedder;
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  auto config = BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  if (!config) {
    // If the RunConfiguration was null, the kernel blob wasn't readable.
    // Fail the whole thing.
    return nullptr;
  }
  config->SetEngineId(engine_id);

  std::unique_ptr<flutter::Shell> shell =
      shell_->Spawn(std::move(config.value()), initial_route,
                    on_create_platform_view, on_create_rasterizer);

  auto spawned_holder =
      std::unique_ptr<AndroidShellHolder>(new AndroidShellHolder(
          GetSettings(), jni_facade, task_runners_, std::move(shell),
          apk_asset_provider_->Clone(),
          std::move(spawned_platform_view_android),
          android_context->RenderingApi(), raw_spawned_platform_view_embedder));
  if (spawned_holder) {
    spawned_holder->project_args_.engine_id = engine_id;
  }
  return spawned_holder;
}

void AndroidShellHolder::Launch(
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
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

  if (settings_.android_embedder_api) {
    TRACE_EVENT1("flutter", "AndroidShellHolder::Launch", "path",
                 "embedder_api");
    RunEngine(entrypoint, libraryUrl, entrypoint_args, engine_id);
    return;
  }

  TRACE_EVENT1("flutter", "AndroidShellHolder::Launch", "path", "legacy");
  auto config = BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  if (!config) {
    return;
  }
  config->SetEngineId(engine_id);
  UpdateDisplayMetrics();
  shell_->RunEngine(std::move(config.value()));
}

FlutterEngineResult AndroidShellHolder::RunEngine(
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

FlutterEngineResult AndroidShellHolder::SpawnEngine(
    const FlutterEngineSpawnConfig* config,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    std::unique_ptr<AndroidShellHolder>* spawned_holder_out) const {
  if (spawned_holder_out == nullptr) {
    return kInvalidArguments;
  }
  *spawned_holder_out = nullptr;

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

  std::unique_ptr<PlatformViewAndroid> spawned_platform_view_android;
  PlatformViewEmbedder* raw_spawned_platform_view_embedder = nullptr;

  PlatformViewAndroid* android_platform_view = platform_view_.get();
  if (!android_platform_view) {
    return kInternalInconsistency;
  }
  std::shared_ptr<flutter::AndroidContext> android_context =
      android_platform_view->GetAndroidContext();
  if (!android_context) {
    return kInternalInconsistency;
  }

  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&jni_facade, android_context, &spawned_platform_view_android,
       &raw_spawned_platform_view_embedder](Shell& shell) {
        spawned_platform_view_android = std::make_unique<PlatformViewAndroid>(
            shell.GetSettings(),     // settings
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            android_context          // Android context
        );
        auto platform_view_embedder = CreatePlatformViewEmbedder(
            shell, spawned_platform_view_android.get(), jni_facade,
            spawned_platform_view_android->GetWeakPtr());
        raw_spawned_platform_view_embedder = platform_view_embedder.get();
        spawned_platform_view_android->SetPlatformView(
            raw_spawned_platform_view_embedder);
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

  auto spawned_holder =
      std::unique_ptr<AndroidShellHolder>(new AndroidShellHolder(
          GetSettings(), jni_facade, task_runners_, std::move(shell),
          apk_asset_provider_->Clone(),
          std::move(spawned_platform_view_android),
          android_context->RenderingApi(), raw_spawned_platform_view_embedder));
  if (!spawned_holder) {
    return kInternalInconsistency;
  }
  spawned_holder->project_args_.engine_id = engine_id;
  *spawned_holder_out = std::move(spawned_holder);
  return kSuccess;
}

Rasterizer::Screenshot AndroidShellHolder::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  if (!IsValid() || !platform_view_) {
    return {nullptr, DlISize(), "", Rasterizer::ScreenshotFormat::kUnknown};
  }
  auto surface_screenshot = platform_view_->Screenshot();
  Rasterizer::Screenshot screenshot;
  screenshot.data = surface_screenshot.data;
  screenshot.frame_size = surface_screenshot.frame_size;
  screenshot.format = "ScreenshotType::UncompressedImage";
  return screenshot;
}

fml::WeakPtr<PlatformViewAndroid> AndroidShellHolder::GetPlatformView() {
  FML_DCHECK(platform_view_);
  return platform_view_;
}

void AndroidShellHolder::NotifyLowMemoryWarning() {
  FML_DCHECK(shell_);
  shell_->NotifyLowMemoryWarning();
}

std::optional<RunConfiguration> AndroidShellHolder::BuildRunConfiguration(
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
  config.AddAssetResolver(apk_asset_provider_->Clone());

  {
    if (!entrypoint.empty() && !libraryUrl.empty()) {
      config.SetEntrypointAndLibrary(entrypoint, libraryUrl);
    } else if (!entrypoint.empty()) {
      config.SetEntrypoint(entrypoint);
    }
    if (!entrypoint_args.empty()) {
      config.SetEntrypointArgs(entrypoint_args);
    }
  }
  return config;
}

void AndroidShellHolder::UpdateDisplayMetrics() {
  std::vector<std::unique_ptr<Display>> displays;
  displays.push_back(std::make_unique<AndroidDisplay>(jni_facade_));
  shell_->OnDisplayUpdates(std::move(displays));
}

bool AndroidShellHolder::IsSurfaceControlEnabled() {
  return GetPlatformView()->IsSurfaceControlEnabled();
}

bool AndroidShellHolder::IsAndroidEmbedderApiEnabled() const {
  return settings_.android_embedder_api;
}

void AndroidShellHolder::InitializeProjectArgs() {
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

void AndroidShellHolder::OnPreEngineRestart(void* user_data) {
  auto* holder = static_cast<AndroidShellHolder*>(user_data);
  if (holder && holder->jni_facade_) {
    holder->jni_facade_->FlutterViewOnPreEngineRestart();
  }
}

void AndroidShellHolder::OnSetApplicationLocale(const char* locale,
                                                void* user_data) {
  auto* holder = static_cast<AndroidShellHolder*>(user_data);
  if (holder && holder->jni_facade_ && locale) {
    holder->jni_facade_->FlutterViewSetApplicationLocale(locale);
  }
}

double AndroidShellHolder::OnGetScaledFontSize(double unscaled_font_size,
                                               int configuration_id,
                                               void* user_data) {
  auto* holder = static_cast<AndroidShellHolder*>(user_data);
  if (holder && holder->jni_facade_) {
    return holder->jni_facade_->FlutterViewGetScaledFontSize(unscaled_font_size,
                                                             configuration_id);
  }
  return unscaled_font_size;
}

void AndroidShellHolder::OnRequestDartDeferredLibrary(intptr_t loading_unit_id,
                                                      void* user_data) {
  auto* holder = static_cast<AndroidShellHolder*>(user_data);
  if (holder && holder->jni_facade_) {
    holder->jni_facade_->RequestDartDeferredLibrary(loading_unit_id);
  }
}

FlutterProjectArgs AndroidShellHolder::CreateFlutterProjectArgs(
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

}  // namespace flutter
