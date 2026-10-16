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
#include "flutter/shell/platform/android/android_display.h"
#include "flutter/shell/platform/android/android_image_generator.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/embedder/embedder_engine.h"
#include "flutter/shell/platform/embedder/platform_view_embedder.h"

namespace flutter {

static PlatformViewEmbedder::PlatformDispatchTable CreateDispatchTable(
    PlatformViewAndroid* platform_view) {
  PlatformViewEmbedder::PlatformDispatchTable dispatch_table;
  dispatch_table.update_semantics_callback =
      [platform_view](int64_t view_id, flutter::SemanticsNodeUpdates update,
                      flutter::CustomAccessibilityActionUpdates actions) {
        platform_view->UpdateSemantics(view_id, std::move(update),
                                       std::move(actions));
      };
  dispatch_table.platform_message_response_callback =
      [platform_view](std::unique_ptr<PlatformMessage> message) {
        platform_view->HandlePlatformMessage(std::move(message));
      };
  dispatch_table.compute_platform_resolved_locale_callback =
      [platform_view](const std::vector<std::string>& supported_locale_data) {
        return platform_view->ComputePlatformResolvedLocales(
            supported_locale_data);
      };
  dispatch_table.on_pre_engine_restart_callback = [platform_view]() {
    platform_view->OnPreEngineRestart();
  };
  dispatch_table.create_rendering_surface_callback = [platform_view]() {
    return platform_view->CreateRenderingSurface();
  };
  dispatch_table.create_external_view_embedder_callback = [platform_view]() {
    return platform_view->CreateExternalViewEmbedder();
  };
  dispatch_table.create_snapshot_surface_producer_callback = [platform_view]() {
    return platform_view->CreateSnapshotSurfaceProducer();
  };
  dispatch_table.create_resource_context_callback = [platform_view]() {
    return platform_view->CreateResourceContext();
  };
  dispatch_table.release_resource_context_callback = [platform_view]() {
    platform_view->ReleaseResourceContext();
  };
  dispatch_table.get_impeller_context_callback = [platform_view]() {
    return platform_view->GetImpellerContext();
  };
  dispatch_table.setup_impeller_context_callback = [platform_view]() {
    platform_view->SetupImpellerContext();
  };
  dispatch_table.create_vsync_waiter_callback = [platform_view]() {
    return platform_view->CreateVSyncWaiter();
  };
  dispatch_table.request_dart_deferred_library_callback =
      [platform_view](intptr_t loading_unit_id) {
        platform_view->RequestDartDeferredLibrary(loading_unit_id);
      };
  dispatch_table.get_scaled_font_size_callback =
      [platform_view](double unscaled_font_size, int configuration_id) {
        return platform_view->GetScaledFontSize(unscaled_font_size,
                                                configuration_id);
      };
  dispatch_table.set_application_locale_callback =
      [platform_view](std::string locale) {
        platform_view->SetApplicationLocale(std::move(locale));
      };
  dispatch_table.set_semantics_tree_enabled_callback =
      [platform_view](bool enabled) {
        platform_view->SetSemanticsTreeEnabled(enabled);
      };
  dispatch_table.custom_platform_message_handler =
      platform_view->GetPlatformMessageHandler();
  return dispatch_table;
}

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

namespace {

// The holder owns its engine through an embedder API handle. Where embedder.h
// has no equivalent yet, the holder reaches past the handle with a cast or
// GetShell(). Keep each of those visible at its use so that
// ci/bin/android_embedder_ratchet.dart counts it; the counts must reach zero
// before the holder switches to FlutterEngineInitialize.

Shell& GetShell(FLUTTER_API_SYMBOL(FlutterEngine) engine) {
  return reinterpret_cast<EmbedderEngine*>(engine)->GetShell();
}

FlutterEngineProcTable LoadEmbedderApi() {
  FlutterEngineProcTable embedder_api = {};
  embedder_api.struct_size = sizeof(FlutterEngineProcTable);
  FlutterEngineResult result = FlutterEngineGetProcAddresses(&embedder_api);
  FML_CHECK(result == kSuccess)
      << "FlutterEngineGetProcAddresses failed: " << result;
  return embedder_api;
}

}  // namespace

AndroidShellHolder::AndroidShellHolder(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(jni_facade),
      embedder_api_(LoadEmbedderApi()),
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
      [this, &jni_facade, &weak_platform_view, rendering_api](Shell& shell) {
        platform_view_android_ = std::make_unique<PlatformViewAndroid>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            rendering_api            // rendering API
        );
        weak_platform_view = platform_view_android_->GetWeakPtr();
        auto platform_view_embedder = std::make_unique<PlatformViewEmbedder>(
            shell, shell.GetTaskRunners(),
            std::unique_ptr<EmbedderSurface>(nullptr),
            CreateDispatchTable(platform_view_android_.get()), nullptr);
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

  // The same Shell::Create that FlutterEngineInitialize ends in, minus what
  // Android does itself: the holder owns the threads behind |task_runners|,
  // and Launch() runs the root isolate once the APK assets are known.
  auto engine = std::make_unique<EmbedderEngine>(
      /*thread_host=*/nullptr, task_runners, settings_,
      /*run_configuration=*/std::nullopt, on_create_platform_view,
      on_create_rasterizer, /*external_texture_resolver=*/nullptr);
  if (engine->LaunchShell(GetDefaultPlatformData())) {
    engine_.reset(
        reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(engine.release()));
  }

  if (engine_) {
    GetShell(engine_.get())
        .GetDartVM()
        ->GetConcurrentMessageLoop()
        ->PostTaskToAllWorkers([]() {
          if (::setpriority(PRIO_PROCESS, gettid(), 1) != 0) {
            FML_LOG(ERROR) << "Failed to set Workers task runner priority";
          }
        });

    GetShell(engine_.get())
        .RegisterImageDecoder(
            [runner = task_runners.GetIOTaskRunner()](sk_sp<SkData> buffer) {
              return AndroidImageGenerator::MakeFromData(std::move(buffer),
                                                         runner);
            },
            -1);
    FML_LOG(INFO) << "Registered Android SDK image decoder (API level 28+)";
  }

  platform_view_ = weak_platform_view;
  FML_DCHECK(platform_view_);
  is_valid_ = engine_ != nullptr;
}

AndroidShellHolder::AndroidShellHolder(
    const Settings& settings,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<ThreadHost>& thread_host,
    UniqueEngine engine,
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    std::unique_ptr<PlatformViewAndroid> platform_view_android,
    const fml::WeakPtr<PlatformViewAndroid>& platform_view,
    AndroidRenderingAPI rendering_api)
    : settings_(settings),
      jni_facade_(jni_facade),
      platform_view_android_(std::move(platform_view_android)),
      platform_view_(platform_view),
      thread_host_(thread_host),
      engine_(std::move(engine)),
      embedder_api_(LoadEmbedderApi()),
      apk_asset_provider_(std::move(apk_asset_provider)),
      android_rendering_api_(rendering_api) {
  FML_DCHECK(jni_facade);
  FML_DCHECK(engine_);
  FML_DCHECK(GetShell(engine_.get()).IsSetup());
  FML_DCHECK(platform_view_android_);
  FML_DCHECK(platform_view_);
  FML_DCHECK(thread_host_);
  is_valid_ = engine_ != nullptr;
}

AndroidShellHolder::~AndroidShellHolder() {
  engine_.reset();
  thread_host_.reset();
  platform_view_android_.reset();
}

void AndroidShellHolder::EngineDeleter::operator()(
    FLUTTER_API_SYMBOL(FlutterEngine) engine) const {
  // Deletes the engine the way the Shell was deleted before it moved behind
  // a handle. FlutterEngineShutdown would also call NotifyDestroyed() on the
  // platform view first.
  delete reinterpret_cast<EmbedderEngine*>(engine);
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
  FML_DCHECK(engine_ && GetShell(engine_.get()).IsSetup())
      << "A new Shell can only be spawned "
         "if the current Shell is properly constructed";

  // Pull out the new PlatformViewAndroid from the new Shell to feed to it to
  // the new AndroidShellHolder.
  std::unique_ptr<PlatformViewAndroid> spawned_platform_view_android;
  fml::WeakPtr<PlatformViewAndroid> weak_platform_view;

  // Take out the old AndroidContext to reuse inside the PlatformViewAndroid
  // of the new Shell.
  PlatformViewAndroid* android_platform_view = platform_view_.get();
  FML_DCHECK(android_platform_view);
  std::shared_ptr<flutter::AndroidContext> android_context =
      android_platform_view->GetAndroidContext();
  FML_DCHECK(android_context);

  // This is a synchronous call, so the captures don't have race checks.
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&jni_facade, android_context, &spawned_platform_view_android,
       &weak_platform_view](Shell& shell) {
        spawned_platform_view_android = std::make_unique<PlatformViewAndroid>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            jni_facade,              // JNI interop
            android_context          // Android context
        );
        weak_platform_view = spawned_platform_view_android->GetWeakPtr();
        auto platform_view_embedder = std::make_unique<PlatformViewEmbedder>(
            shell, shell.GetTaskRunners(),
            std::unique_ptr<EmbedderSurface>(nullptr),
            CreateDispatchTable(spawned_platform_view_android.get()), nullptr);
        spawned_platform_view_android->SetPlatformView(
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

  // embedder.h has no spawn API yet.
  std::unique_ptr<EmbedderEngine> spawned =
      reinterpret_cast<EmbedderEngine*>(engine_.get())
          ->Spawn(std::move(config.value()), initial_route,
                  on_create_platform_view, on_create_rasterizer);
  UniqueEngine engine(
      reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(spawned.release()));

  return std::unique_ptr<AndroidShellHolder>(new AndroidShellHolder(
      GetSettings(), jni_facade, thread_host_, std::move(engine),
      apk_asset_provider_->Clone(), std::move(spawned_platform_view_android),
      weak_platform_view, android_context->RenderingApi()));
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
  auto config = BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  if (!config) {
    return;
  }
  config->SetEngineId(engine_id);
  UpdateDisplayMetrics();
  GetShell(engine_.get()).RunEngine(std::move(config.value()));
}

Rasterizer::Screenshot AndroidShellHolder::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  if (!IsValid()) {
    return {nullptr, DlISize(), "", Rasterizer::ScreenshotFormat::kUnknown};
  }
  return GetShell(engine_.get()).Screenshot(type, base64_encode);
}

fml::WeakPtr<PlatformViewAndroid> AndroidShellHolder::GetPlatformView() {
  FML_DCHECK(platform_view_);
  return platform_view_;
}

void AndroidShellHolder::NotifySurfaceCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (GetPlatformView()->HasAndroidSurface()) {
    FlutterEngineResult callback_result = embedder_api_.SetNextFrameCallback(
        engine_.get(),
        [](void* user_data) {
          static_cast<PlatformViewAndroid*>(user_data)->OnNextFrame();
        },
        platform_view_android_.get());
    if (callback_result != kSuccess) {
      FML_LOG(ERROR) << "FlutterEngineSetNextFrameCallback failed: "
                     << callback_result;
    }
  }
  GetPlatformView()->NotifyCreated(std::move(native_window));
  FlutterEngineResult result = embedder_api_.NotifyCreated(engine_.get());
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineNotifyCreated failed: " << result;
  }
}

void AndroidShellHolder::NotifySurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  GetPlatformView()->NotifySurfaceWindowChanged(std::move(native_window));
  ScheduleFrame();
}

void AndroidShellHolder::NotifySurfaceChanged(const DlISize& size) {
  GetPlatformView()->NotifyChanged(size);
}

void AndroidShellHolder::NotifySurfaceDestroyed() {
  FlutterEngineResult result = embedder_api_.NotifyDestroyed(engine_.get());
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineNotifyDestroyed failed: " << result;
  }
  GetPlatformView()->NotifyDestroyed();
}

void AndroidShellHolder::SetViewportMetrics(int64_t view_id,
                                            const ViewportMetrics& metrics) {
  GetPlatformView()->SetViewportMetrics(view_id, metrics);
}

void AndroidShellHolder::DispatchPlatformMessage(JNIEnv* env,
                                                 std::string name,
                                                 jobject message_data,
                                                 jint message_position,
                                                 jint response_id) {
  GetPlatformView()->DispatchPlatformMessage(env, std::move(name), message_data,
                                             message_position, response_id);
}

void AndroidShellHolder::DispatchEmptyPlatformMessage(JNIEnv* env,
                                                      std::string name,
                                                      jint response_id) {
  GetPlatformView()->DispatchEmptyPlatformMessage(env, std::move(name),
                                                  response_id);
}

void AndroidShellHolder::DispatchPointerDataPacket(
    std::unique_ptr<PointerDataPacket> packet) {
  GetPlatformView()->DispatchPointerDataPacket(std::move(packet));
}

void AndroidShellHolder::DispatchSemanticsAction(JNIEnv* env,
                                                 jint id,
                                                 jint action,
                                                 jobject args,
                                                 jint args_position) {
  GetPlatformView()->DispatchSemanticsAction(env, id, action, args,
                                             args_position);
}

void AndroidShellHolder::SetSemanticsEnabled(bool enabled) {
  FlutterEngineResult result =
      embedder_api_.UpdateSemanticsEnabled(engine_.get(), enabled);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineUpdateSemanticsEnabled failed: " << result;
  }
}

void AndroidShellHolder::SetAccessibilityFeatures(int32_t flags) {
  FlutterEngineResult result = embedder_api_.UpdateAccessibilityFeatures(
      engine_.get(), static_cast<FlutterAccessibilityFeature>(flags));
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineUpdateAccessibilityFeatures failed: "
                   << result;
  }
}

void AndroidShellHolder::RegisterExternalTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) {
  GetPlatformView()->RegisterExternalTexture(texture_id, surface_texture);
}

void AndroidShellHolder::RegisterImageTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    ImageExternalTexture::ImageLifecycle lifecycle) {
  GetPlatformView()->RegisterImageTexture(texture_id, image_texture_entry,
                                          lifecycle);
}

void AndroidShellHolder::UnregisterTexture(int64_t texture_id) {
  GetPlatformView()->UnregisterTexture(texture_id);
}

void AndroidShellHolder::MarkTextureFrameAvailable(int64_t texture_id) {
  GetPlatformView()->MarkTextureFrameAvailable(texture_id);
}

void AndroidShellHolder::ScheduleFrame() {
  FlutterEngineResult result = embedder_api_.ScheduleFrame(engine_.get());
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineScheduleFrame failed: " << result;
  }
}

void AndroidShellHolder::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  GetPlatformView()->LoadDartDeferredLibrary(loading_unit_id,
                                             std::move(snapshot_data),
                                             std::move(snapshot_instructions));
}

void AndroidShellHolder::UpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  GetPlatformView()->UpdateAssetResolverByType(
      std::move(updated_asset_resolver), type);
}

void AndroidShellHolder::NotifyLowMemoryWarning() {
  FML_DCHECK(engine_);
  GetShell(engine_.get()).NotifyLowMemoryWarning();
}

const std::shared_ptr<PlatformMessageHandler>&
AndroidShellHolder::GetPlatformMessageHandler() const {
  return GetShell(engine_.get()).GetPlatformMessageHandler();
}

Shell* AndroidShellHolder::GetShellForTesting() const {
  return engine_ ? &GetShell(engine_.get()) : nullptr;
}

FlutterEngineProcTable& AndroidShellHolder::GetEmbedderApiForTesting() {
  return embedder_api_;
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
  GetShell(engine_.get()).OnDisplayUpdates(std::move(displays));
}

bool AndroidShellHolder::IsSurfaceControlEnabled() {
  return GetPlatformView()->IsSurfaceControlEnabled();
}

}  // namespace flutter
