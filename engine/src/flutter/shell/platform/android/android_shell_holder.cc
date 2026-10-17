// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <pthread.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/settings.h"
#include "flutter/fml/cpu_affinity.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/platform/android/android_image_generator.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/embedder/embedder_engine.h"
#include "flutter/shell/platform/embedder/platform_view_embedder.h"

namespace flutter {

// The state behind the FlutterProjectArgs callbacks of one engine. The engine
// passes it back to them as user_data, and its holder keeps it alive until
// the engine has shut down.
class AndroidEngineCallbacks {
 public:
  explicit AndroidEngineCallbacks(
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade)
      : jni_facade(std::move(jni_facade)) {}

  // Asks Java which of the supported locales the platform would pick and
  // returns it in storage that stays valid until the next call, as
  // FlutterComputePlatformResolvedLocaleCallback2 allows. Java may answer
  // with codes that differ from the supported ones (for example "iw" for
  // "he"), so the answer is not matched back to an input locale.
  const FlutterLocale* ComputePlatformResolvedLocale(
      const FlutterLocale** supported_locales,
      size_t locale_count) {
    std::vector<std::string> supported_locale_data;
    supported_locale_data.reserve(locale_count * kStringsPerLocale);
    for (size_t i = 0; i < locale_count; ++i) {
      const FlutterLocale* locale = supported_locales[i];
      supported_locale_data.emplace_back(
          locale->language_code ? locale->language_code : "");
      supported_locale_data.emplace_back(
          locale->country_code ? locale->country_code : "");
      supported_locale_data.emplace_back(
          locale->script_code ? locale->script_code : "");
    }
    std::unique_ptr<std::vector<std::string>> resolved =
        platform_view->ComputePlatformResolvedLocales(supported_locale_data);
    if (!resolved || resolved->empty()) {
      return nullptr;
    }
    resolved->resize(kStringsPerLocale);
    resolved_locale_codes_ = std::move(*resolved);
    resolved_locale_ = {
        .struct_size = sizeof(FlutterLocale),
        .language_code = resolved_locale_codes_[0].c_str(),
        .country_code = resolved_locale_codes_[1].c_str(),
        .script_code = resolved_locale_codes_[2].c_str(),
        .variant_code = nullptr,
    };
    return &resolved_locale_;
  }

  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade;
  PlatformViewAndroid* platform_view = nullptr;
  std::shared_ptr<VsyncWaiterAndroid> vsync_waiter;
  PlatformMessageHandlerAndroid* message_handler = nullptr;

 private:
  // Language, country and script code, the order Java uses for locales.
  static constexpr size_t kStringsPerLocale = 3;

  std::vector<std::string> resolved_locale_codes_;
  FlutterLocale resolved_locale_ = {};

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidEngineCallbacks);
};

static FlutterProjectArgs MakeProjectArgs() {
  FlutterProjectArgs args = {};
  args.struct_size = sizeof(FlutterProjectArgs);
  args.update_semantics_callback2 = [](const FlutterSemanticsUpdate2* update,
                                       void* user_data) {
    if (update != nullptr && user_data != nullptr) {
      static_cast<AndroidEngineCallbacks*>(user_data)
          ->platform_view->UpdateSemantics(*update);
    }
  };
  args.platform_message_callback = [](const FlutterPlatformMessage* message,
                                      void* user_data) {
    if (message != nullptr && user_data != nullptr) {
      static_cast<AndroidEngineCallbacks*>(user_data)
          ->message_handler->HandlePlatformMessage(*message);
    }
  };
  args.enable_direct_platform_message_dispatch = true;
  args.vsync_callback = [](void* user_data, intptr_t baton) {
    static_cast<AndroidEngineCallbacks*>(user_data)->vsync_waiter->AwaitVSync(
        baton);
  };
  args.compute_platform_resolved_locale_callback2 =
      [](const FlutterLocale** supported_locales, size_t locale_count,
         void* user_data) -> const FlutterLocale* {
    return static_cast<AndroidEngineCallbacks*>(user_data)
        ->ComputePlatformResolvedLocale(supported_locales, locale_count);
  };
  args.on_pre_engine_restart_callback = [](void* user_data) {
    static_cast<AndroidEngineCallbacks*>(user_data)
        ->platform_view->OnPreEngineRestart();
  };
  args.request_dart_deferred_library_callback = [](intptr_t loading_unit_id,
                                                   void* user_data) {
    static_cast<AndroidEngineCallbacks*>(user_data)
        ->platform_view->RequestDartDeferredLibrary(loading_unit_id);
  };
  args.set_application_locale_callback = [](const char* locale,
                                            void* user_data) {
    static_cast<AndroidEngineCallbacks*>(user_data)
        ->platform_view->SetApplicationLocale(locale ? locale : "");
  };
  args.set_semantics_tree_enabled_callback = [](bool enabled, void* user_data) {
    static_cast<AndroidEngineCallbacks*>(user_data)
        ->platform_view->SetSemanticsTreeEnabled(enabled);
  };
  return args;
}

static PlatformViewEmbedder::PlatformDispatchTable CreateDispatchTable(
    PlatformViewAndroid* platform_view,
    AndroidEngineCallbacks* callbacks) {
  const FlutterProjectArgs args = MakeProjectArgs();
  PlatformViewEmbedder::PlatformDispatchTable dispatch_table =
      PlatformDispatchTableFromProjectArgs(&args, callbacks);
  dispatch_table.create_rendering_surface_callback = [platform_view]() {
    return platform_view->CreateRenderingSurface();
  };
  dispatch_table.create_external_view_embedder_callback = [platform_view]() {
    return platform_view->CreateExternalViewEmbedder();
  };
  dispatch_table.create_snapshot_surface_producer_callback = [platform_view]() {
    return platform_view->CreateSnapshotSurfaceProducer();
  };
  dispatch_table.make_resource_current_callback = [platform_view]() {
    return platform_view->MakeResourceContextCurrent();
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
// has no equivalent yet, the holder reaches past the handle with a cast. Keep
// each of those visible at its use so that ci/bin/android_embedder_ratchet.dart
// counts it; the counts must reach zero before the holder switches to
// FlutterEngineInitialize.

FlutterEngineProcTable LoadEmbedderApi() {
  FlutterEngineProcTable embedder_api = {};
  embedder_api.struct_size = sizeof(FlutterEngineProcTable);
  FlutterEngineResult result = FlutterEngineGetProcAddresses(&embedder_api);
  FML_CHECK(result == kSuccess)
      << "FlutterEngineGetProcAddresses failed: " << result;
  return embedder_api;
}

// Gives the vsync source the engine handle once the holder owns the engine,
// or retires it when there is no engine. There is no source when the Shell
// failed before it created the platform view.
void AttachVsyncWaiter(VsyncWaiterAndroid* vsync_waiter,
                       FLUTTER_API_SYMBOL(FlutterEngine) engine,
                       const FlutterEngineProcTable* embedder_api) {
  if (vsync_waiter == nullptr) {
    return;
  }
  if (engine != nullptr) {
    vsync_waiter->Attach(engine, embedder_api);
  } else {
    vsync_waiter->Detach();
  }
}

uint64_t NextHolderId() {
  static std::atomic<uint64_t> next_id{1};
  return next_id.fetch_add(1, std::memory_order_relaxed);
}

// Stages platform-specific flutter::Texture instances created by
// PlatformViewAndroid immediately before FlutterEngineRegisterExternalTexture
// is invoked, so EmbedderExternalTextureResolver can claim ownership by
// (holder_id, texture_id) without exposing C++ texture types in embedder.h.
class PendingTextureTable {
 public:
  void Put(uint64_t holder_id,
           int64_t texture_id,
           std::unique_ptr<Texture> texture) {
    std::scoped_lock lock(mutex_);
    entries_[holder_id][texture_id] = std::move(texture);
  }

  std::unique_ptr<Texture> Take(uint64_t holder_id, int64_t texture_id) {
    std::scoped_lock lock(mutex_);
    auto holder_it = entries_.find(holder_id);
    if (holder_it == entries_.end()) {
      return nullptr;
    }
    auto tex_it = holder_it->second.find(texture_id);
    if (tex_it == holder_it->second.end()) {
      return nullptr;
    }
    std::unique_ptr<Texture> texture = std::move(tex_it->second);
    holder_it->second.erase(tex_it);
    if (holder_it->second.empty()) {
      entries_.erase(holder_it);
    }
    return texture;
  }

  void RemoveForHolder(uint64_t holder_id) {
    std::scoped_lock lock(mutex_);
    entries_.erase(holder_id);
  }

 private:
  std::mutex mutex_;
  std::unordered_map<uint64_t,
                     std::unordered_map<int64_t, std::unique_ptr<Texture>>>
      entries_;
};

PendingTextureTable& GetPendingTextureTable() {
  static auto* table = new PendingTextureTable();
  return *table;
}

std::unique_ptr<EmbedderExternalTextureResolver> MakeExternalTextureResolver(
    uint64_t holder_id) {
  return std::make_unique<EmbedderExternalTextureResolver>(
      [holder_id](int64_t texture_id) -> std::unique_ptr<Texture> {
        return GetPendingTextureTable().Take(holder_id, texture_id);
      });
}

struct ScopedRunConfiguration {
  std::vector<const char*> c_entrypoint_args;
  FlutterCustomAssetResolver custom_asset_resolver = {};
  FlutterEngineRunConfiguration config = {};

  ScopedRunConfiguration(const Settings& settings,
                         const APKAssetProvider* apk_asset_provider,
                         const std::string& entrypoint,
                         const std::string& library_url,
                         const std::vector<std::string>& entrypoint_args,
                         int64_t engine_id) {
    c_entrypoint_args.reserve(entrypoint_args.size());
    for (const std::string& arg : entrypoint_args) {
      c_entrypoint_args.push_back(arg.c_str());
    }

    const FlutterCustomAssetResolver* asset_resolver_ptr = nullptr;
    if (apk_asset_provider != nullptr) {
      custom_asset_resolver =
          apk_asset_provider->Clone()->ToFlutterCustomAssetResolver();
      asset_resolver_ptr = &custom_asset_resolver;
    }

    config.struct_size = sizeof(FlutterEngineRunConfiguration);
    config.kernel_blob_path = settings.application_kernel_asset.empty()
                                  ? nullptr
                                  : settings.application_kernel_asset.c_str();
    config.custom_dart_entrypoint =
        entrypoint.empty() ? nullptr : entrypoint.c_str();
    config.custom_dart_entrypoint_library =
        (!entrypoint.empty() && !library_url.empty()) ? library_url.c_str()
                                                      : nullptr;
    config.dart_entrypoint_argc = c_entrypoint_args.size();
    config.dart_entrypoint_argv =
        c_entrypoint_args.empty() ? nullptr : c_entrypoint_args.data();
    config.custom_asset_resolver = asset_resolver_ptr;
    config.engine_id = engine_id;
  }
};

}  // namespace

AndroidShellHolder::AndroidShellHolder(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(jni_facade),
      engine_callbacks_(std::make_unique<AndroidEngineCallbacks>(jni_facade)),
      embedder_api_(LoadEmbedderApi()),
      holder_id_(NextHolderId()),
      android_rendering_api_(android_rendering_api) {
  static size_t thread_host_count = 1;
  auto thread_label = std::to_string(thread_host_count++);

  thread_host_ = std::make_shared<ThreadHost>();
  if (settings.merged_platform_ui_thread !=
      Settings::MergedPlatformUIThread::kEnabled) {
    thread_host_->ui_thread = std::make_unique<fml::Thread>(
        AndroidPlatformThreadConfigSetter,
        fml::Thread::ThreadConfig(thread_label + ".ui",
                                  fml::Thread::ThreadPriority::kDisplay));
  }
  thread_host_->raster_thread = std::make_unique<fml::Thread>(
      AndroidPlatformThreadConfigSetter,
      fml::Thread::ThreadConfig(thread_label + ".raster",
                                fml::Thread::ThreadPriority::kRaster));
  thread_host_->io_thread = std::make_unique<fml::Thread>(
      AndroidPlatformThreadConfigSetter,
      fml::Thread::ThreadConfig(thread_label + ".io",
                                fml::Thread::ThreadPriority::kNormal));

  fml::WeakPtr<PlatformViewAndroid> weak_platform_view;
  std::shared_ptr<VsyncWaiterAndroid> vsync_waiter;
  platform_message_handler_ =
      std::make_shared<PlatformMessageHandlerAndroid>(jni_facade);
  PlatformMessageHandlerAndroid* message_handler_raw =
      platform_message_handler_.get();
  AndroidRenderingAPI rendering_api = android_rendering_api_;
  EmbedderEngine::PlatformViewFactory on_create_platform_view =
      [this, &jni_facade, &weak_platform_view, &vsync_waiter,
       message_handler_raw, rendering_api, callbacks = engine_callbacks_.get()](
          PlatformView::Delegate& delegate, const TaskRunners& runners) {
        vsync_waiter = std::make_shared<VsyncWaiterAndroid>(runners);
        platform_view_android_ = std::make_unique<PlatformViewAndroid>(
            delegate,      // delegate
            runners,       // task runners
            jni_facade,    // JNI interop
            rendering_api  // rendering API
        );
        weak_platform_view = platform_view_android_->GetWeakPtr();
        callbacks->platform_view = platform_view_android_.get();
        callbacks->vsync_waiter = vsync_waiter;
        callbacks->message_handler = message_handler_raw;
        auto platform_view_embedder = std::make_unique<PlatformViewEmbedder>(
            delegate, runners, std::unique_ptr<EmbedderSurface>(nullptr),
            CreateDispatchTable(platform_view_android_.get(), callbacks),
            nullptr);
        platform_view_android_->SetPlatformView(
            platform_view_embedder->GetWeakPtr());
        return platform_view_embedder;
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
      MakeExternalTextureResolver(holder_id_));

  auto* io_runner_ptr =
      new fml::RefPtr<fml::TaskRunner>(task_runners.GetIOTaskRunner());
  FlutterImageGeneratorRegistration image_generator_registration = {};
  image_generator_registration.struct_size =
      sizeof(FlutterImageGeneratorRegistration);
  image_generator_registration.priority = -1;
  image_generator_registration.user_data = io_runner_ptr;
  image_generator_registration.create_generator =
      [](void* user_data, const uint8_t* data, size_t data_size,
         FlutterImageGenerator* generator_out) -> bool {
    auto* runner = static_cast<fml::RefPtr<fml::TaskRunner>*>(user_data);
    return AndroidImageGenerator::CreateGenerator(data, data_size, *runner,
                                                  generator_out);
  };
  image_generator_registration.destruction_callback = [](void* user_data) {
    delete static_cast<fml::RefPtr<fml::TaskRunner>*>(user_data);
  };
  engine->AddImageDecoder(
      MakeImageGeneratorFactory(image_generator_registration),
      image_generator_registration.priority);
  engine->SetVMServiceUriCallback(&FlutterMain::OnVMServiceUriAvailable);

  if (engine->LaunchShell(GetDefaultPlatformData())) {
    engine_.reset(
        reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(engine.release()));
  }

  if (engine_) {
    // Concurrent worker threads are raw std::threads without an
    // fml::MessageLoop, whereas the dedicated IO thread (also tagged
    // kFlutterNativeThreadTypeWorker) always has an initialized
    // fml::MessageLoop.
    constexpr int kWorkerThreadPriority = 1;
    FlutterEngineResult priority_result =
        embedder_api_.PostCallbackOnAllNativeThreads(
            engine_.get(),
            [](FlutterNativeThreadType type, void* /*baton*/) {
              if (type == kFlutterNativeThreadTypeWorker &&
                  !fml::MessageLoop::IsInitializedForCurrentThread()) {
                if (::setpriority(PRIO_PROCESS, gettid(),
                                  kWorkerThreadPriority) != 0) {
                  FML_LOG(ERROR)
                      << "Failed to set Workers task runner priority";
                }
              }
            },
            nullptr);
    if (priority_result != kSuccess) {
      FML_LOG(ERROR) << "FlutterEnginePostCallbackOnAllNativeThreads failed: "
                     << priority_result;
    }

    FML_LOG(INFO) << "Registered Android SDK image decoder (API level 28+)";
  }

  platform_view_ = weak_platform_view;
  FML_DCHECK(platform_view_);
  platform_message_handler_->Attach(engine_.get(), &embedder_api_);
  vsync_waiter_ = std::move(vsync_waiter);
  AttachVsyncWaiter(vsync_waiter_.get(), engine_.get(), &embedder_api_);
  is_valid_ = engine_ != nullptr;
}

AndroidShellHolder::AndroidShellHolder(
    const Settings& settings,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<ThreadHost>& thread_host,
    std::unique_ptr<AndroidEngineCallbacks> engine_callbacks,
    UniqueEngine engine,
    uint64_t holder_id,
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    std::unique_ptr<PlatformViewAndroid> platform_view_android,
    const fml::WeakPtr<PlatformViewAndroid>& platform_view,
    std::shared_ptr<PlatformMessageHandlerAndroid> platform_message_handler,
    std::shared_ptr<VsyncWaiterAndroid> vsync_waiter,
    AndroidRenderingAPI rendering_api)
    : settings_(settings),
      jni_facade_(jni_facade),
      platform_view_android_(std::move(platform_view_android)),
      platform_view_(platform_view),
      platform_message_handler_(std::move(platform_message_handler)),
      thread_host_(thread_host),
      engine_callbacks_(std::move(engine_callbacks)),
      engine_(std::move(engine)),
      embedder_api_(LoadEmbedderApi()),
      vsync_waiter_(std::move(vsync_waiter)),
      holder_id_(holder_id),
      apk_asset_provider_(std::move(apk_asset_provider)),
      android_rendering_api_(rendering_api) {
  FML_DCHECK(jni_facade);
  FML_DCHECK(engine_);
  FML_DCHECK(platform_view_android_);
  FML_DCHECK(platform_view_);
  FML_DCHECK(platform_message_handler_);
  FML_DCHECK(thread_host_);
  platform_message_handler_->Attach(engine_.get(), &embedder_api_);
  // The spawned engine may already wait for a vsync; see VsyncWaiterAndroid.
  AttachVsyncWaiter(vsync_waiter_.get(), engine_.get(), &embedder_api_);
  is_valid_ = engine_ != nullptr;
  FML_DCHECK(IsValid());
}

AndroidShellHolder::~AndroidShellHolder() {
  if (vsync_waiter_) {
    vsync_waiter_->Detach();
  }
  if (platform_message_handler_) {
    platform_message_handler_->ReleasePendingResponses();
  }
  if (engine_) {
    FlutterEngineResult result = embedder_api_.Shutdown(engine_.release());
    if (result != kSuccess) {
      FML_LOG(ERROR) << "FlutterEngineShutdown failed: " << result;
    }
  }
  thread_host_.reset();
  platform_view_android_.reset();
  GetPendingTextureTable().RemoveForHolder(holder_id_);
}

void AndroidShellHolder::EngineDeleter::operator()(
    FLUTTER_API_SYMBOL(FlutterEngine) engine) const {
  FlutterEngineResult result = FlutterEngineShutdown(engine);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineShutdown failed: " << result;
  }
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
  FML_DCHECK(IsValid()) << "A new Shell can only be spawned "
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
  auto spawned_callbacks = std::make_unique<AndroidEngineCallbacks>(jni_facade);
  std::shared_ptr<VsyncWaiterAndroid> vsync_waiter;
  auto spawned_message_handler =
      std::make_shared<PlatformMessageHandlerAndroid>(jni_facade);
  PlatformMessageHandlerAndroid* spawned_message_handler_raw =
      spawned_message_handler.get();
  EmbedderEngine::PlatformViewFactory on_create_platform_view =
      [&jni_facade, android_context, &spawned_platform_view_android,
       &weak_platform_view, &vsync_waiter, spawned_message_handler_raw,
       callbacks = spawned_callbacks.get()](PlatformView::Delegate& delegate,
                                            const TaskRunners& runners) {
        vsync_waiter = std::make_shared<VsyncWaiterAndroid>(runners);
        spawned_platform_view_android = std::make_unique<PlatformViewAndroid>(
            delegate,        // delegate
            runners,         // task runners
            jni_facade,      // JNI interop
            android_context  // Android context
        );
        weak_platform_view = spawned_platform_view_android->GetWeakPtr();
        callbacks->platform_view = spawned_platform_view_android.get();
        callbacks->vsync_waiter = vsync_waiter;
        callbacks->message_handler = spawned_message_handler_raw;
        auto platform_view_embedder = std::make_unique<PlatformViewEmbedder>(
            delegate, runners, std::unique_ptr<EmbedderSurface>(nullptr),
            CreateDispatchTable(spawned_platform_view_android.get(), callbacks),
            nullptr);
        spawned_platform_view_android->SetPlatformView(
            platform_view_embedder->GetWeakPtr());
        return platform_view_embedder;
      };

  ScopedRunConfiguration run_config(GetSettings(), apk_asset_provider_.get(),
                                    entrypoint, libraryUrl, entrypoint_args,
                                    engine_id);

  const uint64_t spawned_holder_id = NextHolderId();
  // embedder.h has no spawn API yet.
  auto spawned =
      reinterpret_cast<EmbedderEngine*>(engine_.get())
          ->Spawn(&run_config.config, initial_route, on_create_platform_view,
                  MakeExternalTextureResolver(spawned_holder_id));
  if (!spawned) {
    // If Spawn failed, the kernel blob wasn't readable or the shell failed to
    // spawn.
    return nullptr;
  }
  UniqueEngine engine(
      reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(spawned.release()));

  return std::unique_ptr<AndroidShellHolder>(new AndroidShellHolder(
      GetSettings(), jni_facade, thread_host_, std::move(spawned_callbacks),
      std::move(engine), spawned_holder_id,
      apk_asset_provider_ ? apk_asset_provider_->Clone() : nullptr,
      std::move(spawned_platform_view_android), weak_platform_view,
      std::move(spawned_message_handler), std::move(vsync_waiter),
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
  if (!embedder_api_.RunsAOTCompiledDartCode() &&
      !fml::FileMapping::CreateReadOnly(
          GetSettings().application_kernel_asset)) {
    FML_DLOG(ERROR) << "Unable to load the kernel blob asset.";
    return;
  }

  UpdateDisplayMetrics();

  ScopedRunConfiguration run_config(GetSettings(), apk_asset_provider_.get(),
                                    entrypoint, libraryUrl, entrypoint_args,
                                    engine_id);

  FlutterEngineResult result =
      embedder_api_.LaunchEngine(engine_.get(), &run_config.config);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineLaunchEngine failed: " << result;
  }
}

AndroidShellHolder::ScreenshotData AndroidShellHolder::Screenshot(
    FlutterScreenshotType type,
    bool base64_encode) {
  if (!IsValid()) {
    return {};
  }
  FlutterScreenshotRequest request = {
      .struct_size = sizeof(FlutterScreenshotRequest),
      .type = type,
      .base64_encode = base64_encode,
  };
  FlutterScreenshot c_screenshot = {
      .struct_size = sizeof(FlutterScreenshot),
  };
  FlutterEngineResult result =
      embedder_api_.Screenshot(engine_.get(), &request, &c_screenshot);
  if (result != kSuccess) {
    return {};
  }
  fml::MallocMapping data =
      c_screenshot.data != nullptr
          ? fml::MallocMapping::Copy(c_screenshot.data, c_screenshot.data_size)
          : fml::MallocMapping();
  ScreenshotData screenshot = {
      .data = std::move(data),
      .width = c_screenshot.frame_width,
      .height = c_screenshot.frame_height,
      .format =
          c_screenshot.frame_type != nullptr ? c_screenshot.frame_type : "",
      .pixel_format = c_screenshot.format,
  };
  embedder_api_.FreeScreenshot(&c_screenshot);
  return screenshot;
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

namespace {

// Maps a Platform -> Dart response handle created via
// `FlutterPlatformMessageCreateResponseHandle` back to `(jni_facade,
// response_id)`.
//
// `FlutterEngineSendPlatformMessage` clones the `PlatformMessageResponse`
// ref-ptr out of the handle, and the embedder immediately releases the handle
// via `FlutterPlatformMessageReleaseResponseHandle`. When Dart later invokes
// the response callback (or drops it without replying), the callback runs on
// the platform thread. If Dart drops the callback without replying, the
// entry in `PendingPlatformMessageResponseTable` is never invoked; we use a
// monotonic token and bound the table or clean up on callback invocation.
struct PendingPlatformMessageResponse {
  std::weak_ptr<PlatformViewAndroidJNI> jni_facade;
  int response_id = 0;
};

class PendingPlatformMessageResponseTable {
 public:
  static PendingPlatformMessageResponseTable& Instance() {
    static PendingPlatformMessageResponseTable* instance =
        new PendingPlatformMessageResponseTable();
    return *instance;
  }

  void* Insert(const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
               int response_id) {
    std::lock_guard lock(mutex_);
    const uintptr_t token = next_token_++;
    entries_[token] = PendingPlatformMessageResponse{jni_facade, response_id};
    return reinterpret_cast<void*>(token);
  }

  std::optional<PendingPlatformMessageResponse> Take(void* user_data) {
    std::lock_guard lock(mutex_);
    const auto token = reinterpret_cast<uintptr_t>(user_data);
    auto it = entries_.find(token);
    if (it == entries_.end()) {
      return std::nullopt;
    }
    PendingPlatformMessageResponse entry = std::move(it->second);
    entries_.erase(it);
    return entry;
  }

  static void OnDataCallback(const uint8_t* data,
                             size_t size,
                             void* user_data) {
    auto entry = Instance().Take(user_data);
    if (!entry.has_value()) {
      return;
    }
    auto jni = entry->jni_facade.lock();
    if (!jni) {
      return;
    }
    if (data == nullptr) {
      jni->FlutterViewHandlePlatformMessageResponse(entry->response_id,
                                                    nullptr);
    } else {
      jni->FlutterViewHandlePlatformMessageResponse(
          entry->response_id, std::make_unique<fml::MallocMapping>(
                                  fml::MallocMapping::Copy(data, size)));
    }
  }

 private:
  std::mutex mutex_;
  uintptr_t next_token_ = 1;
  std::unordered_map<uintptr_t, PendingPlatformMessageResponse> entries_;
};

// Number of int64_t fields in each serialized PointerData record sent by
// `AndroidTouchProcessor.java` (must match `PointerData::kBytesPerField` and
// `kPointerDataFieldCount` in `flutter/lib/ui/window/pointer_data.h`).
constexpr size_t kAndroidPointerDataFieldCount = 36;
constexpr size_t kAndroidPointerDataBytesPerField = sizeof(int64_t);
constexpr size_t kAndroidPointerDataStride =
    kAndroidPointerDataFieldCount * kAndroidPointerDataBytesPerField;

// Layout matches `flutter::PointerData` in
// `flutter/lib/ui/window/pointer_data.h`.
struct RawPointerDataRecord {
  int64_t embedder_id;
  int64_t time_stamp;
  int64_t change;
  int64_t kind;
  int64_t signal_kind;
  int64_t device;
  int64_t pointer_identifier;
  double physical_x;
  double physical_y;
  double physical_delta_x;
  double physical_delta_y;
  int64_t buttons;
  int64_t obscured;
  int64_t synthesized;
  double pressure;
  double pressure_min;
  double pressure_max;
  double distance;
  double distance_max;
  double size;
  double radius_major;
  double radius_minor;
  double radius_min;
  double radius_max;
  double orientation;
  double tilt;
  int64_t platform_data;
  double scroll_delta_x;
  double scroll_delta_y;
  double pan_x;
  double pan_y;
  double pan_delta_x;
  double pan_delta_y;
  double scale;
  double rotation;
  int64_t view_id;
};
static_assert(sizeof(RawPointerDataRecord) == kAndroidPointerDataStride,
              "RawPointerDataRecord must match PointerData binary layout.");

bool ToFlutterPointerPhase(int64_t change, FlutterPointerPhase* phase_out) {
  // Values match `PointerData::Change` in `pointer_data.h`:
  // 0=kCancel, 1=kAdd, 2=kRemove, 3=kHover, 4=kDown, 5=kMove, 6=kUp,
  // 7=kPanZoomStart, 8=kPanZoomUpdate, 9=kPanZoomEnd.
  switch (change) {
    case 0:
      *phase_out = FlutterPointerPhase::kCancel;
      return true;
    case 1:
      *phase_out = FlutterPointerPhase::kAdd;
      return true;
    case 2:
      *phase_out = FlutterPointerPhase::kRemove;
      return true;
    case 3:
      *phase_out = FlutterPointerPhase::kHover;
      return true;
    case 4:
      *phase_out = FlutterPointerPhase::kDown;
      return true;
    case 5:
      *phase_out = FlutterPointerPhase::kMove;
      return true;
    case 6:
      *phase_out = FlutterPointerPhase::kUp;
      return true;
    case 7:
      *phase_out = FlutterPointerPhase::kPanZoomStart;
      return true;
    case 8:
      *phase_out = FlutterPointerPhase::kPanZoomUpdate;
      return true;
    case 9:
      *phase_out = FlutterPointerPhase::kPanZoomEnd;
      return true;
    default:
      return false;
  }
}

FlutterPointerDeviceKind ToFlutterPointerDeviceKind(int64_t kind) {
  // Values match `PointerData::DeviceKind` in `pointer_data.h`:
  // 0=kTouch, 1=kMouse, 2=kStylus, 3=kInvertedStylus, 4=kTrackpad.
  switch (kind) {
    case 0:
      return kFlutterPointerDeviceKindTouch;
    case 1:
      return kFlutterPointerDeviceKindMouse;
    case 2:
      return kFlutterPointerDeviceKindStylus;
    case 3:
      return kFlutterPointerDeviceKindInvertedStylus;
    case 4:
      return kFlutterPointerDeviceKindTrackpad;
    default:
      return kFlutterPointerDeviceKindTouch;
  }
}

FlutterPointerSignalKind ToFlutterPointerSignalKind(int64_t signal_kind) {
  // Values match `PointerData::SignalKind` in `pointer_data.h`:
  // 0=kNone, 1=kScroll, 2=kScrollInertiaCancel, 3=kScale.
  switch (signal_kind) {
    case 0:
      return kFlutterPointerSignalKindNone;
    case 1:
      return kFlutterPointerSignalKindScroll;
    case 2:
      return kFlutterPointerSignalKindScrollInertiaCancel;
    case 3:
      return kFlutterPointerSignalKindScale;
    default:
      return kFlutterPointerSignalKindNone;
  }
}

}  // namespace

void AndroidShellHolder::SetViewportMetrics(
    int64_t view_id,
    const AndroidViewportMetrics& metrics) {
  // Match the validation in `PlatformView::SetViewportMetrics` before calling
  // `SendWindowMetricsEvent`: invalid device pixel ratio or inverted
  // width/height constraints must be rejected early without dispatching.
  if (metrics.device_pixel_ratio <= 0.0 ||
      metrics.physical_min_width_constraint >
          metrics.physical_max_width_constraint ||
      metrics.physical_min_height_constraint >
          metrics.physical_max_height_constraint ||
      metrics.physical_min_width_constraint < 0.0 ||
      metrics.physical_max_width_constraint < 0.0 ||
      metrics.physical_min_height_constraint < 0.0 ||
      metrics.physical_max_height_constraint < 0.0) {
    FML_DLOG(ERROR)
        << "Invalid viewport metrics: device_pixel_ratio must be > 0 and "
           "width/height constraints must be non-negative and ordered.";
    return;
  }

  const size_t feature_count =
      std::min({metrics.physical_display_features_type.size(),
                metrics.physical_display_features_state.size(),
                metrics.physical_display_features_bounds.size() / 4});
  std::vector<FlutterDisplayFeature> features;
  std::vector<const FlutterDisplayFeature*> feature_ptrs;
  features.reserve(feature_count);
  feature_ptrs.reserve(feature_count);
  for (size_t i = 0; i < feature_count; ++i) {
    FlutterDisplayFeature feature = {};
    feature.struct_size = sizeof(FlutterDisplayFeature);
    feature.bounds.left = metrics.physical_display_features_bounds[i * 4 + 0];
    feature.bounds.top = metrics.physical_display_features_bounds[i * 4 + 1];
    feature.bounds.right = metrics.physical_display_features_bounds[i * 4 + 2];
    feature.bounds.bottom = metrics.physical_display_features_bounds[i * 4 + 3];
    feature.type = static_cast<FlutterDisplayFeatureType>(
        metrics.physical_display_features_type[i]);
    feature.state = static_cast<FlutterDisplayFeatureState>(
        metrics.physical_display_features_state[i]);
    features.push_back(feature);
  }
  for (const auto& feature : features) {
    feature_ptrs.push_back(&feature);
  }

  FlutterDisplayCornerRadii corner_radii = {};
  corner_radii.struct_size = sizeof(FlutterDisplayCornerRadii);
  corner_radii.top_left = metrics.top_left_corner_radius;
  corner_radii.top_right = metrics.top_right_corner_radius;
  corner_radii.bottom_right = metrics.bottom_right_corner_radius;
  corner_radii.bottom_left = metrics.bottom_left_corner_radius;
  const bool has_corner_radii = metrics.top_left_corner_radius >= 0.0 &&
                                metrics.top_right_corner_radius >= 0.0 &&
                                metrics.bottom_right_corner_radius >= 0.0 &&
                                metrics.bottom_left_corner_radius >= 0.0;

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.width = static_cast<size_t>(std::max(0.0, metrics.physical_width));
  event.height = static_cast<size_t>(std::max(0.0, metrics.physical_height));
  event.pixel_ratio = metrics.device_pixel_ratio;
  event.left = 0;
  event.top = 0;
  event.physical_view_inset_top = metrics.physical_view_inset_top;
  event.physical_view_inset_right = metrics.physical_view_inset_right;
  event.physical_view_inset_bottom = metrics.physical_view_inset_bottom;
  event.physical_view_inset_left = metrics.physical_view_inset_left;
  event.display_id = metrics.display_id;
  event.view_id = view_id;
  event.has_constraints =
      (metrics.physical_min_width_constraint !=
           metrics.physical_max_width_constraint ||
       metrics.physical_min_height_constraint !=
           metrics.physical_max_height_constraint ||
       metrics.physical_min_width_constraint != metrics.physical_width ||
       metrics.physical_min_height_constraint != metrics.physical_height);
  event.min_width_constraint =
      static_cast<size_t>(std::max(0.0, metrics.physical_min_width_constraint));
  event.min_height_constraint = static_cast<size_t>(
      std::max(0.0, metrics.physical_min_height_constraint));
  event.max_width_constraint =
      static_cast<size_t>(std::max(0.0, metrics.physical_max_width_constraint));
  event.max_height_constraint = static_cast<size_t>(
      std::max(0.0, metrics.physical_max_height_constraint));
  event.physical_padding.top = metrics.physical_padding_top;
  event.physical_padding.right = metrics.physical_padding_right;
  event.physical_padding.bottom = metrics.physical_padding_bottom;
  event.physical_padding.left = metrics.physical_padding_left;
  event.physical_system_gesture_insets.top =
      metrics.physical_system_gesture_inset_top;
  event.physical_system_gesture_insets.right =
      metrics.physical_system_gesture_inset_right;
  event.physical_system_gesture_insets.bottom =
      metrics.physical_system_gesture_inset_bottom;
  event.physical_system_gesture_insets.left =
      metrics.physical_system_gesture_inset_left;
  event.physical_touch_slop = metrics.physical_touch_slop;
  event.display_features_count = feature_ptrs.size();
  event.display_features = feature_ptrs.empty() ? nullptr : feature_ptrs.data();
  event.display_corner_radii = has_corner_radii ? &corner_radii : nullptr;

  FlutterEngineResult result =
      embedder_api_.SendWindowMetricsEvent(engine_.get(), &event);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineSendWindowMetricsEvent failed: " << result;
  }
}

void AndroidShellHolder::DispatchPlatformMessage(const std::string& name,
                                                 const void* message_data,
                                                 int message_position,
                                                 int response_id) {
  if (message_position < 0) {
    return;
  }
  FlutterPlatformMessageResponseHandle* response_handle = nullptr;
  void* token = nullptr;
  if (response_id != 0 &&
      embedder_api_.PlatformMessageCreateResponseHandle != nullptr) {
    token = PendingPlatformMessageResponseTable::Instance().Insert(jni_facade_,
                                                                   response_id);
    FlutterEngineResult create_result =
        embedder_api_.PlatformMessageCreateResponseHandle(
            engine_.get(), &PendingPlatformMessageResponseTable::OnDataCallback,
            token, &response_handle);
    if (create_result != kSuccess) {
      PendingPlatformMessageResponseTable::Instance().Take(token);
      response_handle = nullptr;
    }
  }

  // `DispatchPlatformMessage` is called from Java when the message payload is
  // non-null (even if `message_position == 0`). Ensure `message.message` is
  // non-null so `FlutterEngineSendPlatformMessage` preserves the non-null
  // 0-byte `ByteData(0)` distinction vs `DispatchEmptyPlatformMessage`.
  static constexpr uint8_t kEmptyMessageByte = 0;
  const uint8_t* bytes = static_cast<const uint8_t*>(message_data);
  if (bytes == nullptr && message_position == 0) {
    bytes = &kEmptyMessageByte;
  }

  FlutterPlatformMessage message = {};
  message.struct_size = sizeof(FlutterPlatformMessage);
  message.channel = name.c_str();
  message.message = bytes;
  message.message_size = static_cast<size_t>(message_position);
  message.response_handle = response_handle;

  FlutterEngineResult send_result =
      embedder_api_.SendPlatformMessage(engine_.get(), &message);
  if (send_result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineSendPlatformMessage failed: "
                   << send_result;
    if (token != nullptr) {
      PendingPlatformMessageResponseTable::Instance().Take(token);
    }
  }

  if (response_handle != nullptr &&
      embedder_api_.PlatformMessageReleaseResponseHandle != nullptr) {
    embedder_api_.PlatformMessageReleaseResponseHandle(engine_.get(),
                                                       response_handle);
  }
}

void AndroidShellHolder::DispatchEmptyPlatformMessage(const std::string& name,
                                                      int response_id) {
  FlutterPlatformMessageResponseHandle* response_handle = nullptr;
  void* token = nullptr;
  if (response_id != 0 &&
      embedder_api_.PlatformMessageCreateResponseHandle != nullptr) {
    token = PendingPlatformMessageResponseTable::Instance().Insert(jni_facade_,
                                                                   response_id);
    FlutterEngineResult create_result =
        embedder_api_.PlatformMessageCreateResponseHandle(
            engine_.get(), &PendingPlatformMessageResponseTable::OnDataCallback,
            token, &response_handle);
    if (create_result != kSuccess) {
      PendingPlatformMessageResponseTable::Instance().Take(token);
      response_handle = nullptr;
    }
  }

  FlutterPlatformMessage message = {};
  message.struct_size = sizeof(FlutterPlatformMessage);
  message.channel = name.c_str();
  message.message = nullptr;
  message.message_size = 0;
  message.response_handle = response_handle;

  FlutterEngineResult send_result =
      embedder_api_.SendPlatformMessage(engine_.get(), &message);
  if (send_result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineSendPlatformMessage failed: "
                   << send_result;
    if (token != nullptr) {
      PendingPlatformMessageResponseTable::Instance().Take(token);
    }
  }

  if (response_handle != nullptr &&
      embedder_api_.PlatformMessageReleaseResponseHandle != nullptr) {
    embedder_api_.PlatformMessageReleaseResponseHandle(engine_.get(),
                                                       response_handle);
  }
}

void AndroidShellHolder::DispatchPointerDataPacket(const uint8_t* buffer,
                                                   int size_bytes) {
  if (buffer == nullptr || size_bytes <= 0) {
    return;
  }
  const size_t total_bytes = static_cast<size_t>(size_bytes);
  if (total_bytes % kAndroidPointerDataStride != 0) {
    FML_DLOG(ERROR) << "PointerData packet size (" << total_bytes
                    << ") is not a multiple of stride ("
                    << kAndroidPointerDataStride << ").";
    return;
  }
  const size_t count = total_bytes / kAndroidPointerDataStride;
  std::vector<FlutterPointerEvent> events;
  events.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    RawPointerDataRecord raw = {};
    std::memcpy(&raw, buffer + i * kAndroidPointerDataStride,
                sizeof(RawPointerDataRecord));

    FlutterPointerPhase phase = FlutterPointerPhase::kCancel;
    if (!ToFlutterPointerPhase(raw.change, &phase)) {
      continue;
    }

    FlutterPointerEvent event = {};
    event.struct_size = sizeof(FlutterPointerEvent);
    event.phase = phase;
    event.timestamp = static_cast<size_t>(raw.time_stamp);
    event.x = raw.physical_x;
    event.y = raw.physical_y;
    event.device = static_cast<int32_t>(raw.device);
    event.signal_kind = ToFlutterPointerSignalKind(raw.signal_kind);
    event.scroll_delta_x = raw.scroll_delta_x;
    event.scroll_delta_y = raw.scroll_delta_y;
    event.device_kind = ToFlutterPointerDeviceKind(raw.kind);
    event.buttons = raw.buttons;
    event.pan_x = raw.pan_x;
    event.pan_y = raw.pan_y;
    event.scale = raw.scale;
    event.rotation = raw.rotation;
    event.view_id = raw.view_id;
    event.pressure = raw.pressure;
    event.pressure_min = raw.pressure_min;
    event.pressure_max = raw.pressure_max;
    event.distance = raw.distance;
    event.distance_max = raw.distance_max;
    event.size = raw.size;
    event.radius_major = raw.radius_major;
    event.radius_minor = raw.radius_minor;
    event.radius_min = raw.radius_min;
    event.radius_max = raw.radius_max;
    event.orientation = raw.orientation;
    event.tilt = raw.tilt;
    event.platform_data = raw.platform_data;
    event.embedder_id = raw.embedder_id;
    events.push_back(event);
  }

  if (events.empty()) {
    return;
  }

  TRACE_EVENT0("flutter", "AndroidShellHolder::DispatchPointerDataPacket");
  TRACE_FLOW_BEGIN("flutter", "PointerEvent", next_pointer_flow_id_);
  next_pointer_flow_id_++;

  FlutterEngineResult result = embedder_api_.SendPointerEvent(
      engine_.get(), events.data(), events.size());
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineSendPointerEvent failed: " << result;
  }
}

void AndroidShellHolder::InvokePlatformMessageResponseCallback(
    int response_id,
    const uint8_t* data,
    size_t size) {
  if (platform_message_handler_) {
    platform_message_handler_->InvokePlatformMessageResponseCallback(
        response_id, data, size);
  }
}

void AndroidShellHolder::InvokePlatformMessageEmptyResponseCallback(
    int response_id) {
  if (platform_message_handler_) {
    platform_message_handler_->InvokePlatformMessageEmptyResponseCallback(
        response_id);
  }
}

void AndroidShellHolder::DispatchSemanticsAction(int32_t node_id,
                                                 int32_t action,
                                                 const uint8_t* args_data,
                                                 int32_t args_length) {
  // The Android embedder currently targets the implicit view (ID 0).
  constexpr FlutterViewId kImplicitViewId = 0;
  FlutterSendSemanticsActionInfo info = {};
  info.struct_size = sizeof(FlutterSendSemanticsActionInfo);
  info.node_id = node_id;
  info.action = static_cast<FlutterSemanticsAction>(action);
  info.data = (args_data != nullptr && args_length > 0) ? args_data : nullptr;
  info.data_length = (args_data != nullptr && args_length > 0)
                         ? static_cast<size_t>(args_length)
                         : 0;
  info.view_id = kImplicitViewId;
  FlutterEngineResult result =
      embedder_api_.SendSemanticsAction(engine_.get(), &info);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineSendSemanticsAction failed: " << result;
  }
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
  if (!platform_view_) {
    return;
  }
  std::unique_ptr<Texture> texture =
      platform_view_->CreateSurfaceTexture(texture_id, surface_texture);
  if (!texture) {
    return;
  }
  GetPendingTextureTable().Put(holder_id_, texture_id, std::move(texture));
  FlutterEngineResult result =
      embedder_api_.RegisterExternalTexture(engine_.get(), texture_id);
  GetPendingTextureTable().Take(holder_id_, texture_id);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineRegisterExternalTexture failed: " << result;
  }
}

void AndroidShellHolder::RegisterImageTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    ImageExternalTexture::ImageLifecycle lifecycle) {
  if (!platform_view_) {
    return;
  }
  std::unique_ptr<Texture> texture = platform_view_->CreateImageTexture(
      texture_id, image_texture_entry, lifecycle);
  if (!texture) {
    return;
  }
  GetPendingTextureTable().Put(holder_id_, texture_id, std::move(texture));
  FlutterEngineResult result =
      embedder_api_.RegisterExternalTexture(engine_.get(), texture_id);
  GetPendingTextureTable().Take(holder_id_, texture_id);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineRegisterExternalTexture failed: " << result;
  }
}

void AndroidShellHolder::UnregisterTexture(int64_t texture_id) {
  FlutterEngineResult result =
      embedder_api_.UnregisterExternalTexture(engine_.get(), texture_id);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineUnregisterExternalTexture failed: "
                   << result;
  }
}

void AndroidShellHolder::MarkTextureFrameAvailable(int64_t texture_id) {
  FlutterEngineResult result = embedder_api_.MarkExternalTextureFrameAvailable(
      engine_.get(), texture_id);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineMarkExternalTextureFrameAvailable failed: "
                   << result;
  }
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
  if (!snapshot_data || !snapshot_instructions) {
    return;
  }
  struct DeferredLibraryMappings {
    std::unique_ptr<const fml::Mapping> data;
    std::unique_ptr<const fml::Mapping> instructions;
  };
  const uint8_t* data_ptr = snapshot_data->GetMapping();
  const size_t data_size = snapshot_data->GetSize();
  const uint8_t* instr_ptr = snapshot_instructions->GetMapping();
  const size_t instr_size = snapshot_instructions->GetSize();
  auto* mappings = new DeferredLibraryMappings{
      std::move(snapshot_data), std::move(snapshot_instructions)};

  FlutterDartDeferredLibrary library = {};
  library.struct_size = sizeof(FlutterDartDeferredLibrary);
  library.loading_unit_id = loading_unit_id;
  library.snapshot_data = data_ptr;
  library.snapshot_data_size = data_size;
  library.snapshot_instructions = instr_ptr;
  library.snapshot_instructions_size = instr_size;
  library.user_data = mappings;
  library.destruction_callback = [](void* user_data) {
    delete static_cast<DeferredLibraryMappings*>(user_data);
  };

  FlutterEngineResult result =
      embedder_api_.LoadDartDeferredLibrary(engine_.get(), &library);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineLoadDartDeferredLibrary failed: " << result;
    delete mappings;
  }
}

void AndroidShellHolder::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string& error_message,
    bool transient) {
  FlutterDartDeferredLibraryLoadError error = {};
  error.struct_size = sizeof(FlutterDartDeferredLibraryLoadError);
  error.loading_unit_id = loading_unit_id;
  error.error_message = error_message.c_str();
  error.transient = transient;

  FlutterEngineResult result =
      embedder_api_.NotifyDartDeferredLibraryLoadError(engine_.get(), &error);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineNotifyDartDeferredLibraryLoadError failed: "
                   << result;
  }
}

void AndroidShellHolder::UpdateJavaAssetManager(
    std::unique_ptr<APKAssetProvider> asset_provider) {
  if (!asset_provider) {
    return;
  }
  FlutterCustomAssetResolver resolver =
      asset_provider->ToFlutterCustomAssetResolver();
  FlutterEngineResult result =
      embedder_api_.UpdateAssetResolver(engine_.get(), &resolver);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineUpdateAssetResolver failed: " << result;
  }
}

void AndroidShellHolder::NotifyLowMemoryWarning() {
  FlutterEngineResult result =
      embedder_api_.NotifyLowMemoryWarning(engine_.get());
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineNotifyLowMemoryWarning failed: " << result;
  }
}

FlutterEngineProcTable& AndroidShellHolder::GetEmbedderApiForTesting() {
  return embedder_api_;
}

void AndroidShellHolder::UpdateDisplayMetrics() {
  // FlutterJNI initializes displayWidth, displayHeight, and displayDensity to
  // -1.0f before Java reports display metrics; clamp negative values to 0.0 so
  // casting to size_t never wraps around.
  constexpr FlutterEngineDisplayId kImplicitDisplayId = 0;
  constexpr size_t kSingleDisplayCount = 1;
  FlutterEngineDisplay display = {};
  display.struct_size = sizeof(FlutterEngineDisplay);
  display.display_id = kImplicitDisplayId;
  display.single_display = true;
  display.refresh_rate = std::max(0.0, jni_facade_->GetDisplayRefreshRate());
  display.width =
      static_cast<size_t>(std::max(0.0, jni_facade_->GetDisplayWidth()));
  display.height =
      static_cast<size_t>(std::max(0.0, jni_facade_->GetDisplayHeight()));
  display.device_pixel_ratio = std::max(0.0, jni_facade_->GetDisplayDensity());
  FlutterEngineResult result = embedder_api_.NotifyDisplayUpdate(
      engine_.get(), kFlutterEngineDisplaysUpdateTypeStartup, &display,
      kSingleDisplayCount);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineNotifyDisplayUpdate failed: " << result;
  }
}

bool AndroidShellHolder::IsSurfaceControlEnabled() {
  return GetPlatformView()->IsSurfaceControlEnabled();
}

}  // namespace flutter
