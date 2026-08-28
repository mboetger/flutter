// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_FLUTTER_MAIN_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_FLUTTER_MAIN_H_

#include <jni.h>

#include "flutter/common/settings.h"
#include "flutter/fml/macros.h"
#include "flutter/runtime/dart_service_isolate.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"

namespace flutter {

class FlutterMain {
 public:
  ~FlutterMain();

  static bool Register(JNIEnv* env);

  static FlutterMain& Get();

  const flutter::Settings& GetSettings() const;
  flutter::AndroidRenderingAPI GetAndroidRenderingAPI();

  /// Returns the complete array of command line arguments configured for the
  /// engine instance, suitable for passing to
  /// `FlutterProjectArgs.command_line_argv`.
  ///
  /// The argument vector starts with the synthetic executable name `"flutter"`
  /// at index 0, followed by any shell arguments supplied via JNI / Android
  /// manifest metadata, followed by environment switches
  /// (`GetSwitchesFromEnvironment`).
  static std::vector<std::string> GetCommandLineArgs();

  static AndroidRenderingAPI SelectedRenderingAPI(
      const flutter::Settings& settings,
      int api_level);

  // Returns true if the Android Embedder C-API architecture is enabled.
  static bool IsEmbedderAPIEnabled();
  static bool IsEmbedderAPIEnabled(const flutter::Settings& settings);

  // Test overrides for matrix testing of both legacy and embedder paths.
  static void SetEmbedderAPIEnabledForTesting(bool enabled);
  static void ResetEmbedderAPIEnabledForTesting();

  // Test helpers to configure settings without JNI initialization.
  static void SetSettingsForTesting(const flutter::Settings& settings);
  static void ResetSettingsForTesting();

  // Overrides the command line args for testing purposes.
  static void SetCommandLineArgsForTesting(
      std::optional<std::vector<std::string>> args);

  // Resets any testing overrides for the command line args.
  static void ResetCommandLineArgsForTesting();

 private:
  const flutter::Settings settings_;
  const flutter::AndroidRenderingAPI android_rendering_api_;
  const std::vector<std::string> command_line_args_;
  DartServiceIsolate::CallbackHandle vm_service_uri_callback_ = 0;

  explicit FlutterMain(const flutter::Settings& settings,
                       flutter::AndroidRenderingAPI android_rendering_api,
                       std::vector<std::string> command_line_args);

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
