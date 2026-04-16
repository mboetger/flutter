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
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/android_compositor_vulkan.h"
#include "flutter/shell/platform/android/android_display.h"
#include "flutter/shell/platform/android/android_image_generator.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/embedder/embedder_engine.h"
#include "flutter/shell/platform/embedder/embedder_external_view_embedder.h"
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
static PlatformData GetDefaultPlatformData() {
  PlatformData platform_data;
  platform_data.lifecycle_state = "AppLifecycleState.detached";
  return platform_data;
}

static PlatformViewEmbedder::PlatformDispatchTable CreateDispatchTable(
    const fml::WeakPtr<PlatformViewAndroid>& platform_view) {
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
      [platform_view](int response_id, std::unique_ptr<fml::Mapping> mapping) {
        if (platform_view) {
          platform_view->GetPlatformMessageHandler()
              ->InvokePlatformMessageResponseCallback(response_id,
                                                      std::move(mapping));
        }
      };
  dispatch_table.platform_message_empty_response_completion_callback =
      [platform_view](int response_id) {
        if (platform_view) {
          platform_view->GetPlatformMessageHandler()
              ->InvokePlatformMessageEmptyResponseCallback(response_id);
        }
      };
  dispatch_table.vsync_callback = [platform_view](intptr_t baton) {
    if (platform_view) {
      platform_view->OnVsyncCallback(baton);
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
  dispatch_table.on_pre_engine_restart_callback = [platform_view]() {
    if (platform_view) {
      platform_view->OnPreEngineRestart();
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

AndroidShellHolder::AndroidShellHolder(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(jni_facade),
      android_rendering_api_(android_rendering_api) {
  static size_t thread_host_count = 1;
  auto thread_label = std::to_string(thread_host_count++);

  // Ensure that the message loop is initialized for the current thread
  // which is used as the platform thread.
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  // 1. Setup Task Runners.
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

  std::shared_ptr<ThreadHost> thread_host =
      std::make_shared<ThreadHost>(host_config);

  fml::RefPtr<fml::TaskRunner> raster_runner =
      thread_host->raster_thread->GetTaskRunner();
  fml::RefPtr<fml::TaskRunner> ui_runner;
  fml::RefPtr<fml::TaskRunner> io_runner =
      thread_host->io_thread->GetTaskRunner();
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();

  if (settings.merged_platform_ui_thread ==
      Settings::MergedPlatformUIThread::kEnabled) {
    ui_runner = platform_runner;
  } else {
    ui_runner = thread_host->ui_thread->GetTaskRunner();
  }

  flutter::TaskRunners task_runners(thread_label,     // label
                                    platform_runner,  // platform
                                    raster_runner,    // raster
                                    ui_runner,        // ui
                                    io_runner         // io
  );

  // 2. Prepare FlutterProjectArgs.
  FlutterProjectArgs args = {};
  args.struct_size = sizeof(FlutterProjectArgs);
  args.assets_path = settings_.assets_path.c_str();
  args.icu_data_path = settings_.icu_data_path.c_str();

  // We'll fill in more args as we migrate further.
  // For now, we still need to create the PlatformViewAndroid.

  // 3. Initialize the engine using the public API.
  // Note: FlutterEngineInitialize currently takes a FlutterRendererConfig.
  // On Android, we have a complex setup with AndroidContext and
  // EmbedderSurfaceAndroid.
  // We'll need to create a FlutterRendererConfig that wraps our
  // Android-specific rendering logic.

  // ... (Implementation continues)
  fml::WeakPtr<PlatformViewAndroid> weak_platform_view;
  AndroidRenderingAPI rendering_api = android_rendering_api_;
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [jni_facade, &weak_platform_view, rendering_api, this](Shell& shell) {
        auto context_settings =
            PlatformViewAndroid::CreateContextSettings(shell.GetSettings());
        auto android_context = PlatformViewAndroid::CreateAndroidContext(
            shell.GetTaskRunners(), rendering_api,
            shell.GetSettings().enable_opengl_gpu_tracing, context_settings);

        auto embedder_surface = std::make_unique<EmbedderSurfaceAndroid>(
            android_context, shell.GetSettings().enable_impeller,
            shell.GetSettings().impeller_enable_lazy_shader_mode);
        embedder_surface_ = embedder_surface.get();

        platform_view_android_ = std::make_unique<PlatformViewAndroid>(
            engine_,                 // engine handle
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            android_context,         // Android context
            embedder_surface.get()   // Embedder surface
        );
        weak_platform_view = platform_view_android_->GetWeakPtr();
        platform_view_ = weak_platform_view;

        auto dispatch_table = CreateDispatchTable(weak_platform_view);

        std::shared_ptr<EmbedderExternalViewEmbedder> external_view_embedder;

        if (shell.GetSettings().enable_impeller &&
            rendering_api == AndroidRenderingAPI::kImpellerVulkan) {
          auto impeller_context = android_context->GetImpellerContext();
          if (impeller_context) {
            auto context_vk =
                std::static_pointer_cast<impeller::ContextVK>(impeller_context);
            android_compositor_vulkan_ =
                std::make_unique<AndroidCompositorVulkan>(context_vk);
            platform_view_android_->SetCompositor(
                android_compositor_vulkan_.get());

            external_view_embedder =
                std::make_shared<EmbedderExternalViewEmbedder>(
                    false,  // avoid_backing_store_cache
                    [this](GrDirectContext* context,
                           const std::shared_ptr<impeller::AiksContext>&
                               aiks_context,
                           const FlutterBackingStoreConfig& config) {
                      return android_compositor_vulkan_->CreateRenderTarget(
                          aiks_context, config);
                    },
                    [this](FlutterViewId view_id,
                           const std::vector<const FlutterLayer*>& layers) {
                      FlutterPresentViewInfo info = {};
                      info.struct_size = sizeof(FlutterPresentViewInfo);
                      info.view_id = view_id;
                      info.layers =
                          const_cast<const FlutterLayer**>(layers.data());
                      info.layers_count = layers.size();
                      info.user_data = android_compositor_vulkan_.get();
                      return android_compositor_vulkan_->PresentView(&info);
                    });
          }
        }

        auto platform_view_embedder = std::make_unique<PlatformViewEmbedder>(
            shell, shell.GetTaskRunners(), std::move(embedder_surface),
            dispatch_table, external_view_embedder);
        platform_view_android_->SetPlatformView(
            platform_view_embedder->GetWeakPtr());

        return platform_view_embedder;
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  std::unique_ptr<Shell> shell =
      Shell::Create(GetDefaultPlatformData(),  // window data
                    task_runners,              // task runners
                    settings_,                 // settings
                    on_create_platform_view,   // platform view create callback
                    on_create_rasterizer       // rasterizer create callback
      );

  if (shell) {
    shell->GetDartVM()->GetConcurrentMessageLoop()->PostTaskToAllWorkers([]() {
      if (::setpriority(PRIO_PROCESS, gettid(), 1) != 0) {
        FML_LOG(ERROR) << "Failed to set Workers task runner priority";
      }
    });

    shell->RegisterImageDecoder(
        [runner = task_runners.GetIOTaskRunner()](sk_sp<SkData> buffer) {
          return AndroidImageGenerator::MakeFromData(std::move(buffer), runner);
        },
        -1);
    FML_DLOG(INFO) << "Registered Android SDK image decoder (API level 28+)";

    auto embedder_thread_host =
        EmbedderThreadHost::Create(std::move(thread_host), task_runners);
    auto embedder_engine = EmbedderEngine::Create(
        std::move(embedder_thread_host), task_runners, std::move(shell),
        std::make_unique<EmbedderExternalTextureResolver>());
    engine_ = reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(
        embedder_engine.release());

    if (platform_view_android_) {
      platform_view_android_->SetEngine(engine_);
    }
  }

  FML_DCHECK(platform_view_);
  is_valid_ = engine_ != nullptr;
}

AndroidShellHolder::AndroidShellHolder(
    const flutter::Settings& settings,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
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
      engine_(engine),
      apk_asset_provider_(std::move(apk_asset_provider)),
      android_rendering_api_(rendering_api) {
  FML_DCHECK(jni_facade);
  FML_DCHECK(engine_);
  FML_DCHECK(reinterpret_cast<EmbedderEngine*>(engine_)->GetShell().IsSetup());
  FML_DCHECK(platform_view_);
  is_valid_ = engine_ != nullptr;
}

AndroidShellHolder::~AndroidShellHolder() {
  if (engine_) {
    FlutterEngineShutdown(engine_);
    engine_ = nullptr;
  }
}

const std::shared_ptr<PlatformMessageHandler>&
AndroidShellHolder::GetPlatformMessageHandler() const {
  return reinterpret_cast<EmbedderEngine*>(engine_)
      ->GetShell()
      .GetPlatformMessageHandler();
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
  FML_DCHECK(engine_);

  // Pull out the new PlatformViewAndroid from the new Shell to feed to it to
  // the new AndroidShellHolder.
  //
  // It's a weak pointer because it's owned by the Shell (which we're also)
  // making below. And the AndroidShellHolder then owns the Shell.
  std::unique_ptr<PlatformViewAndroid> platform_view_android_out;
  EmbedderSurfaceAndroid* embedder_surface_out = nullptr;
  fml::WeakPtr<PlatformViewAndroid> weak_platform_view;

  // Take out the old AndroidContext to reuse inside the PlatformViewAndroid
  // of the new Shell.
  PlatformViewAndroid* android_platform_view = platform_view_android_.get();
  // There's some indirection with platform_view_ being a weak pointer but
  // we just checked that the engine_ exists above and a valid shell is the
  // owner of the platform view so this weak pointer always exists.
  FML_DCHECK(android_platform_view);
  std::shared_ptr<flutter::AndroidContext> android_context =
      android_platform_view->GetAndroidContext();
  FML_DCHECK(android_context);

  // This is a synchronous call, so the captures don't have race checks.
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [jni_facade, android_context, &platform_view_android_out,
       &embedder_surface_out, &weak_platform_view](Shell& shell) {
        auto embedder_surface = std::make_unique<EmbedderSurfaceAndroid>(
            android_context, shell.GetSettings().enable_impeller,
            shell.GetSettings().impeller_enable_lazy_shader_mode);
        embedder_surface_out = embedder_surface.get();

        platform_view_android_out = std::make_unique<PlatformViewAndroid>(
            nullptr,                 // engine handle not yet available
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

  auto* parent_embedder_engine = reinterpret_cast<EmbedderEngine*>(engine_);
  auto spawned_embedder_engine = parent_embedder_engine->Spawn(
      parent_embedder_engine->GetThreadHost(),
      parent_embedder_engine->GetTaskRunners(), std::move(config.value()),
      initial_route, on_create_platform_view, on_create_rasterizer,
      std::make_unique<EmbedderExternalTextureResolver>());

  if (!spawned_embedder_engine) {
    return nullptr;
  }

  auto spawned_engine_handle =
      reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(
          spawned_embedder_engine.release());

  if (platform_view_android_out) {
    platform_view_android_out->SetEngine(spawned_engine_handle);
  }

  return std::unique_ptr<AndroidShellHolder>(new AndroidShellHolder(
      GetSettings(), jni_facade, spawned_engine_handle,
      apk_asset_provider_ ? apk_asset_provider_->Clone() : nullptr,
      std::move(platform_view_android_out), embedder_surface_out,
      android_context->RenderingApi()));
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
  auto run_configuration =
      BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  if (!run_configuration) {
    return;
  }
  run_configuration->SetEngineId(engine_id);
  UpdateDisplayMetrics();

  // We should really be using FlutterEngineRun here.
  // For now, let's keep the internal call but wrap it slightly.
  reinterpret_cast<EmbedderEngine*>(engine_)->GetShell().RunEngine(
      std::move(run_configuration.value()));
}

Rasterizer::Screenshot AndroidShellHolder::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  if (!IsValid()) {
    return {nullptr, DlISize(), "", Rasterizer::ScreenshotFormat::kUnknown};
  }
  return reinterpret_cast<EmbedderEngine*>(engine_)->GetShell().Screenshot(
      type, base64_encode);
}

fml::WeakPtr<PlatformView> AndroidShellHolder::GetPlatformView() {
  FML_DCHECK(engine_);
  return reinterpret_cast<EmbedderEngine*>(engine_)
      ->GetShell()
      .GetPlatformView();
}

PlatformViewAndroid* AndroidShellHolder::GetPlatformViewAndroid() {
  return platform_view_android_.get();
}

EmbedderSurfaceAndroid* AndroidShellHolder::GetEmbedderSurfaceAndroid() {
  return embedder_surface_;
}

void AndroidShellHolder::NotifyLowMemoryWarning() {
  FML_DCHECK(engine_);
  reinterpret_cast<EmbedderEngine*>(engine_)
      ->GetShell()
      .NotifyLowMemoryWarning();
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
  reinterpret_cast<EmbedderEngine*>(engine_)->GetShell().OnDisplayUpdates(
      std::move(displays));
}

bool AndroidShellHolder::IsSurfaceControlEnabled() {
  return GetPlatformViewAndroid()->IsSurfaceControlEnabled();
}

}  // namespace flutter
