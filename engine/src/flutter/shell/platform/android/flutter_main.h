// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_FLUTTER_MAIN_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_FLUTTER_MAIN_H_

#include <jni.h>

#include <optional>
#include <string>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/fml/macros.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

class FlutterMain {
 public:
  ~FlutterMain();

  static bool Register(JNIEnv* env);

  static FlutterMain& Get();

  const flutter::Settings& GetSettings() const;
  flutter::AndroidRenderingAPI GetAndroidRenderingAPI() const;

  const std::vector<std::string>& GetCommandLineArgs() const;
  const std::string& GetAppStoragePath() const;
  const std::string& GetEngineCachesPath() const;
  const std::string& GetKernelPath() const;
  int64_t GetInitTimeMillis() const;
  int GetApiLevel() const;

  static AndroidRenderingAPI SelectedRenderingAPI(
      const flutter::Settings& settings,
      int api_level);

  static bool IsEmbedderAPIEnabled();
  static void SetEmbedderAPIEnabledForTesting(std::optional<bool> enabled);
  static void ResetEmbedderAPIEnabledForTesting();

  static void InitForTesting(
      const flutter::Settings& settings,
      flutter::AndroidRenderingAPI api = AndroidRenderingAPI::kSoftware,
      std::vector<std::string> args = {},
      std::string app_storage_path = "",
      std::string engine_caches_path = "",
      std::string kernel_path = "",
      int64_t init_time_millis = 0,
      int api_level = 0);
  static void ResetForTesting();

 private:
  const flutter::Settings settings_;
  const flutter::AndroidRenderingAPI android_rendering_api_;
  const std::vector<std::string> command_line_args_;
  const std::string app_storage_path_;
  const std::string engine_caches_path_;
  const std::string kernel_path_;
  const int64_t init_time_millis_ = 0;
  const int api_level_ = 0;
  intptr_t vm_service_uri_callback_ = 0;

  explicit FlutterMain(const flutter::Settings& settings,
                       flutter::AndroidRenderingAPI android_rendering_api,
                       std::vector<std::string> command_line_args = {},
                       std::string app_storage_path = "",
                       std::string engine_caches_path = "",
                       std::string kernel_path = "",
                       int64_t init_time_millis = 0,
                       int api_level = 0);

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
