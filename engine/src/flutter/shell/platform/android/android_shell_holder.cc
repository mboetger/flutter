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
#include "flutter/lib/ui/painting/image_generator_registry.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/android_display.h"
#include "flutter/shell/platform/android/android_image_generator.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/embedder/embedder_engine.h"
#include "flutter/shell/platform/embedder/embedder_thread_host.h"
#include "flutter/shell/platform/embedder/platform_view_embedder.h"

namespace flutter {

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
      if (::setpriority(PRIO_PROCESS, 0, 10) != 0) {
        FML_LOG(ERROR) << "Failed to set IO task runner priority";
      }
      break;
    }
    case fml::Thread::ThreadPriority::kDisplay: {
      fml::RequestAffinity(fml::CpuAffinity::kNotEfficiency);
      if (::setpriority(PRIO_PROCESS, 0, -1) != 0) {
        FML_LOG(ERROR) << "Failed to set UI task runner priority";
      }
      break;
    }
    case fml::Thread::ThreadPriority::kRaster: {
      fml::RequestAffinity(fml::CpuAffinity::kNotEfficiency);
      // Android describes -8 as "most important display threads, for
      // compositing the screen and retrieving input events". Conservatively
      // set the raster thread to slightly lower priority than it.
      if (::setpriority(PRIO_PROCESS, 0, -5) != 0) {
        // Defensive fallback. Depending on the OEM, it may not be possible
        // to set priority to -5.
        if (::setpriority(PRIO_PROCESS, 0, -2) != 0) {
          FML_LOG(ERROR) << "Failed to set raster task runner priority";
        }
      }
      break;
    }
    default:
      fml::RequestAffinity(fml::CpuAffinity::kNotPerformance);
      if (::setpriority(PRIO_PROCESS, 0, 0) != 0) {
        FML_LOG(ERROR) << "Failed to set priority";
      }
  }
}

AndroidShellHolder::AndroidShellHolder(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI android_rendering_api,
    std::unique_ptr<APKAssetProvider> apk_asset_provider)
    : settings_(settings),
      jni_facade_(jni_facade),
      apk_asset_provider_(std::move(apk_asset_provider)),
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

  ThreadHost thread_host(host_config);

  std::unique_ptr<PlatformViewAndroid> platform_view_android;
  AndroidRenderingAPI rendering_api = android_rendering_api_;
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&jni_facade, &platform_view_android, rendering_api](Shell& shell) {
        platform_view_android = std::make_unique<PlatformViewAndroid>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            rendering_api            // rendering API
        );

        // Setup Impeller context on the raster thread, as expected by
        // Shell::Create
        fml::AutoResetWaitableEvent latch;
        fml::TaskRunner::RunNowOrPostTask(
            shell.GetTaskRunners().GetRasterTaskRunner(),
            [&latch, platform_view = platform_view_android.get()]() {
              platform_view->SetupImpellerContext();
              latch.Signal();
            });
        latch.Wait();

        auto embedder_surface = platform_view_android->TakeSurface();
        FML_LOG(ERROR) << "AndroidShellHolder: embedder_surface is "
                       << (embedder_surface ? "not null" : "null");

        PlatformViewEmbedder::PlatformDispatchTable dispatch_table;

        dispatch_table.update_semantics_callback =
            [platform_view = platform_view_android.get()](
                int64_t view_id, flutter::SemanticsNodeUpdates update,
                flutter::CustomAccessibilityActionUpdates actions) {
              platform_view->UpdateSemantics(view_id, std::move(update),
                                             std::move(actions));
            };

        dispatch_table.platform_message_response_callback =
            [platform_view = platform_view_android.get()](
                std::unique_ptr<PlatformMessage> message) {
              platform_view->HandlePlatformMessage(std::move(message));
            };

        dispatch_table.on_pre_engine_restart_callback =
            [platform_view = platform_view_android.get()]() {
              platform_view->OnPreEngineRestart();
            };

        return std::make_unique<PlatformViewEmbedder>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            std::move(embedder_surface), dispatch_table,
            nullptr  // external view embedder
        );
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
  raster_runner = thread_host.raster_thread->GetTaskRunner();
  if (settings.merged_platform_ui_thread ==
      Settings::MergedPlatformUIThread::kEnabled) {
    ui_runner = platform_runner;
  } else {
    ui_runner = thread_host.ui_thread->GetTaskRunner();
  }
  io_runner = thread_host.io_thread->GetTaskRunner();

  flutter::TaskRunners task_runners(thread_label,     // label
                                    platform_runner,  // platform
                                    raster_runner,    // raster
                                    ui_runner,        // ui
                                    io_runner         // io
  );

  thread_host_ = std::make_shared<EmbedderThreadHost>(
      std::move(thread_host), task_runners,
      std::set<fml::RefPtr<EmbedderTaskRunner>>{});

  RunConfiguration run_configuration(
      IsolateConfiguration::CreateForAppSnapshot());
  if (apk_asset_provider_) {
    run_configuration.AddAssetResolver(apk_asset_provider_->Clone());
  }

  embedder_engine_ = std::make_unique<EmbedderEngine>(
      thread_host_, task_runners, settings_, std::move(run_configuration),
      on_create_platform_view, on_create_rasterizer,
      nullptr  // external_texture_resolver
  );

  is_valid_ = embedder_engine_->LaunchShell();

  if (is_valid_) {
    embedder_engine_->GetShell()
        .GetDartVM()
        ->GetConcurrentMessageLoop()
        ->PostTaskToAllWorkers([]() {
          if (::setpriority(PRIO_PROCESS, gettid(), 1) != 0) {
            FML_LOG(ERROR) << "Failed to set Workers task runner priority";
          }
        });

    embedder_engine_->GetShell().RegisterImageDecoder(
        [runner = task_runners.GetIOTaskRunner()](sk_sp<SkData> buffer) {
          return AndroidImageGenerator::MakeFromData(std::move(buffer), runner);
        },
        -1);
    FML_DLOG(INFO) << "Registered Android SDK image decoder (API level 28+)";
  }

  platform_view_ = std::move(platform_view_android);
  FML_DCHECK(platform_view_);
}

AndroidShellHolder::AndroidShellHolder(
    const Settings& settings,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<EmbedderThreadHost>& thread_host,
    std::unique_ptr<EmbedderEngine> embedder_engine,
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    std::unique_ptr<PlatformViewAndroid> platform_view,
    AndroidRenderingAPI rendering_api)
    : settings_(settings),
      jni_facade_(jni_facade),
      platform_view_(std::move(platform_view)),
      thread_host_(thread_host),
      embedder_engine_(std::move(embedder_engine)),
      apk_asset_provider_(std::move(apk_asset_provider)),
      android_rendering_api_(rendering_api) {
  FML_DCHECK(jni_facade);
  FML_DCHECK(embedder_engine_);
  FML_DCHECK(embedder_engine_->GetShell().IsSetup());
  FML_DCHECK(platform_view_);
  FML_DCHECK(thread_host_);
  is_valid_ = embedder_engine_ != nullptr;
}

AndroidShellHolder::~AndroidShellHolder() {
  embedder_engine_.reset();
  thread_host_.reset();
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
  FML_DCHECK(embedder_engine_ && embedder_engine_->GetShell().IsSetup())
      << "A new Shell can only be spawned "
         "if the current Shell is properly constructed";

  // Pull out the new PlatformViewAndroid from the new Shell to feed to it to
  // the new AndroidShellHolder.
  //
  // It's a weak pointer because it's owned by the Shell (which we're also)
  // making below. And the AndroidShellHolder then owns the EmbedderEngine.
  fml::WeakPtr<PlatformViewAndroid> weak_platform_view;

  // Take out the old AndroidContext to reuse inside the PlatformViewAndroid
  // of the new Shell.
  PlatformViewAndroid* android_platform_view = platform_view_.get();
  // There's some indirection with platform_view_ being a weak pointer but
  // we just checked that the embedder_engine_ exists above and a valid shell is
  // the owner of the platform view so this weak pointer always exists.
  FML_DCHECK(android_platform_view);
  std::shared_ptr<flutter::AndroidContext> android_context =
      android_platform_view->GetAndroidContext();
  FML_DCHECK(android_context);

  // This is a synchronous call, so the captures don't have race checks.
  std::unique_ptr<PlatformViewAndroid> platform_view_android;

  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&jni_facade, &platform_view_android, android_context](Shell& shell) {
        platform_view_android = std::make_unique<PlatformViewAndroid>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            android_context          // Android context
        );

        auto embedder_surface = platform_view_android->TakeSurface();

        PlatformViewEmbedder::PlatformDispatchTable dispatch_table;

        dispatch_table.update_semantics_callback =
            [platform_view = platform_view_android.get()](
                int64_t view_id, flutter::SemanticsNodeUpdates update,
                flutter::CustomAccessibilityActionUpdates actions) {
              platform_view->UpdateSemantics(view_id, std::move(update),
                                             std::move(actions));
            };

        dispatch_table.platform_message_response_callback =
            [platform_view = platform_view_android.get()](
                std::unique_ptr<PlatformMessage> message) {
              platform_view->HandlePlatformMessage(std::move(message));
            };

        dispatch_table.on_pre_engine_restart_callback =
            [platform_view = platform_view_android.get()]() {
              platform_view->OnPreEngineRestart();
            };

        return std::make_unique<PlatformViewEmbedder>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            std::move(embedder_surface), dispatch_table,
            nullptr  // external view embedder
        );
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  auto config = BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  auto config2 = BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  if (!config || !config2) {
    // If the RunConfiguration was null, the kernel blob wasn't readable.
    // Fail the whole thing.
    return nullptr;
  }
  config->SetEngineId(engine_id);
  config2->SetEngineId(engine_id);

  std::unique_ptr<flutter::Shell> spawned_shell =
      embedder_engine_->GetShell().Spawn(std::move(config.value()),
                                         initial_route, on_create_platform_view,
                                         on_create_rasterizer);

  if (!spawned_shell) {
    return nullptr;
  }

  auto spawned_embedder_engine = std::make_unique<EmbedderEngine>(
      thread_host_, embedder_engine_->GetTaskRunners(),
      std::move(config2.value()), std::move(spawned_shell),
      nullptr  // external_texture_resolver
  );

  return std::unique_ptr<AndroidShellHolder>(new AndroidShellHolder(
      GetSettings(), jni_facade, thread_host_,
      std::move(spawned_embedder_engine), apk_asset_provider_->Clone(),
      std::move(platform_view_android), android_context->RenderingApi()));
}

void AndroidShellHolder::Launch(const std::string& entrypoint,
                                const std::string& libraryUrl,
                                const std::vector<std::string>& entrypoint_args,
                                int64_t engine_id) {
  if (!IsValid()) {
    return;
  }

  auto config = BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  if (!config) {
    return;
  }
  config->SetEngineId(engine_id);
  UpdateDisplayMetrics();
  embedder_engine_->SetRunConfiguration(std::move(config.value()));
  embedder_engine_->RunRootIsolate();
}

void AndroidShellHolder::SetAPKAssetProvider(
    std::unique_ptr<APKAssetProvider> apk_asset_provider) {
  apk_asset_provider_ = std::move(apk_asset_provider);
}

Rasterizer::Screenshot AndroidShellHolder::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  if (!IsValid()) {
    return {nullptr, DlISize(), "", Rasterizer::ScreenshotFormat::kUnknown};
  }
  return embedder_engine_->GetShell().Screenshot(type, base64_encode);
}

fml::WeakPtr<PlatformViewAndroid> AndroidShellHolder::GetPlatformView() {
  FML_DCHECK(platform_view_);
  return platform_view_->GetWeakPtr();
}

fml::WeakPtr<PlatformView> AndroidShellHolder::GetEnginePlatformView() const {
  FML_DCHECK(embedder_engine_);
  return embedder_engine_->GetShell().GetPlatformView();
}

void AndroidShellHolder::NotifyLowMemoryWarning() {
  FML_DCHECK(embedder_engine_);
  embedder_engine_->GetShell().NotifyLowMemoryWarning();
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
  if (apk_asset_provider_) {
    config.AddAssetResolver(apk_asset_provider_->Clone());
  }

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
  embedder_engine_->GetShell().OnDisplayUpdates(std::move(displays));
}

bool AndroidShellHolder::IsSurfaceControlEnabled() {
  return GetPlatformView()->IsSurfaceControlEnabled();
}

}  // namespace flutter
