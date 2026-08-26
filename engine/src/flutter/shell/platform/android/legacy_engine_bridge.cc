// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/platform/android/legacy_engine_bridge.h"

#include <pthread.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "flutter/common/settings.h"
#include "flutter/fml/cpu_affinity.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/message_loop.h"
#include "flutter/lib/ui/painting/image_generator_registry.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/android_display.h"
#include "flutter/shell/platform/android/android_image_generator.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/embedder_surface_android.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/embedder/platform_view_embedder.h"

namespace flutter {

// Android platform thread priority constants:
// Priority 10 represents nice priority for low-priority background work.
constexpr int kBackgroundThreadNicePriority = 10;
// Priority -1 gives slight priority boost to UI display thread.
constexpr int kDisplayThreadNicePriority = -1;
// Priority -5 gives display-level compositor priority to raster thread.
constexpr int kRasterThreadPrimaryNicePriority = -5;
// Priority -2 is a conservative fallback if -5 is disallowed by the OEM.
constexpr int kRasterThreadFallbackNicePriority = -2;
// Priority 0 represents standard normal thread nice priority.
constexpr int kNormalThreadNicePriority = 0;
// Priority 1 gives workers slightly lower priority than interactive UI.
constexpr int kWorkerThreadNicePriority = 1;

/// Inheriting ThreadConfigurer and use Android platform thread API to configure
/// the thread priorities
static void AndroidPlatformThreadConfigSetter(
    const fml::Thread::ThreadConfig& config) {
  // set thread name
  fml::Thread::SetCurrentThreadName(config);
  // set thread priority
  switch (config.priority) {
    case fml::Thread::ThreadPriority::kBackground: {
      fml::RequestAffinity(fml::CpuAffinity::kEfficiency);
      if (::setpriority(PRIO_PROCESS, 0, kBackgroundThreadNicePriority) != 0) {
        FML_LOG(ERROR) << "Failed to set IO task runner priority";
      }
      break;
    }
    case fml::Thread::ThreadPriority::kDisplay: {
      fml::RequestAffinity(fml::CpuAffinity::kNotEfficiency);
      if (::setpriority(PRIO_PROCESS, 0, kDisplayThreadNicePriority) != 0) {
        FML_LOG(ERROR) << "Failed to set UI task runner priority";
      }
      break;
    }
    case fml::Thread::ThreadPriority::kRaster: {
      fml::RequestAffinity(fml::CpuAffinity::kNotEfficiency);
      // Android describes -8 as "most important display threads, for
      // compositing the screen and retrieving input events". Conservatively
      // set the raster thread to slightly lower priority than it.
      if (::setpriority(PRIO_PROCESS, 0, kRasterThreadPrimaryNicePriority) !=
          0) {
        // Defensive fallback. Depending on the OEM, it may not be possible
        // to set priority to -5.
        if (::setpriority(PRIO_PROCESS, 0, kRasterThreadFallbackNicePriority) !=
            0) {
          FML_LOG(ERROR) << "Failed to set raster task runner priority";
        }
      }
      break;
    }
    default:
      fml::RequestAffinity(fml::CpuAffinity::kNotPerformance);
      if (::setpriority(PRIO_PROCESS, 0, kNormalThreadNicePriority) != 0) {
        FML_LOG(ERROR) << "Failed to set priority";
      }
  }
}

static PlatformData GetDefaultPlatformData() {
  PlatformData platform_data;
  platform_data.lifecycle_state = "AppLifecycleState.detached";
  return platform_data;
}

static PlatformViewEmbedder::PlatformDispatchTable CreateDispatchTable(
    fml::WeakPtr<PlatformViewAndroid> platform_view) {
  PlatformViewEmbedder::PlatformDispatchTable dispatch_table;
  dispatch_table.update_semantics_callback =
      [platform_view](int64_t view_id, flutter::SemanticsNodeUpdates update,
                      flutter::CustomAccessibilityActionUpdates actions) {
        if (platform_view) {
          platform_view->UpdateSemantics(view_id, std::move(update),
                                         std::move(actions));
        }
      };
  dispatch_table.platform_message_response_callback =
      [platform_view](std::unique_ptr<flutter::PlatformMessage> message) {
        if (platform_view) {
          platform_view->HandlePlatformMessage(std::move(message));
        }
      };
  dispatch_table.platform_message_response_completion_callback =
      [platform_view](int response_id, std::unique_ptr<fml::Mapping> data) {
        if (platform_view) {
          platform_view->GetJniFacade()
              ->FlutterViewHandlePlatformMessageResponse(response_id,
                                                         std::move(data));
        }
      };
  dispatch_table.platform_message_empty_response_completion_callback =
      [platform_view](int response_id) {
        if (platform_view) {
          platform_view->GetJniFacade()
              ->FlutterViewHandlePlatformMessageResponse(response_id, nullptr);
        }
      };
  dispatch_table.vsync_callback = [platform_view](intptr_t baton) {
    if (platform_view) {
      platform_view->OnVsyncCallback(baton);
    }
  };
  dispatch_table.on_pre_engine_restart_callback = [platform_view]() {
    if (platform_view) {
      platform_view->OnPreEngineRestart();
    }
  };
  dispatch_table.compute_platform_resolved_locale_callback =
      [platform_view](const std::vector<std::string>& supported_locale_data) {
        if (platform_view) {
          return platform_view->ComputePlatformResolvedLocales(
              supported_locale_data);
        }
        return std::make_unique<std::vector<std::string>>();
      };
  dispatch_table.request_dart_deferred_library_callback =
      [platform_view](intptr_t loading_unit_id) {
        if (platform_view) {
          platform_view->RequestDartDeferredLibrary(loading_unit_id);
        }
      };
  dispatch_table.on_channel_update = [platform_view](const std::string& name,
                                                     bool listening) {
    if (platform_view) {
      platform_view->SendChannelUpdate(name, listening);
    }
  };
  dispatch_table.view_focus_change_request_callback =
      [platform_view](const ViewFocusChangeRequest& request) {
        if (platform_view) {
          platform_view->RequestViewFocusChange(request);
        }
      };
  return dispatch_table;
}

LegacyEngineBridge::LegacyEngineBridge(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(jni_facade),
      android_rendering_api_(android_rendering_api) {
  static size_t thread_host_count = 1;
  auto thread_label = std::to_string(thread_host_count++);

  auto mask = ThreadHost::Type::kRaster | ThreadHost::Type::kIo;
  if (settings.merged_platform_ui_thread !=
      Settings::MergedPlatformUIThread::kEnabled) {
    mask |= ThreadHost::Type::kUi;
  }

  flutter::ThreadHost::ThreadHostConfig host_config(
      thread_label, mask, AndroidPlatformThreadConfigSetter);
  host_config.ui_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kUi, thread_label),
      fml::Thread::ThreadPriority::kDisplay);
  host_config.raster_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kRaster, thread_label),
      fml::Thread::ThreadPriority::kRaster);
  host_config.io_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kIo, thread_label),
      fml::Thread::ThreadPriority::kNormal);

  thread_host_ = std::make_shared<ThreadHost>(host_config);

  fml::WeakPtr<PlatformViewAndroid> weak_platform_view;
  AndroidRenderingAPI rendering_api = android_rendering_api_;
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [jni_facade, &weak_platform_view, rendering_api, this](Shell& shell) {
        auto context_settings =
            PlatformViewAndroid::CreateContextSettings(shell.GetSettings());
        auto android_context = PlatformViewAndroid::CreateAndroidContext(
            shell.GetTaskRunners(), rendering_api,
            shell.GetSettings().enable_opengl_gpu_tracing, context_settings,
            shell.GetShutdownSafeIOTaskRunner());

        auto embedder_surface =
            std::make_unique<EmbedderSurfaceAndroid>(android_context, shell);
        embedder_surface_ = embedder_surface.get();

        platform_view_android_ = std::make_unique<PlatformViewAndroid>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            android_context,         // Android context
            embedder_surface.get()   // Embedder surface
        );
        weak_platform_view = platform_view_android_->GetWeakPtr();
        platform_view_ = weak_platform_view;

        auto dispatch_table = CreateDispatchTable(weak_platform_view);

        auto platform_view_embedder = std::make_unique<PlatformViewEmbedder>(
            shell, shell.GetTaskRunners(), std::move(embedder_surface),
            dispatch_table, nullptr);
        platform_view_android_->SetPlatformView(
            platform_view_embedder->GetWeakPtr());

        return platform_view_embedder;
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  // The current thread will be used as the platform thread. Ensure that the
  // message loop is initialized.
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  fml::RefPtr<fml::TaskRunner> raster_runner;
  fml::RefPtr<fml::TaskRunner> ui_runner;
  fml::RefPtr<fml::TaskRunner> io_runner;
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();
  raster_runner = thread_host_->raster_thread->GetTaskRunner();
  if (settings.merged_platform_ui_thread ==
      Settings::MergedPlatformUIThread::kEnabled) {
    ui_runner = platform_runner;
  } else {
    ui_runner = thread_host_->ui_thread->GetTaskRunner();
  }
  io_runner = thread_host_->io_thread->GetTaskRunner();

  flutter::TaskRunners task_runners(thread_label,     // label
                                    platform_runner,  // platform
                                    raster_runner,    // raster
                                    ui_runner,        // ui
                                    io_runner         // io
  );

  shell_ =
      Shell::Create(GetDefaultPlatformData(),  // window data
                    task_runners,              // task runners
                    settings_,                 // settings
                    on_create_platform_view,   // platform view create callback
                    on_create_rasterizer       // rasterizer create callback
      );

  if (shell_) {
    shell_->GetDartVM()->GetConcurrentMessageLoop()->PostTaskToAllWorkers([]() {
      if (::setpriority(PRIO_PROCESS, gettid(), kWorkerThreadNicePriority) !=
          0) {
        FML_LOG(ERROR) << "Failed to set Workers task runner priority";
      }
    });

    shell_->RegisterImageDecoder(
        [runner = task_runners.GetIOTaskRunner()](sk_sp<SkData> buffer) {
          return AndroidImageGenerator::MakeFromData(std::move(buffer), runner);
        },
        -1);
    FML_DLOG(INFO) << "Registered Android SDK image decoder (API level 28+)";
  }

  platform_view_ = weak_platform_view;
  FML_DCHECK(platform_view_);
  is_valid_ = shell_ != nullptr;
}

LegacyEngineBridge::LegacyEngineBridge(
    const Settings& settings,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<ThreadHost>& thread_host,
    std::unique_ptr<Shell> shell,
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    std::unique_ptr<PlatformViewAndroid> platform_view_android,
    EmbedderSurfaceAndroid* embedder_surface,
    AndroidRenderingAPI rendering_api)
    : settings_(settings),
      jni_facade_(jni_facade),
      platform_view_android_(std::move(platform_view_android)),
      platform_view_(platform_view_android_
                         ? platform_view_android_->GetWeakPtr()
                         : fml::WeakPtr<PlatformViewAndroid>()),
      embedder_surface_(embedder_surface),
      thread_host_(thread_host),
      shell_(std::move(shell)),
      apk_asset_provider_(std::move(apk_asset_provider)),
      android_rendering_api_(rendering_api) {
  FML_DCHECK(jni_facade);
  FML_DCHECK(shell_);
  FML_DCHECK(shell_->IsSetup());
  FML_DCHECK(platform_view_);
  FML_DCHECK(thread_host_);
  is_valid_ = shell_ != nullptr;
}

LegacyEngineBridge::~LegacyEngineBridge() {
  shell_.reset();
  thread_host_.reset();
}

bool LegacyEngineBridge::IsValid() const {
  return is_valid_;
}

const flutter::Settings& LegacyEngineBridge::GetSettings() const {
  return settings_;
}

std::unique_ptr<AndroidEngineBridge> LegacyEngineBridge::Spawn(
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::string& initial_route,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) const {
  FML_DCHECK(shell_ && shell_->IsSetup())
      << "A new Shell can only be spawned "
         "if the current Shell is properly constructed";

  // Pull out the new PlatformViewAndroid from the new Shell to feed it to
  // the new LegacyEngineBridge.
  std::unique_ptr<PlatformViewAndroid> platform_view_android_out;
  EmbedderSurfaceAndroid* embedder_surface_out = nullptr;
  fml::WeakPtr<PlatformViewAndroid> weak_platform_view;

  // Take out the old AndroidContext to reuse inside the PlatformViewAndroid
  // of the new Shell.
  PlatformViewAndroid* android_platform_view = platform_view_android_.get();
  FML_DCHECK(android_platform_view);
  std::shared_ptr<flutter::AndroidContext> android_context =
      android_platform_view->GetAndroidContext();
  FML_DCHECK(android_context);

  // This is a synchronous call, so the captures don't have race checks.
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [jni_facade, android_context, &platform_view_android_out,
       &embedder_surface_out, &weak_platform_view](Shell& shell) {
        auto embedder_surface =
            std::make_unique<EmbedderSurfaceAndroid>(android_context, shell);
        embedder_surface_out = embedder_surface.get();

        platform_view_android_out = std::make_unique<PlatformViewAndroid>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            android_context,         // Android context
            embedder_surface.get()   // Embedder surface
        );
        weak_platform_view = platform_view_android_out->GetWeakPtr();

        auto dispatch_table = CreateDispatchTable(weak_platform_view);

        auto platform_view_embedder = std::make_unique<PlatformViewEmbedder>(
            shell, shell.GetTaskRunners(), std::move(embedder_surface),
            dispatch_table, nullptr);
        platform_view_android_out->SetPlatformView(
            platform_view_embedder->GetWeakPtr());

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

  return std::make_unique<LegacyEngineBridge>(
      GetSettings(), jni_facade, thread_host_, std::move(shell),
      apk_asset_provider_->Clone(), std::move(platform_view_android_out),
      embedder_surface_out, android_context->RenderingApi());
}

void LegacyEngineBridge::Launch(
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) {
  if (!IsValid()) {
    return;
  }

  apk_asset_provider_ = std::move(apk_asset_provider);
  auto config = BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  if (!config) {
    return;
  }
  config->SetEngineId(engine_id);
  UpdateDisplayMetrics();
  shell_->RunEngine(std::move(config.value()));
}

Rasterizer::Screenshot LegacyEngineBridge::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  if (!IsValid()) {
    return {nullptr, DlISize(), "", Rasterizer::ScreenshotFormat::kUnknown};
  }
  return shell_->Screenshot(type, base64_encode);
}

fml::WeakPtr<PlatformViewAndroid> LegacyEngineBridge::GetPlatformView() {
  FML_DCHECK(platform_view_);
  return platform_view_;
}

PlatformViewAndroid* LegacyEngineBridge::GetPlatformViewAndroid() {
  return platform_view_android_.get();
}

EmbedderSurfaceAndroid* LegacyEngineBridge::GetEmbedderSurfaceAndroid() {
  return embedder_surface_;
}

void LegacyEngineBridge::NotifyLowMemoryWarning() {
  FML_DCHECK(shell_);
  shell_->NotifyLowMemoryWarning();
}

const std::shared_ptr<PlatformMessageHandler>&
LegacyEngineBridge::GetPlatformMessageHandler() const {
  if (platform_view_android_) {
    return platform_view_android_->GetPlatformMessageHandler();
  }
  return shell_->GetPlatformMessageHandler();
}

std::optional<RunConfiguration> LegacyEngineBridge::BuildRunConfiguration(
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

void LegacyEngineBridge::UpdateDisplayMetrics() {
  std::vector<std::unique_ptr<Display>> displays;
  displays.push_back(std::make_unique<AndroidDisplay>(jni_facade_));
  shell_->OnDisplayUpdates(std::move(displays));
}

bool LegacyEngineBridge::IsSurfaceControlEnabled() {
  if (platform_view_android_) {
    return platform_view_android_->IsSurfaceControlEnabled();
  }
  return false;
}

}  // namespace flutter
