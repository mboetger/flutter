// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "shell/platform/android/flutter_main.h"
#include "third_party/googletest/googlemock/include/gmock/gmock-nice-strict.h"

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

}  // namespace testing
}  // namespace flutter
