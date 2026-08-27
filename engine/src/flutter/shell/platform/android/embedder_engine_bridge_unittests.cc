// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/embedder_engine_bridge.h"

#include <memory>

#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(EmbedderEngineBridgeTest, ConstructorInitializesComponents) {
  auto jni_mock = std::make_shared<JNIMock>();
  Settings settings;
  settings.enable_software_rendering = true;

  EmbedderEngineBridge bridge(settings, jni_mock,
                              AndroidRenderingAPI::kSoftware);

  EXPECT_TRUE(bridge.IsValid());
  EXPECT_NE(bridge.GetPlatformViewAndroid(), nullptr);
  EXPECT_NE(bridge.GetEmbedderSurfaceAndroid(), nullptr);
  EXPECT_NE(bridge.GetAndroidCompositor(), nullptr);
  EXPECT_NE(bridge.GetAndroidSurfaceManager(), nullptr);
  EXPECT_EQ(bridge.GetEngineHandleForTesting(), nullptr);
  EXPECT_FALSE(bridge.IsSurfaceControlEnabled());
}

TEST(EmbedderEngineBridgeTest, AccessorsReturnValidReferences) {
  auto jni_mock = std::make_shared<JNIMock>();
  Settings settings;
  settings.advisory_script_uri = "test_script";

  EmbedderEngineBridge bridge(settings, jni_mock,
                              AndroidRenderingAPI::kImpellerOpenGLES);

  EXPECT_EQ(bridge.GetSettings().advisory_script_uri, "test_script");
  EXPECT_TRUE(bridge.GetPlatformView());
  EXPECT_NE(bridge.GetPlatformMessageHandler(), nullptr);
}

TEST(EmbedderEngineBridgeTest, ScreenshotReturnsEmptyWhenNotRunning) {
  auto jni_mock = std::make_shared<JNIMock>();
  Settings settings;

  EmbedderEngineBridge bridge(settings, jni_mock,
                              AndroidRenderingAPI::kImpellerOpenGLES);

  auto screenshot =
      bridge.Screenshot(Rasterizer::ScreenshotType::SkiaPicture, false);
  EXPECT_EQ(screenshot.data, nullptr);
}

TEST(EmbedderEngineBridgeTest, NotifyLowMemoryGracefulWhenNotRunning) {
  auto jni_mock = std::make_shared<JNIMock>();
  Settings settings;

  EmbedderEngineBridge bridge(settings, jni_mock,
                              AndroidRenderingAPI::kImpellerOpenGLES);

  // Should not crash even when engine_ is null.
  bridge.NotifyLowMemoryWarning();
}

TEST(EmbedderEngineBridgeTest, UpdateDisplayMetricsGracefulWhenNotRunning) {
  auto jni_mock = std::make_shared<JNIMock>();
  Settings settings;

  EmbedderEngineBridge bridge(settings, jni_mock,
                              AndroidRenderingAPI::kImpellerOpenGLES);

  // Should not crash even when engine_ is null.
  bridge.UpdateDisplayMetrics();
}

}  // namespace testing
}  // namespace flutter
