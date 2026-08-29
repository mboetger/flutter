// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_engine.h"

#include <memory>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

namespace {

struct ScopedEmbedderAPIOverrideReset {
  ~ScopedEmbedderAPIOverrideReset() {
    FlutterMain::ResetEmbedderAPIEnabledForTesting();
  }
};

}  // namespace

TEST(AndroidEngine, InitializationAndLifecycle) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  EXPECT_TRUE(engine.IsValid());
  EXPECT_FALSE(engine.IsRunning());
  EXPECT_EQ(engine.GetRenderingAPI(), AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(engine.GetSurfaceManager(), nullptr);
  EXPECT_NE(engine.GetCompositor(), nullptr);
  EXPECT_EQ(engine.GetEngineHandle(), nullptr);

  const FlutterEngineProcTable& procs = engine.GetProcTable();
  EXPECT_EQ(procs.struct_size, sizeof(FlutterEngineProcTable));
  EXPECT_NE(procs.Initialize, nullptr);
  EXPECT_NE(procs.RunInitialized, nullptr);
  EXPECT_NE(procs.Deinitialize, nullptr);
  EXPECT_NE(procs.Shutdown, nullptr);
  EXPECT_NE(procs.Spawn, nullptr);
  EXPECT_NE(procs.SendWindowMetricsEvent, nullptr);
  EXPECT_NE(procs.SendPointerEvent, nullptr);
  EXPECT_NE(procs.SendPlatformMessage, nullptr);
  EXPECT_NE(procs.SendPlatformMessageResponse, nullptr);
}

TEST(AndroidEngine, SurfaceLifecycle) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  auto window_a = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  engine.OnSurfaceCreated(window_a);
  EXPECT_FALSE(engine.GetCompositor()->IsSurfaceDestroyed());
  EXPECT_TRUE(engine.GetSurfaceManager()->HasNativeWindow());
  EXPECT_EQ(engine.GetSurfaceManager()->GetNativeWindow(), window_a);

  auto window_b = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  engine.OnSurfaceWindowChanged(window_b);
  EXPECT_FALSE(engine.GetCompositor()->IsSurfaceDestroyed());
  EXPECT_EQ(engine.GetSurfaceManager()->GetNativeWindow(), window_b);

  engine.OnSurfaceDestroyed();
  EXPECT_TRUE(engine.GetCompositor()->IsSurfaceDestroyed());
  EXPECT_FALSE(engine.GetSurfaceManager()->HasNativeWindow());
}

TEST(AndroidEngine, ViewportMetricsAndInputGracefulBeforeLaunch) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  // Calling methods before launch should not crash.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.width = 1080;
  event.height = 1920;
  event.pixel_ratio = 2.0;
  event.physical_view_inset_top = 24.0;
  event.physical_view_inset_bottom = 120.0;
  engine.SetViewportMetrics(event);

  FlutterPointerEvent pointer_event = {};
  pointer_event.struct_size = sizeof(FlutterPointerEvent);
  pointer_event.phase = kDown;
  pointer_event.x = 100.0;
  pointer_event.y = 200.0;
  engine.DispatchPointerEvents(&pointer_event, 1);

  // Sized to exactly one AndroidPointerData record (36 fields * 8 bytes = 288
  // bytes).
  constexpr size_t kPointerDataPacketSize = 288;
  std::vector<uint8_t> dummy_packet(kPointerDataPacketSize, 0);
  engine.DispatchPointerDataPacket(dummy_packet.data(), dummy_packet.size());
  engine.DispatchPointerDataPacket(nullptr, 0);
}

TEST(AndroidEngine, PlatformMessagingGracefulBeforeLaunch) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  const uint8_t data[] = {1, 2, 3, 4};
  constexpr size_t data_size = sizeof(data);
  engine.SendPlatformMessage("test_channel", data, data_size, 42);
  engine.SendPlatformMessageResponse(42, data, data_size);
}

TEST(AndroidEngine, SemanticsAndAccessibilityGracefulBeforeLaunch) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  engine.SetSemanticsEnabled(true);
  engine.SetAccessibilityFeatures(1);
  const uint8_t action_data[] = {0};
  engine.DispatchSemanticsAction(0, 1, kFlutterSemanticsActionTap, action_data,
                                 sizeof(action_data));
}

TEST(AndroidEngine, ExternalTexturesGracefulBeforeLaunch) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  EXPECT_FALSE(engine.RegisterExternalTexture(101));
  EXPECT_FALSE(engine.UnregisterExternalTexture(101));
  EXPECT_FALSE(engine.MarkExternalTextureFrameAvailable(101));
}

TEST(AndroidEngine, DeferredLibrariesGracefulBeforeLaunch) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  const uint8_t snapshot_data[] = {0x00, 0x01};
  EXPECT_FALSE(engine.LoadDartDeferredLibrary(
      1, snapshot_data, sizeof(snapshot_data), nullptr, 0));
  EXPECT_FALSE(engine.LoadDartDeferredLibraryError(1, "Not found", true));
}

TEST(AndroidEngine, ScreenshotAndVsyncGracefulBeforeLaunch) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  FlutterEngineScreenshot screenshot =
      engine.Screenshot(kFlutterEngineScreenshotTypeUncompressedImage, false);
  EXPECT_EQ(screenshot.bytes, nullptr);
  engine.ReleaseScreenshot(&screenshot);

  engine.OnVsync(1234, 1000000, 2000000);
  engine.NotifyLowMemoryWarning();
}

TEST(AndroidEngine, SemanticsSerializationWireProtocolParity) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);
  flags.is_button = true;
  flags.is_focused = kFlutterTristateTrue;

  FlutterTransformation transform = {
      .scaleX = 1.0,
      .skewX = 0.0,
      .transX = 10.0,
      .skewY = 0.0,
      .scaleY = 1.0,
      .transY = 20.0,
      .pers0 = 0.0,
      .pers1 = 0.0,
      .pers2 = 1.0,
  };

  FlutterLocaleStringAttribute locale_attr = {
      .struct_size = sizeof(FlutterLocaleStringAttribute),
      .locale = "en-US",
  };

  FlutterStringAttribute str_attr = {
      .struct_size = sizeof(FlutterStringAttribute),
      .start = 0,
      .end = 5,
      .type = kLocale,
      .locale = &locale_attr,
  };

  const FlutterStringAttribute* label_attrs[] = {&str_attr};

  int32_t traversal_children[] = {1, 2};
  int32_t hit_test_children[] = {2, 1};
  int32_t custom_actions[] = {10};

  FlutterSemanticsNode2 node = {};
  node.struct_size = sizeof(FlutterSemanticsNode2);
  node.id = 0;
  node.flags2 = &flags;
  node.actions = kFlutterSemanticsActionTap;
  node.rect = {0.0, 0.0, 100.0, 50.0};
  node.transform = transform;
  node.hit_test_transform = transform;
  node.child_count = 2;
  node.children_in_traversal_order = traversal_children;
  node.children_in_hit_test_order = hit_test_children;
  node.custom_accessibility_actions_count = 1;
  node.custom_accessibility_actions = custom_actions;
  node.label = "Button";
  node.label_attribute_count = 1;
  node.label_attributes = label_attrs;

  FlutterSemanticsNode2* nodes[] = {&node};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = nodes;

  // Expect JNIMock to receive the serialized semantics buffer and string args.
  EXPECT_CALL(*jni, FlutterViewUpdateSemantics(::testing::_, ::testing::_,
                                               ::testing::_))
      .Times(1);

  engine.HandleSemanticsUpdate2ForTesting(&update);
}

TEST(AndroidEngine, FeatureFlagGatingDualPathValidation) {
  ScopedEmbedderAPIOverrideReset reset_on_exit;
  Settings settings;

  // Path 1: Feature flag enabled.
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled(settings));
  {
    auto jni = std::make_shared<JNIMock>();
    AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
    EXPECT_TRUE(engine.IsValid());
  }

  // Path 2: Feature flag disabled.
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled(settings));
  {
    auto jni = std::make_shared<JNIMock>();
    AndroidEngine engine(settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
    EXPECT_TRUE(engine.IsValid());
  }
}

struct EngineMatrixConfig {
  AndroidRenderingAPI rendering_api;
  bool embedder_api_enabled;
};

class AndroidEngineMatrixTest
    : public ::testing::TestWithParam<EngineMatrixConfig> {
 protected:
  void SetUp() override {
    FlutterMain::SetEmbedderAPIEnabledForTesting(
        GetParam().embedder_api_enabled);
  }

  void TearDown() override { FlutterMain::ResetEmbedderAPIEnabledForTesting(); }
};

TEST_P(AndroidEngineMatrixTest, MatrixLifecycleAndConfiguration) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, GetParam().rendering_api);

  EXPECT_TRUE(engine.IsValid());
  EXPECT_FALSE(engine.IsRunning());
  EXPECT_EQ(engine.GetRenderingAPI(), GetParam().rendering_api);
  EXPECT_NE(engine.GetSurfaceManager(), nullptr);
  EXPECT_NE(engine.GetCompositor(), nullptr);
  EXPECT_EQ(engine.GetEngineHandle(), nullptr);

  const FlutterEngineProcTable& procs = engine.GetProcTable();
  EXPECT_EQ(procs.struct_size, sizeof(FlutterEngineProcTable));
  EXPECT_NE(procs.Initialize, nullptr);
  EXPECT_NE(procs.RunInitialized, nullptr);
  EXPECT_NE(procs.Deinitialize, nullptr);
  EXPECT_NE(procs.Shutdown, nullptr);
  EXPECT_NE(procs.Spawn, nullptr);
}

TEST_P(AndroidEngineMatrixTest, MatrixSurfaceLifecycleTransitions) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, GetParam().rendering_api);

  auto window_a = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  engine.OnSurfaceCreated(window_a);
  EXPECT_FALSE(engine.GetCompositor()->IsSurfaceDestroyed());
  EXPECT_TRUE(engine.GetSurfaceManager()->HasNativeWindow());
  EXPECT_EQ(engine.GetSurfaceManager()->GetNativeWindow(), window_a);

  auto window_b = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  engine.OnSurfaceWindowChanged(window_b);
  EXPECT_FALSE(engine.GetCompositor()->IsSurfaceDestroyed());
  EXPECT_EQ(engine.GetSurfaceManager()->GetNativeWindow(), window_b);

  engine.OnSurfaceDestroyed();
  EXPECT_TRUE(engine.GetCompositor()->IsSurfaceDestroyed());
  EXPECT_FALSE(engine.GetSurfaceManager()->HasNativeWindow());
}

TEST_P(AndroidEngineMatrixTest, MatrixViewportMetricsAndPointerEvents) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, GetParam().rendering_api);

  // Standard viewport metrics configuration for test matrix.
  // Rationale: 1080x1920 portrait resolution with 2.0x display pixel ratio.
  constexpr double kPhysicalWidth = 1080.0;
  constexpr double kPhysicalHeight = 1920.0;
  constexpr double kDevicePixelRatio = 2.0;
  // Rationale: Top status bar (24dp = 48px), bottom navigation bar (48dp =
  // 96px).
  constexpr double kPaddingTop = 48.0;
  constexpr double kPaddingBottom = 96.0;

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.width = static_cast<size_t>(kPhysicalWidth);
  event.height = static_cast<size_t>(kPhysicalHeight);
  event.pixel_ratio = kDevicePixelRatio;
  event.physical_view_inset_top = kPaddingTop;
  event.physical_view_inset_bottom = kPaddingBottom;
  engine.SetViewportMetrics(event);

  // Sized to exactly one AndroidPointerData record (36 fields * 8 bytes = 288
  // bytes).
  constexpr size_t kPointerDataPacketSize = 288;
  std::vector<uint8_t> dummy_packet(kPointerDataPacketSize, 0);
  engine.DispatchPointerDataPacket(dummy_packet.data(), dummy_packet.size());
}

TEST_P(AndroidEngineMatrixTest, MatrixPlatformMessagingAndResponses) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, GetParam().rendering_api);

  // Rationale: Test response ID 100 and arbitrary 4-byte payload.
  constexpr int32_t kResponseId = 100;
  constexpr size_t kDataSize = 4;
  const uint8_t data[kDataSize] = {0xDE, 0xAD, 0xBE, 0xEF};

  engine.SendPlatformMessage("flutter/test_channel", data, kDataSize,
                             kResponseId);
  engine.SendPlatformMessageResponse(kResponseId, data, kDataSize);
}

TEST_P(AndroidEngineMatrixTest, MatrixSemanticsAndAccessibilityTree) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, GetParam().rendering_api);

  // Rationale: Accessibility features bitmask (1 = accessible navigation
  // enabled).
  constexpr int32_t kAccessibilityFeatures = 1;
  // Rationale: Semantics action targeting root node (node ID 0) with action ID
  // 1 (tap).
  constexpr int32_t kNodeId = 0;

  engine.SetSemanticsEnabled(true);
  engine.SetAccessibilityFeatures(kAccessibilityFeatures);
  engine.DispatchSemanticsAction(0, kNodeId, kFlutterSemanticsActionTap,
                                 nullptr, 0);
}

TEST_P(AndroidEngineMatrixTest, MatrixExternalTextureLifecycle) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, GetParam().rendering_api);

  // Rationale: Arbitrary texture ID 200.
  constexpr int64_t kTextureId = 200;

  EXPECT_FALSE(engine.RegisterExternalTexture(kTextureId));
  EXPECT_FALSE(engine.MarkExternalTextureFrameAvailable(kTextureId));
  EXPECT_FALSE(engine.UnregisterExternalTexture(kTextureId));
}

TEST_P(AndroidEngineMatrixTest, MatrixDeferredLoadingAndMemoryPressure) {
  Settings settings;
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(settings, jni, GetParam().rendering_api);

  // Rationale: Deferred loading unit ID 5.
  constexpr intptr_t kLoadingUnitId = 5;
  const uint8_t snapshot_data[] = {0x00, 0x01, 0x02};
  EXPECT_FALSE(engine.LoadDartDeferredLibrary(
      kLoadingUnitId, snapshot_data, sizeof(snapshot_data), nullptr, 0));
  EXPECT_FALSE(engine.LoadDartDeferredLibraryError(kLoadingUnitId,
                                                   "Module not found", true));
  engine.NotifyLowMemoryWarning();
}

INSTANTIATE_TEST_SUITE_P(
    BackendAndFlagMatrix,
    AndroidEngineMatrixTest,
    ::testing::Values(
        EngineMatrixConfig{AndroidRenderingAPI::kImpellerOpenGLES, true},
        EngineMatrixConfig{AndroidRenderingAPI::kImpellerOpenGLES, false},
        EngineMatrixConfig{AndroidRenderingAPI::kImpellerVulkan, true},
        EngineMatrixConfig{AndroidRenderingAPI::kImpellerVulkan, false},
        EngineMatrixConfig{AndroidRenderingAPI::kSoftware, true},
        EngineMatrixConfig{AndroidRenderingAPI::kSoftware, false}),
    [](const ::testing::TestParamInfo<EngineMatrixConfig>& info) {
      std::string api_name;
      switch (info.param.rendering_api) {
        case AndroidRenderingAPI::kImpellerOpenGLES:
          api_name = "OpenGLES";
          break;
        case AndroidRenderingAPI::kImpellerVulkan:
          api_name = "Vulkan";
          break;
        case AndroidRenderingAPI::kSoftware:
          api_name = "Software";
          break;
        case AndroidRenderingAPI::kSkiaOpenGLES:
          api_name = "SkiaOpenGLES";
          break;
        case AndroidRenderingAPI::kImpellerAutoselect:
          api_name = "ImpellerAutoselect";
          break;
      }
      return api_name + (info.param.embedder_api_enabled ? "_EmbedderAPI"
                                                         : "_LegacyShell");
    });

}  // namespace testing
}  // namespace flutter
