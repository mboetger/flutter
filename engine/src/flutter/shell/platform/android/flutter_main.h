// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_FLUTTER_MAIN_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_FLUTTER_MAIN_H_

#include <cstdint>
#include <string>
#include <vector>

#include "flutter/fml/build_config.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/task_runner.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"

#if FML_OS_ANDROID
#include <jni.h>
#else
typedef void* jobject;
typedef void* JNIEnv;
typedef void* jclass;
typedef void* jmethodID;
typedef void* jstring;
typedef void* jobjectArray;
typedef int32_t jint;
typedef int64_t jlong;
#endif

namespace flutter {

struct AndroidSettings {
  bool enable_embedder_api = true;
  bool enable_software_rendering = false;
  bool enable_impeller = true;
  bool trace_systrace = false;
  std::string requested_rendering_backend;
  std::string application_kernel_asset;
  std::vector<std::string> command_line_args;
};

class FlutterMain {
 public:
  ~FlutterMain();

  static bool Register(JNIEnv* env);

  static FlutterMain& Get();

  const AndroidSettings& GetSettings() const;
  flutter::AndroidRenderingAPI GetAndroidRenderingAPI();

  static AndroidRenderingAPI SelectedRenderingAPI(
      const AndroidSettings& settings,
      int api_level);

  // Returns true if the Android Embedder C-API architecture is enabled.
  static bool IsEmbedderAPIEnabled();

  // Test overrides for matrix testing.
  static void SetEmbedderAPIEnabledForTesting(bool enabled);
  static void ResetEmbedderAPIEnabledForTesting();

  // Test helpers to configure settings without JNI initialization.
  static void SetSettingsForTesting(const AndroidSettings& settings);
  static void ResetSettingsForTesting();

 private:
  const AndroidSettings settings_;
  const flutter::AndroidRenderingAPI android_rendering_api_;
  fml::RefPtr<fml::TaskRunner> platform_task_runner_;
  intptr_t vm_service_uri_callback_ = 0;

  explicit FlutterMain(const AndroidSettings& settings,
                       flutter::AndroidRenderingAPI android_rendering_api);

  static void Init(JNIEnv* env,
                   jclass clazz,
                   jobject context,
                   jobjectArray jargs,
                   jstring kernelPath,
                   jstring appStoragePath,
                   jstring engineCachesPath,
                   jlong initTimeMillis,
                   jint api_level);

  void SetupDartVMServiceUriCallback(JNIEnv* env);

  FML_DISALLOW_COPY_AND_ASSIGN(FlutterMain);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_FLUTTER_MAIN_H_
