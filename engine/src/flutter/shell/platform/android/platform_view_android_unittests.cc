// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "shell/platform/android/flutter_main.h"

namespace flutter {
namespace testing {

#if !SLIMPELLER
TEST(FlutterMainSelectedRenderingAPI, SelectsImpellerAutoselectOnApi29Plus) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerAutoselect);
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 34),
            AndroidRenderingAPI::kImpellerAutoselect);
}

TEST(FlutterMainSelectedRenderingAPI, SelectsSkiaOpenGLESOnApiBelow29) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 28),
            AndroidRenderingAPI::kSkiaOpenGLES);
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 24),
            AndroidRenderingAPI::kSkiaOpenGLES);
}

TEST(FlutterMainSelectedRenderingAPI,
     SelectsSkiaOpenGLESWhenImpellerDisabledOnApi29Plus) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = false;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kSkiaOpenGLES);
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 34),
            AndroidRenderingAPI::kSkiaOpenGLES);
}

#ifndef FLUTTER_RELEASE
TEST(FlutterMainSelectedRenderingAPI,
     SelectsExplicitImpellerBackendWhenRequested) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  settings.requested_rendering_backend = "vulkan";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerVulkan);

  settings.requested_rendering_backend = "opengles";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerOpenGLES);
}

TEST(FlutterMainSelectedRenderingAPI,
     SelectsSkiaOpenGLESWhenBackendRequestedWithImpellerDisabled) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = false;

  settings.requested_rendering_backend = "vulkan";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kSkiaOpenGLES);

  settings.requested_rendering_backend = "opengles";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kSkiaOpenGLES);
}

TEST(FlutterMainSelectedRenderingAPI,
     SelectsAutoselectWhenUnrecognizedBackendRequested) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  settings.requested_rendering_backend = "unknown_backend";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerAutoselect);
}
#endif  // !FLUTTER_RELEASE

TEST(FlutterMainSelectedRenderingAPI, SelectsSoftwareRenderingWhenRequested) {
  Settings settings;
  settings.enable_software_rendering = true;
  settings.enable_impeller = false;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kSoftware);
}
#endif  // !SLIMPELLER

}  // namespace testing
}  // namespace flutter
