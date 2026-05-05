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

#include <EGL/egl.h>
#include <dlfcn.h>
#include "common/settings.h"
#include "flutter/fml/cpu_affinity.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/runtime/dart_service_isolate.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/android_compositor_opengl.h"
#include "flutter/shell/platform/android/android_compositor_software.h"
#include "flutter/shell/platform/android/android_compositor_vulkan.h"
#include "flutter/shell/platform/android/android_display.h"
#include "flutter/shell/platform/android/android_image_generator.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/android_surface_gl_impeller.h"
#include "flutter/shell/platform/android/android_surface_gl_skia.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/embedder/embedder_engine.h"
#include "flutter/shell/platform/embedder/embedder_external_view_embedder.h"
#include "flutter/shell/platform/embedder/embedder_render_target_skia.h"
#include "flutter/shell/platform/embedder/embedder_thread_host.h"
#include "flutter/shell/platform/embedder/platform_view_embedder.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"

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

  thread_host_ = std::make_shared<ThreadHost>(host_config);

  fml::RefPtr<fml::TaskRunner> raster_runner =
      thread_host_->raster_thread->GetTaskRunner();
  fml::RefPtr<fml::TaskRunner> ui_runner;
  fml::RefPtr<fml::TaskRunner> io_runner =
      thread_host_->io_thread->GetTaskRunner();
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();

  if (settings.merged_platform_ui_thread ==
      Settings::MergedPlatformUIThread::kEnabled) {
    ui_runner = platform_runner;
  } else {
    ui_runner = thread_host_->ui_thread->GetTaskRunner();
  }

  flutter::TaskRunners task_runners(thread_label,     // label
                                    platform_runner,  // platform
                                    raster_runner,    // raster
                                    ui_runner,        // ui
                                    io_runner         // io
  );

  task_runners_ = std::make_unique<TaskRunners>(task_runners);

  // 2. Prepare FlutterProjectArgs.
  FlutterProjectArgs args = {};
  args.struct_size = sizeof(FlutterProjectArgs);
  args.assets_path = settings_.assets_path.c_str();
  args.icu_data_path = settings_.icu_data_path.c_str();

  // We'll fill in more args as we migrate further.
  // For now, we still need to create the PlatformViewAndroid.

  // 3. Create AndroidContext.
  auto context_settings = PlatformViewAndroid::CreateContextSettings(settings_);
  auto android_context = PlatformViewAndroid::CreateAndroidContext(
      task_runners, android_rendering_api_, settings_.enable_opengl_gpu_tracing,
      context_settings);

  fml::WeakPtr<PlatformViewAndroid> weak_platform_view;

  owned_embedder_surface_ = std::make_unique<EmbedderSurfaceAndroid>(
      android_context, settings_.enable_impeller,
      settings_.impeller_enable_lazy_shader_mode);
  embedder_surface_ = owned_embedder_surface_.get();

  platform_view_android_ = std::make_unique<PlatformViewAndroid>(
      nullptr,           // engine handle not yet available
      task_runners,      // task runners
      jni_facade,        // JNI interop
      android_context,   // Android context
      embedder_surface_  // Embedder surface
  );
  FML_LOG(INFO) << "AndroidShellHolder: Created platform_view_android_: "
                << platform_view_android_.get();
  weak_platform_view = platform_view_android_->GetWeakPtr();
  platform_view_ = weak_platform_view;

  FML_DCHECK(platform_view_);
  is_valid_ = platform_view_android_ != nullptr;
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
  if (vm_service_callback_handle_ != 0) {
    DartServiceIsolate::RemoveServerStatusCallback(vm_service_callback_handle_);
  }
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
    const std::string& assets_path,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) {
  apk_asset_provider_ = std::move(apk_asset_provider);

  // Prepare FlutterRendererConfig based on API
  FlutterRendererConfig config = {};
  auto* android_platform_view = platform_view_android_.get();
  auto android_context = android_platform_view->GetAndroidContext();

  if (android_context->RenderingApi() == AndroidRenderingAPI::kSkiaOpenGLES ||
      android_context->RenderingApi() ==
          AndroidRenderingAPI::kImpellerOpenGLES) {
    config.type = kOpenGL;
    config.open_gl.struct_size = sizeof(FlutterOpenGLRendererConfig);
    config.open_gl.make_current = [](void* user_data) -> bool {
      auto* holder = static_cast<AndroidShellHolder*>(user_data);
      auto* surface = holder->GetEmbedderSurfaceAndroid()->GetAndroidSurface();
      if (holder->android_rendering_api_ ==
          AndroidRenderingAPI::kImpellerOpenGLES) {
        auto* gl_surface = static_cast<AndroidSurfaceGLImpeller*>(surface);
        return gl_surface->GLContextMakeCurrent()->GetResult();
      } else {
        auto* gl_surface = static_cast<AndroidSurfaceGLSkia*>(surface);
        return gl_surface->GLContextMakeCurrent()->GetResult();
      }
    };
    config.open_gl.clear_current = [](void* user_data) -> bool {
      auto* holder = static_cast<AndroidShellHolder*>(user_data);
      auto* surface = holder->GetEmbedderSurfaceAndroid()->GetAndroidSurface();
      if (holder->android_rendering_api_ ==
          AndroidRenderingAPI::kImpellerOpenGLES) {
        auto* gl_surface = static_cast<AndroidSurfaceGLImpeller*>(surface);
        return gl_surface->GLContextClearCurrent();
      } else {
        auto* gl_surface = static_cast<AndroidSurfaceGLSkia*>(surface);
        return gl_surface->GLContextClearCurrent();
      }
    };
    config.open_gl.present = [](void* user_data) -> bool {
      auto* holder = static_cast<AndroidShellHolder*>(user_data);
      auto* surface = holder->GetEmbedderSurfaceAndroid()->GetAndroidSurface();
      std::optional<DlIRect> frame_damage = std::nullopt;
      std::optional<DlIRect> buffer_damage = std::nullopt;
      GLPresentInfo present_info = {
          .fbo_id = 0,
          .frame_damage = frame_damage,
          .buffer_damage = buffer_damage,
      };
      if (holder->android_rendering_api_ ==
          AndroidRenderingAPI::kImpellerOpenGLES) {
        auto* gl_surface = static_cast<AndroidSurfaceGLImpeller*>(surface);
        return gl_surface->GLContextPresent(present_info);
      } else {
        auto* gl_surface = static_cast<AndroidSurfaceGLSkia*>(surface);
        return gl_surface->GLContextPresent(present_info);
      }
    };
    config.open_gl.fbo_callback = [](void* user_data) -> unsigned int {
      return 0;
    };
    config.open_gl.make_resource_current = [](void* user_data) -> bool {
      auto* holder = static_cast<AndroidShellHolder*>(user_data);
      auto* surface = holder->GetEmbedderSurfaceAndroid()->GetAndroidSurface();
      if (holder->android_rendering_api_ ==
          AndroidRenderingAPI::kImpellerOpenGLES) {
        auto* gl_surface = static_cast<AndroidSurfaceGLImpeller*>(surface);
        return gl_surface->ResourceContextMakeCurrent();
      } else {
        auto* gl_surface = static_cast<AndroidSurfaceGLSkia*>(surface);
        return gl_surface->ResourceContextMakeCurrent();
      }
    };
    config.open_gl.gl_proc_resolver = [](void* user_data,
                                         const char* name) -> void* {
      void* address = reinterpret_cast<void*>(eglGetProcAddress(name));
      if (address != nullptr) {
        return address;
      }
      static void* gles_handle = []() {
        void* handle = dlopen("libGLESv3.so", RTLD_LAZY);
        if (handle == nullptr) {
          handle = dlopen("libGLESv2.so", RTLD_LAZY);
        }
        return handle;
      }();
      if (gles_handle != nullptr) {
        return dlsym(gles_handle, name);
      }
      return nullptr;
    };
  } else if (android_context->RenderingApi() ==
             AndroidRenderingAPI::kImpellerVulkan) {
    config.type = kVulkan;
    // Fill in Vulkan callbacks if needed
  }

  FML_LOG(INFO) << "Rendering API: "
                << static_cast<int>(android_context->RenderingApi());

  // Prepare FlutterProjectArgs
  FlutterProjectArgs args = {};
  args.struct_size = sizeof(FlutterProjectArgs);
  args.vsync_callback = [](void* user_data, intptr_t baton) {
    auto* holder = static_cast<AndroidShellHolder*>(user_data);
    holder->GetPlatformViewAndroid()->OnVsyncCallback(baton);
  };
  args.assets_path = assets_path.c_str();
  // args.icu_data_path = icu_path.c_str();

  FML_LOG(INFO) << "assets_path: " << args.assets_path;
  FML_LOG(INFO) << "icu_data_path: "
                << (args.icu_data_path ? args.icu_data_path : "NULL");

  AssetResolver* resolver = apk_asset_provider_.get();
  kernel_mapping_ = resolver->GetAsMapping("kernel_blob.bin");
  if (kernel_mapping_) {
    args.application_kernel_data = kernel_mapping_->GetMapping();
    args.application_kernel_data_size = kernel_mapping_->GetSize();
    FML_LOG(INFO) << "Loaded kernel_blob.bin from APK, size: "
                  << args.application_kernel_data_size;
  } else {
    FML_LOG(WARNING) << "Could not load kernel_blob.bin from APK.";
  }

  if (!entrypoint.empty()) {
    args.custom_dart_entrypoint = entrypoint.c_str();
  }

  std::vector<const char*> argv;
  if (!entrypoint_args.empty()) {
    for (const auto& arg : entrypoint_args) {
      argv.push_back(arg.c_str());
    }
    args.dart_entrypoint_argc = argv.size();
    args.dart_entrypoint_argv = argv.data();
  }

  const auto& engine_args = FlutterMain::Get().GetArgs();
  FML_LOG(INFO) << "engine_args count: " << engine_args.size();
  for (const auto& arg : engine_args) {
    FML_LOG(INFO) << "engine_arg: " << arg;
  }
  std::vector<const char*> engine_argv;
  for (const auto& arg : engine_args) {
    if (arg != "--start-paused") {
      engine_argv.push_back(arg.c_str());
    }
  }
  engine_argv.push_back("--enable-vm-service");
  engine_argv.push_back("--disable-service-auth-codes");
  engine_argv.push_back("--verbose-logging");
  args.command_line_argc = engine_argv.size();
  args.command_line_argv = engine_argv.data();
  FML_LOG(INFO) << "engine_argv count: " << args.command_line_argc;
  for (int i = 0; i < args.command_line_argc; ++i) {
    FML_LOG(INFO) << "engine_argv[" << i << "]: " << args.command_line_argv[i];
  }

  // 4. Setup Custom Task Runners.
  args.custom_task_runners = nullptr;

  // Call FlutterEngineRun (convenience method)
  FlutterEngineResult result =
      FlutterEngineRun(FLUTTER_ENGINE_VERSION, &config, &args, this, &engine_);

  if (result != kSuccess) {
    FML_LOG(ERROR) << "Could not run Flutter engine. Result: " << result;
    is_valid_ = false;
    return;
  }

  FML_LOG(INFO)
      << "Launch: FlutterEngineRun completed. platform_view_android_ is "
      << (platform_view_android_ ? "NON-NULL" : "NULL");
  if (platform_view_android_) {
    platform_view_android_->SetEngine(engine_);
    auto* embedder_engine = reinterpret_cast<EmbedderEngine*>(engine_);
    auto platform_view = embedder_engine->GetShell().GetPlatformView();
    platform_view_android_->SetPlatformView(platform_view);
    FML_LOG(INFO)
        << "Synchronously bound PlatformViewAndroid to primary PlatformView!";
  }

  is_valid_ = engine_ != nullptr;
  UpdateDisplayMetrics();
  FML_LOG(INFO) << "UpdateDisplayMetrics called";

  // Setup VM service URI callback to notify Java
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  FML_LOG(INFO) << "EnsureInitializedForCurrentThread called";
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();
  FML_LOG(INFO) << "GetCurrent TaskRunner called";

  JNIEnv* env = fml::jni::AttachCurrentThread();
  FML_LOG(INFO) << "AttachCurrentThread called, env: " << env;

  jclass local_class = env->FindClass("io/flutter/embedding/engine/FlutterJNI");
  auto flutter_jni_class =
      fml::jni::ScopedJavaGlobalRef<jclass>(env, local_class);
  FML_LOG(INFO) << "FindClass FlutterJNI called, class: " << local_class;
  if (!flutter_jni_class.is_null()) {
    jfieldID uri_field = env->GetStaticFieldID(
        flutter_jni_class.obj(), "vmServiceUri", "Ljava/lang/String;");
    if (uri_field != nullptr) {
      auto set_uri = [flutter_jni_class, uri_field](const std::string& uri) {
        FML_LOG(INFO) << "The Dart VM service is listening on " << uri;
        JNIEnv* env = fml::jni::AttachCurrentThread();
        fml::jni::ScopedJavaLocalRef<jstring> java_uri =
            fml::jni::StringToJavaString(env, uri);
        env->SetStaticObjectField(flutter_jni_class.obj(), uri_field,
                                  java_uri.obj());
      };

      vm_service_callback_handle_ = DartServiceIsolate::AddServerStatusCallback(
          [platform_runner, set_uri](const std::string& uri) {
            platform_runner->PostTask([uri, set_uri] { set_uri(uri); });
          });
    }
  }
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
  FlutterEngineNotifyLowMemoryWarning(engine_);
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
  if (!engine_) {
    return;
  }
  if (GetPlatformViewAndroid()->HasViewportMetrics()) {
    FML_LOG(INFO) << "UpdateDisplayMetrics: Skipping hardcoded fallback "
                     "because Java already provided metrics.";
    return;
  }
  FML_LOG(INFO)
      << "UpdateDisplayMetrics: Java provided no metrics (headless/driver "
         "mode). Sending hardcoded fallback: 1280x2856 @ 3.0";
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.width = 1280;
  event.height = 2856;
  event.pixel_ratio = 3.0;

  auto result = FlutterEngineSendWindowMetricsEvent(engine_, &event);
  if (result == kSuccess) {
    FML_LOG(INFO)
        << "Successfully sent hardcoded window metrics: 1280x2856 @ 3.0";
  } else {
    FML_LOG(ERROR) << "Failed to send hardcoded window metrics, result: "
                   << result;
  }
}

bool AndroidShellHolder::IsSurfaceControlEnabled() {
  return GetPlatformViewAndroid()->IsSurfaceControlEnabled();
}

}  // namespace flutter
