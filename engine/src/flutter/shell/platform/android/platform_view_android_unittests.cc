// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "shell/platform/android/flutter_main.h"
#include "shell/platform/embedder/embedder.h"
#include "third_party/googletest/googlemock/include/gmock/gmock-nice-strict.h"

namespace flutter {
namespace testing {

// TODO(matanlurey): Re-enable.
//
// This test (and the entire suite) was skipped on CI (see
// https://github.com/flutter/flutter/issues/163742) and has since bit rotted
// (we fallback to OpenGLES on emulators for performance reasons); either fix
// the test, or remove it.
#if !SLIMPELLER
TEST(AndroidPlatformView, DISABLED_SelectsVulkanBasedOnApiLevel) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerVulkan);
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 24),
            AndroidRenderingAPI::kImpellerOpenGLES);
}
#endif  // !SLIMPELLER

TEST(FlutterMainTest, EmbedderAPIEnabledTestingOverrides) {
  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  FlutterMain::ResetSettingsForTesting();
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());
}

TEST(FlutterMainTest, EmbedderAPIEnabledSettingsFallback) {
  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  FlutterMain::ResetSettingsForTesting();
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  Settings settings_disabled;
  settings_disabled.enable_embedder_api = false;
  FlutterMain::SetSettingsForTesting(settings_disabled);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  Settings settings_enabled;
  settings_enabled.enable_embedder_api = true;
  FlutterMain::SetSettingsForTesting(settings_enabled);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  // Test override takes precedence over settings
  FlutterMain::SetSettingsForTesting(settings_disabled);
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetSettingsForTesting();
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());
}

TEST(FlutterMainTest, PrefetchDefaultFontManagerRunsSuccessfully) {
  EXPECT_EQ(FlutterEnginePrefetchDefaultFontManager(), kSuccess);
}

TEST(FlutterMainTest, CallbackCacheConfigRoundtrip) {
  EXPECT_EQ(FlutterEngineSetCallbackCachePath("/data/local/tmp"), kSuccess);
  EXPECT_EQ(FlutterEngineLoadCallbackCache(), kSuccess);
}

}  // namespace testing
}  // namespace flutter
