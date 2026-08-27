// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <string>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/fml/message_loop.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

struct RenderingAPITestCase {
  int api_level;
  bool enable_impeller;
  bool enable_software_rendering;
  std::string requested_backend;
  AndroidRenderingAPI expected_api;
};

class AndroidRenderingAPISelectionTest
    : public ::testing::TestWithParam<RenderingAPITestCase> {};

TEST_P(AndroidRenderingAPISelectionTest, SelectsExpectedBackend) {
  const auto& param = GetParam();
  Settings settings;
  settings.enable_impeller = param.enable_impeller;
  settings.enable_software_rendering = param.enable_software_rendering;
  settings.requested_rendering_backend = param.requested_backend;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, param.api_level),
            param.expected_api);
}

#if !SLIMPELLER
INSTANTIATE_TEST_SUITE_P(
    BackendSelectionAcrossAPILevels,
    AndroidRenderingAPISelectionTest,
    ::testing::Values(
        // Software rendering overrides everything when impeller is false.
        RenderingAPITestCase{21, false, true, "",
                             AndroidRenderingAPI::kSoftware},
        RenderingAPITestCase{28, false, true, "",
                             AndroidRenderingAPI::kSoftware},
        RenderingAPITestCase{29, false, true, "",
                             AndroidRenderingAPI::kSoftware},
        RenderingAPITestCase{35, false, true, "",
                             AndroidRenderingAPI::kSoftware},

        // Explicit requested backend "opengles" with Impeller.
        RenderingAPITestCase{21, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},
        RenderingAPITestCase{24, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},
        RenderingAPITestCase{28, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},
        RenderingAPITestCase{29, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},
        RenderingAPITestCase{35, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},

        // Explicit requested backend "vulkan" with Impeller.
        RenderingAPITestCase{21, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},
        RenderingAPITestCase{24, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},
        RenderingAPITestCase{28, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},
        RenderingAPITestCase{29, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},
        RenderingAPITestCase{35, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},

        // Impeller enabled, no requested backend:
        // Below API 29 falls back to Skia OpenGLES.
        RenderingAPITestCase{21, true, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{24, true, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{28, true, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        // API 29+ selects Impeller Autoselect.
        RenderingAPITestCase{29, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{30, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{31, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{33, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{34, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{35, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},

        // Impeller disabled falls back to Skia OpenGLES regardless of requested
        // backend.
        RenderingAPITestCase{21, false, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{28, false, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{29, false, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{35, false, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{29, false, false, "vulkan",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{29, false, false, "opengles",
                             AndroidRenderingAPI::kSkiaOpenGLES}));
#else
INSTANTIATE_TEST_SUITE_P(
    BackendSelectionSlimpeller,
    AndroidRenderingAPISelectionTest,
    ::testing::Values(
        RenderingAPITestCase{21, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{29, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{35, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect}));
#endif  // !SLIMPELLER

TEST(FlutterMainTest, EmbedderAPIEnabledOverride) {
  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());
}

TEST(FlutterMainTest, InitForTestingAndGetters) {
  FlutterMain::ResetForTesting();

  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_android_embedder_api = true;

  std::vector<std::string> args = {"flutter", "--enable-android-embedder-api",
                                   "--enable-impeller=true"};
  std::string app_storage = "/data/user/0/io.flutter.test/app_flutter";
  std::string engine_caches = "/data/user/0/io.flutter.test/code_cache";
  std::string kernel = "/data/app/io.flutter.test/kernel_blob.bin";
  int64_t init_time = 1234567890;
  int api_level = 34;

  FlutterMain::InitForTesting(settings, AndroidRenderingAPI::kImpellerVulkan,
                              args, app_storage, engine_caches, kernel,
                              init_time, api_level);

  EXPECT_EQ(FlutterMain::Get().GetAndroidRenderingAPI(),
            AndroidRenderingAPI::kImpellerVulkan);
  EXPECT_EQ(FlutterMain::Get().GetCommandLineArgs().size(), 3U);
  EXPECT_EQ(FlutterMain::Get().GetCommandLineArgs()[0], "flutter");
  EXPECT_EQ(FlutterMain::Get().GetCommandLineArgs()[1],
            "--enable-android-embedder-api");
  EXPECT_EQ(FlutterMain::Get().GetCommandLineArgs()[2],
            "--enable-impeller=true");
  EXPECT_EQ(FlutterMain::Get().GetAppStoragePath(), app_storage);
  EXPECT_EQ(FlutterMain::Get().GetEngineCachesPath(), engine_caches);
  EXPECT_EQ(FlutterMain::Get().GetKernelPath(), kernel);
  EXPECT_EQ(FlutterMain::Get().GetInitTimeMillis(), init_time);
  EXPECT_EQ(FlutterMain::Get().GetApiLevel(), api_level);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetForTesting();
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());
}

TEST(FlutterMainTest, InitForTestingDefaults) {
  FlutterMain::ResetForTesting();

  Settings settings;
  settings.enable_software_rendering = true;

  FlutterMain::InitForTesting(settings);

  EXPECT_EQ(FlutterMain::Get().GetAndroidRenderingAPI(),
            AndroidRenderingAPI::kSoftware);
  EXPECT_TRUE(FlutterMain::Get().GetCommandLineArgs().empty());
  EXPECT_TRUE(FlutterMain::Get().GetAppStoragePath().empty());
  EXPECT_TRUE(FlutterMain::Get().GetEngineCachesPath().empty());
  EXPECT_TRUE(FlutterMain::Get().GetKernelPath().empty());
  EXPECT_EQ(FlutterMain::Get().GetInitTimeMillis(), 0);
  EXPECT_EQ(FlutterMain::Get().GetApiLevel(), 0);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetForTesting();
}

TEST(FlutterMainTest, InitForTestingExplicitlyDisabled) {
  FlutterMain::ResetForTesting();

  Settings settings;
  settings.enable_android_embedder_api = false;

  FlutterMain::InitForTesting(settings);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetForTesting();
}

}  // namespace testing
}  // namespace flutter
