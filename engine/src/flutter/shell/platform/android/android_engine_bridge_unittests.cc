// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_engine_bridge.h"

#include <memory>

#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(AndroidEngineBridgeTest, CreateDefaultLegacyBridge) {
  auto jni_mock = std::make_shared<JNIMock>();
  Settings settings;
  settings.enable_embedder_api = false;
  settings.enable_software_rendering = false;

  auto bridge = AndroidEngineBridge::Create(
      settings, jni_mock, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_NE(bridge, nullptr);
  EXPECT_TRUE(bridge->IsValid());
  EXPECT_EQ(bridge->GetSettings().enable_embedder_api, false);
}

TEST(AndroidEngineBridgeTest, CreateEmbedderBridgeWhenFlagEnabled) {
  auto jni_mock = std::make_shared<JNIMock>();
  Settings settings;
  settings.enable_embedder_api = true;
  settings.enable_software_rendering = true;

  auto bridge = AndroidEngineBridge::Create(settings, jni_mock,
                                            AndroidRenderingAPI::kSoftware);
  ASSERT_NE(bridge, nullptr);
  EXPECT_FALSE(bridge->IsValid());
  EXPECT_EQ(bridge->GetSettings().enable_embedder_api, true);
  EXPECT_NE(bridge->GetPlatformViewAndroid(), nullptr);
  EXPECT_NE(bridge->GetEmbedderSurfaceAndroid(), nullptr);
}

TEST(AndroidEngineBridgeTest, ShellForTestingReturnsNullOnBaseClass) {
  auto jni_mock = std::make_shared<JNIMock>();
  Settings settings;
  settings.enable_embedder_api = true;

  auto bridge = AndroidEngineBridge::Create(
      settings, jni_mock, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_NE(bridge, nullptr);
  EXPECT_EQ(bridge->GetShellForTesting(), nullptr);
}

}  // namespace testing
}  // namespace flutter
