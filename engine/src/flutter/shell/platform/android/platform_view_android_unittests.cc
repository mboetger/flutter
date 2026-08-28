// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

#if !SLIMPELLER
TEST(AndroidPlatformView, SelectsRenderingAPIByApiLevel) {
  // Test Android API levels from 21 (minimum supported) up to 35.
  constexpr int kMinSupportedApiLevel = 21;
  constexpr int kImpellerThresholdApiLevel = 29;
  constexpr int kMaxTestedApiLevel = 35;

  // 1. Default Impeller enabled without explicit backend override.
  {
    Settings settings;
    settings.enable_software_rendering = false;
    settings.enable_impeller = true;

    // API levels 21..28 fallback to Skia OpenGLES.
    for (int api = kMinSupportedApiLevel; api < kImpellerThresholdApiLevel;
         ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kSkiaOpenGLES)
          << "Failed for API level " << api;
    }

    // API levels 29..35 autoselect Impeller (Vulkan preferred).
    for (int api = kImpellerThresholdApiLevel; api <= kMaxTestedApiLevel;
         ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kImpellerAutoselect)
          << "Failed for API level " << api;
    }
  }

#ifndef FLUTTER_RELEASE
  // 2. Explicit Impeller OpenGLES requested backend.
  {
    Settings settings;
    settings.enable_software_rendering = false;
    settings.enable_impeller = true;
    settings.requested_rendering_backend = "opengles";

    for (int api = kMinSupportedApiLevel; api <= kMaxTestedApiLevel; ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kImpellerOpenGLES)
          << "Failed for API level " << api;
    }
  }

  // 3. Explicit Impeller Vulkan requested backend.
  {
    Settings settings;
    settings.enable_software_rendering = false;
    settings.enable_impeller = true;
    settings.requested_rendering_backend = "vulkan";

    for (int api = kMinSupportedApiLevel; api <= kMaxTestedApiLevel; ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kImpellerVulkan)
          << "Failed for API level " << api;
    }
  }
#endif  // !FLUTTER_RELEASE

  // 4. Software rendering.
  {
    Settings settings;
    settings.enable_software_rendering = true;
    settings.enable_impeller = false;

    for (int api = kMinSupportedApiLevel; api <= kMaxTestedApiLevel; ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kSoftware)
          << "Failed for API level " << api;
    }
  }

  // 5. Impeller disabled explicitly.
  {
    Settings settings;
    settings.enable_software_rendering = false;
    settings.enable_impeller = false;

    for (int api = kMinSupportedApiLevel; api <= kMaxTestedApiLevel; ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kSkiaOpenGLES)
          << "Failed for API level " << api;
    }
  }
}
#endif  // !SLIMPELLER

namespace {

struct ScopedEmbedderAPIOverrideReset {
  ~ScopedEmbedderAPIOverrideReset() {
    FlutterMain::ResetEmbedderAPIEnabledForTesting();
  }
};

}  // namespace

TEST(PlatformViewAndroidTest, EmbedderAPIFeatureFlagSettings) {
  ScopedEmbedderAPIOverrideReset reset_on_exit;
  FlutterMain::ResetEmbedderAPIEnabledForTesting();

  Settings settings_disabled;
  settings_disabled.enable_android_embedder_api = false;
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled(settings_disabled));

  Settings settings_enabled;
  settings_enabled.enable_android_embedder_api = true;
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled(settings_enabled));
}

TEST(PlatformViewAndroidTest, EmbedderAPIFeatureFlagTestingOverride) {
  ScopedEmbedderAPIOverrideReset reset_on_exit;
  FlutterMain::ResetEmbedderAPIEnabledForTesting();

  Settings settings_disabled;
  settings_disabled.enable_android_embedder_api = false;

  // Override to true.
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled(settings_disabled));

  // Override to false with enabled settings.
  Settings settings_enabled;
  settings_enabled.enable_android_embedder_api = true;
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled(settings_enabled));

  // Passing std::nullopt clears test override.
  FlutterMain::SetEmbedderAPIEnabledForTesting(std::nullopt);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled(settings_disabled));
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled(settings_enabled));

  // Reset override explicitly.
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled(settings_disabled));
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled(settings_enabled));
}

struct ScopedCommandLineArgsOverrideReset {
  ~ScopedCommandLineArgsOverrideReset() {
    FlutterMain::ResetCommandLineArgsForTesting();
  }
};

TEST(PlatformViewAndroidTest, FlutterMainCommandLineArgsTestingOverride) {
  ScopedCommandLineArgsOverrideReset reset_on_exit;
  FlutterMain::ResetCommandLineArgsForTesting();

  // Initial state without override or singleton returns default synthetic
  // executable.
  const std::vector<std::string> default_args = {"flutter"};
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), default_args);

  const std::vector<std::string> test_args = {
      "flutter", "--enable-android-embedder-api", "--enable-impeller=true"};
  FlutterMain::SetCommandLineArgsForTesting(test_args);
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), test_args);

  // Clear with nullopt reverts to default args.
  FlutterMain::SetCommandLineArgsForTesting(std::nullopt);
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), default_args);

  // Set again, then reset explicitly.
  FlutterMain::SetCommandLineArgsForTesting(test_args);
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), test_args);
  FlutterMain::ResetCommandLineArgsForTesting();
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), default_args);
}

}  // namespace testing
}  // namespace flutter
