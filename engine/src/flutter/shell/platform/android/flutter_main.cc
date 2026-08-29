// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <android/log.h>
#include <sys/system_properties.h>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "flutter/fml/command_line.h"
#include "flutter/fml/file.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/native_library.h"
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/paths_android.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

constexpr int kMinimumAndroidApiLevelForImpeller = 29;

extern "C" {
#if FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG
// Used for debugging dart:* sources.
extern const uint8_t kPlatformStrongDill[];
extern const intptr_t kPlatformStrongDillSize;
#endif
}

namespace {

fml::jni::ScopedJavaGlobalRef<jclass>* g_flutter_jni_class = nullptr;

// Workaround for crashes in Vivante GL driver on Android.
//
// See:
//   * https://github.com/flutter/flutter/issues/167850
//   * http://crbug.com/141785
#ifdef FML_OS_ANDROID
bool IsVivante() {
  char product_model[PROP_VALUE_MAX];
  __system_property_get("ro.hardware.egl", product_model);
  return strcmp(product_model, "VIVANTE") == 0;
}

bool CheckATraceIsEnabled() {
  using ATraceIsEnabledProc = bool (*)();
  static ATraceIsEnabledProc a_trace_is_enabled = []() -> ATraceIsEnabledProc {
    static auto android_lib = fml::NativeLibrary::Create("libandroid.so");
    if (!android_lib) {
      return nullptr;
    }
    return reinterpret_cast<ATraceIsEnabledProc>(
        const_cast<uint8_t*>(android_lib->ResolveSymbol("ATrace_isEnabled")));
  }();
  return a_trace_is_enabled ? a_trace_is_enabled() : false;
}
#else
bool IsVivante() {
  return false;
}

bool CheckATraceIsEnabled() {
  return false;
}
#endif  // FML_OS_ANDROID

}  // anonymous namespace

FlutterMain::FlutterMain(const flutter::AndroidSettings& settings,
                         flutter::AndroidRenderingAPI android_rendering_api)
    : settings_(settings), android_rendering_api_(android_rendering_api) {}

FlutterMain::~FlutterMain() {
  if (vm_service_uri_callback_ != 0) {
    FlutterEngineDeregisterVMServiceUriCallback(vm_service_uri_callback_);
    vm_service_uri_callback_ = 0;
  }
}

namespace {
enum class OverrideState : int8_t {
  kNotSet = -1,
  kDisabled = 0,
  kEnabled = 1,
};
}  // namespace

static std::unique_ptr<FlutterMain> g_flutter_main;
static std::atomic<int8_t> s_embedder_api_override_for_testing{
    static_cast<int8_t>(OverrideState::kNotSet)};

bool FlutterMain::IsInitialized() {
  return g_flutter_main != nullptr;
}

FlutterMain& FlutterMain::Get() {
  FML_CHECK(g_flutter_main) << "ensureInitializationComplete must have already "
                               "been called.";
  return *g_flutter_main;
}

const flutter::AndroidSettings& FlutterMain::GetSettings() const {
  return settings_;
}

flutter::AndroidRenderingAPI FlutterMain::GetAndroidRenderingAPI() {
  return android_rendering_api_;
}

bool FlutterMain::IsEmbedderAPIEnabled() {
  int8_t override_val =
      s_embedder_api_override_for_testing.load(std::memory_order_relaxed);
  if (override_val != static_cast<int8_t>(OverrideState::kNotSet)) {
    return override_val == static_cast<int8_t>(OverrideState::kEnabled);
  }
  if (g_flutter_main) {
    return g_flutter_main->GetSettings().enable_embedder_api;
  }
  return flutter::AndroidSettings().enable_embedder_api;
}

void FlutterMain::SetEmbedderAPIEnabledForTesting(bool enabled) {
  s_embedder_api_override_for_testing.store(
      enabled ? static_cast<int8_t>(OverrideState::kEnabled)
              : static_cast<int8_t>(OverrideState::kDisabled),
      std::memory_order_relaxed);
}

void FlutterMain::ResetEmbedderAPIEnabledForTesting() {
  s_embedder_api_override_for_testing.store(
      static_cast<int8_t>(OverrideState::kNotSet), std::memory_order_relaxed);
}

void FlutterMain::SetSettingsForTesting(
    const flutter::AndroidSettings& settings) {
  g_flutter_main.reset(
      new FlutterMain(settings, AndroidRenderingAPI::kSoftware));
}

void FlutterMain::ResetSettingsForTesting() {
  g_flutter_main.reset();
}

static AndroidSettings SettingsFromCommandLine(
    const std::vector<std::string>& args) {
  AndroidSettings settings;
  settings.enable_embedder_api = true;

  for (const auto& arg : args) {
    if (arg.rfind("--", 0) == 0) {
      std::string opt = arg.substr(2);
      std::string key = opt;
      std::string val = "";
      auto eq_pos = opt.find('=');
      if (eq_pos != std::string::npos) {
        key = opt.substr(0, eq_pos);
        val = opt.substr(eq_pos + 1);
      }

      if (key == "enable-embedder-api") {
        settings.enable_embedder_api = (val != "false");
      } else if (key == "enable-software-rendering" ||
                 key == "software-rendering") {
        settings.enable_software_rendering = (val != "false");
      } else if (key == "enable-impeller") {
        settings.enable_impeller = (val != "false");
      } else if (key == "enable-hcpp" ||
                 key == "enable-hcpp-and-surface-control" ||
                 key == "enable-surface-control") {
        settings.enable_surface_control = (val != "false");
      } else if (key == "impeller-backend" ||
                 key == "requested-rendering-backend") {
        settings.requested_rendering_backend = val;
      } else if (key == "snapshot-asset-path" || key == "assets-path") {
        settings.assets_path = val;
      } else if (key == "trace-systrace") {
        settings.trace_systrace = (val != "false");
      }
#if FML_OS_ANDROID
      __android_log_print(ANDROID_LOG_INFO, "FlutterMain",
                          "SettingsFromCommandLine: arg=%s -> key=%s, val=%s, "
                          "enable_surface_control=%d",
                          arg.c_str(), key.c_str(), val.c_str(),
                          settings.enable_surface_control);
#endif
    }
  }
#if FML_OS_ANDROID
  __android_log_print(
      ANDROID_LOG_INFO, "FlutterMain",
      "SettingsFromCommandLine summary: enable_surface_control=%d, backend=%s",
      settings.enable_surface_control,
      settings.requested_rendering_backend.c_str());
#endif
  return settings;
}

void FlutterMain::Init(JNIEnv* env,
                       jclass clazz,
                       jobject context,
                       jobjectArray jargs,
                       jstring kernelPath,
                       jstring appStoragePath,
                       jstring engineCachesPath,
                       jlong initTimeMillis,
                       jint api_level) {
  std::vector<std::string> args;
  args.push_back("flutter");
  for (auto& arg : fml::jni::StringArrayToVector(env, jargs)) {
    args.push_back(std::move(arg));
  }

  auto settings = SettingsFromCommandLine(args);
  settings.command_line_args = args;

  // Turn systracing on if ATrace_isEnabled is true and the user did not already
  // request systracing
  if (!settings.trace_systrace) {
    settings.trace_systrace = CheckATraceIsEnabled();
    if (settings.trace_systrace) {
      __android_log_print(
          ANDROID_LOG_INFO, "Flutter",
          "ATrace was enabled at startup. Flutter and Dart "
          "tracing will be forwarded to systrace and will not show up in "
          "Dart DevTools.");
    }
  }

  AndroidRenderingAPI android_rendering_api =
      SelectedRenderingAPI(settings, api_level);

#if !SLIMPELLER
  switch (android_rendering_api) {
    case AndroidRenderingAPI::kSoftware:
    case AndroidRenderingAPI::kSkiaOpenGLES:
      settings.enable_impeller = false;
      break;
    case AndroidRenderingAPI::kImpellerOpenGLES:
    case AndroidRenderingAPI::kImpellerVulkan:
    case AndroidRenderingAPI::kImpellerAutoselect:
      settings.enable_impeller = true;
      break;
  }
#endif  // !SLIMPELLER

  // Restore the callback cache via Embedder API.
  std::string app_storage_path =
      fml::jni::JavaStringToString(env, appStoragePath);
  FlutterEngineSetCallbackCachePath(app_storage_path.c_str());

  fml::paths::InitializeAndroidCachesPath(
      fml::jni::JavaStringToString(env, engineCachesPath));

  FlutterEngineLoadCallbackCache();

  if (!FlutterEngineRunsAOTCompiledDartCode() && kernelPath) {
    auto application_kernel_path =
        fml::jni::JavaStringToString(env, kernelPath);
    settings.application_kernel_asset = application_kernel_path;
  }

  // Not thread safe. Will be removed when FlutterMain is refactored to no
  // longer be a singleton.
  g_flutter_main.reset(new FlutterMain(settings, android_rendering_api));
  g_flutter_main->SetupDartVMServiceUriCallback(env);
}

void FlutterMain::SetupDartVMServiceUriCallback(JNIEnv* env) {
  if (g_flutter_jni_class == nullptr) {
    g_flutter_jni_class = new fml::jni::ScopedJavaGlobalRef<jclass>(
        env, env->FindClass("io/flutter/embedding/engine/FlutterJNI"));
  }
  if (g_flutter_jni_class->is_null()) {
    return;
  }

  fml::MessageLoop::EnsureInitializedForCurrentThread();
  platform_task_runner_ = fml::MessageLoop::GetCurrent().GetTaskRunner();

  FlutterVMServiceUriCallbackConfig config = {
      .struct_size = sizeof(FlutterVMServiceUriCallbackConfig),
      .callback =
          [](const char* uri, void* user_data) {
            auto* runner = static_cast<fml::TaskRunner*>(user_data);
            std::string uri_str(uri ? uri : "");
            runner->PostTask([uri_str] {
              JNIEnv* env = fml::jni::AttachCurrentThread();
              if (!g_flutter_jni_class || g_flutter_jni_class->is_null()) {
                return;
              }
              jfieldID uri_field =
                  env->GetStaticFieldID(g_flutter_jni_class->obj(),
                                        "vmServiceUri", "Ljava/lang/String;");
              if (uri_field == nullptr) {
                return;
              }
              fml::jni::ScopedJavaLocalRef<jstring> java_uri =
                  fml::jni::StringToJavaString(env, uri_str);
              env->SetStaticObjectField(g_flutter_jni_class->obj(), uri_field,
                                        java_uri.obj());
            });
          },
      .user_data = platform_task_runner_.get(),
  };

  FlutterEngineRegisterVMServiceUriCallback(&config, &vm_service_uri_callback_);
}

static void PrefetchDefaultFontManager(JNIEnv* env, jclass jcaller) {
  FlutterEnginePrefetchDefaultFontManager();
}

bool FlutterMain::Register(JNIEnv* env) {
  static const JNINativeMethod methods[] = {
      {
          .name = "nativeInit",
          .signature = "(Landroid/content/Context;[Ljava/lang/String;Ljava/"
                       "lang/String;Ljava/lang/String;Ljava/lang/String;JI)V",
          .fnPtr = reinterpret_cast<void*>(&Init),
      },
      {
          .name = "nativePrefetchDefaultFontManager",
          .signature = "()V",
          .fnPtr = reinterpret_cast<void*>(&PrefetchDefaultFontManager),
      },
  };

  jclass clazz = env->FindClass("io/flutter/embedding/engine/FlutterJNI");

  if (clazz == nullptr) {
    return false;
  }

  return env->RegisterNatives(clazz, methods, std::size(methods)) == 0;
}

// static
AndroidRenderingAPI FlutterMain::SelectedRenderingAPI(
    const flutter::AndroidSettings& settings,
    int api_level) {
#if !SLIMPELLER
  if (settings.enable_software_rendering) {
    if (settings.enable_impeller) {
      FML_CHECK(!settings.enable_impeller)
          << "Impeller does not support software rendering. Either disable "
             "software rendering or disable impeller.";
    }
    return AndroidRenderingAPI::kSoftware;
  }

  // Debug/Profile only functionality for testing a specific
  // backend configuration.
#ifndef FLUTTER_RELEASE
  if (settings.requested_rendering_backend == "opengles" &&
      settings.enable_impeller) {
    return AndroidRenderingAPI::kImpellerOpenGLES;
  }
  if (settings.requested_rendering_backend == "vulkan" &&
      settings.enable_impeller) {
    return AndroidRenderingAPI::kImpellerVulkan;
  }
#endif

  if (settings.enable_impeller &&
      api_level >= kMinimumAndroidApiLevelForImpeller && !IsVivante()) {
    return AndroidRenderingAPI::kImpellerAutoselect;
  }

  return AndroidRenderingAPI::kSkiaOpenGLES;
#else
  return AndroidRenderingAPI::kImpellerAutoselect;
#endif  // !SLIMPELLER
}

}  // namespace flutter
