// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <thread>
#include <vector>

#include "flutter/shell/platform/android/android_engine.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(AndroidEngineTest, LifecycleAndInitialState) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  EXPECT_FALSE(engine.IsValid());
  EXPECT_EQ(engine.GetRenderingAPI(), AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(engine.GetSurfaceManager(), nullptr);
  EXPECT_NE(engine.GetCompositor(), nullptr);
  EXPECT_NE(engine.GetTaskRunners(), nullptr);
  EXPECT_FALSE(engine.IsSurfaceControlEnabled());
}

TEST(AndroidEngineTest, SurfaceLifecycleTransitions) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kSoftware);

  auto window1 = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  engine.NotifySurfaceCreated(window1);
  engine.NotifySurfaceChanged(300, 400);
  engine.NotifySurfaceDestroyed();

  auto window2 = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  engine.NotifySurfaceCreated(window2);
  engine.NotifySurfaceWindowChanged(window2);
  engine.NotifySurfaceChanged(500, 600);
  engine.NotifySurfaceDestroyed();
}

TEST(AndroidEngineTest, PointerConversionHelpers) {
  // Test PointerData::Change -> FlutterPointerPhase
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(0), kCancel);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(1), kAdd);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(2), kRemove);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(3), kHover);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(4), kDown);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(5), kMove);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(6), kUp);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(7), kPanZoomStart);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(8), kPanZoomUpdate);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(9), kPanZoomEnd);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerPhase(999), kCancel);

  // Test PointerData::DeviceKind -> FlutterPointerDeviceKind
  EXPECT_EQ(AndroidEngine::ToFlutterPointerDeviceKind(0),
            kFlutterPointerDeviceKindTouch);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerDeviceKind(1),
            kFlutterPointerDeviceKindMouse);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerDeviceKind(2),
            kFlutterPointerDeviceKindStylus);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerDeviceKind(3),
            kFlutterPointerDeviceKindInvertedStylus);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerDeviceKind(4),
            kFlutterPointerDeviceKindTrackpad);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerDeviceKind(999),
            kFlutterPointerDeviceKindTouch);

  // Test PointerData signal kind -> FlutterPointerSignalKind
  EXPECT_EQ(AndroidEngine::ToFlutterPointerSignalKind(0),
            kFlutterPointerSignalKindNone);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerSignalKind(1),
            kFlutterPointerSignalKindScroll);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerSignalKind(2),
            kFlutterPointerSignalKindScrollInertiaCancel);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerSignalKind(3),
            kFlutterPointerSignalKindScale);
  EXPECT_EQ(AndroidEngine::ToFlutterPointerSignalKind(999),
            kFlutterPointerSignalKindNone);
}

TEST(AndroidEngineTest, UnpackPointerDataPacket) {
  // Empty or invalid buffer
  EXPECT_TRUE(AndroidEngine::UnpackPointerDataPacket(nullptr, 0).empty());
  std::vector<uint8_t> short_buffer(100, 0);
  EXPECT_TRUE(
      AndroidEngine::UnpackPointerDataPacket(short_buffer.data(), 100).empty());

  // 2 packet entries = 2 * 36 * 8 = 576 bytes
  constexpr size_t kEntryBytes = 36 * 8;
  std::vector<uint8_t> buffer(kEntryBytes * 2, 0);

  // First event: Touch down
  int64_t* int_fields1 = reinterpret_cast<int64_t*>(buffer.data());
  double* double_fields1 = reinterpret_cast<double*>(buffer.data());
  int_fields1[1] = 123456;   // timestamp
  int_fields1[2] = 4;        // change: kDown -> kDown
  int_fields1[3] = 0;        // kind: kTouch -> kTouch
  int_fields1[4] = 0;        // signal: kNone
  int_fields1[5] = 1;        // device: 1
  double_fields1[7] = 10.0;  // x
  double_fields1[8] = 20.0;  // y
  int_fields1[11] = 1;       // buttons
  double_fields1[14] = 0.8;  // pressure
  double_fields1[15] = 0.0;  // pressure_min
  double_fields1[16] = 1.0;  // pressure_max
  int_fields1[35] = 0;       // view_id

  // Second event: Mouse scroll
  int64_t* int_fields2 =
      reinterpret_cast<int64_t*>(buffer.data() + kEntryBytes);
  double* double_fields2 =
      reinterpret_cast<double*>(buffer.data() + kEntryBytes);
  int_fields2[1] = 123789;    // timestamp
  int_fields2[2] = 3;         // change: kHover -> kHover
  int_fields2[3] = 1;         // kind: kMouse -> kMouse
  int_fields2[4] = 1;         // signal: kScroll -> kScroll
  int_fields2[5] = 2;         // device: 2
  double_fields2[7] = 50.0;   // x
  double_fields2[8] = 60.0;   // y
  double_fields2[27] = 0.0;   // scroll_delta_x
  double_fields2[28] = 15.0;  // scroll_delta_y
  int_fields2[35] = 1;        // view_id

  auto events =
      AndroidEngine::UnpackPointerDataPacket(buffer.data(), buffer.size());
  ASSERT_EQ(events.size(), 2u);

  EXPECT_EQ(events[0].struct_size, sizeof(FlutterPointerEvent));
  EXPECT_EQ(events[0].timestamp, 123456u);
  EXPECT_EQ(events[0].phase, kDown);
  EXPECT_EQ(events[0].device_kind, kFlutterPointerDeviceKindTouch);
  EXPECT_EQ(events[0].signal_kind, kFlutterPointerSignalKindNone);
  EXPECT_EQ(events[0].device, 1);
  EXPECT_DOUBLE_EQ(events[0].x, 10.0);
  EXPECT_DOUBLE_EQ(events[0].y, 20.0);
  EXPECT_EQ(events[0].buttons, 1);
  EXPECT_DOUBLE_EQ(events[0].pressure, 0.8);
  EXPECT_EQ(events[0].view_id, 0);

  EXPECT_EQ(events[1].struct_size, sizeof(FlutterPointerEvent));
  EXPECT_EQ(events[1].timestamp, 123789u);
  EXPECT_EQ(events[1].phase, kHover);
  EXPECT_EQ(events[1].device_kind, kFlutterPointerDeviceKindMouse);
  EXPECT_EQ(events[1].signal_kind, kFlutterPointerSignalKindScroll);
  EXPECT_EQ(events[1].device, 2);
  EXPECT_DOUBLE_EQ(events[1].x, 50.0);
  EXPECT_DOUBLE_EQ(events[1].y, 60.0);
  EXPECT_DOUBLE_EQ(events[1].scroll_delta_y, 15.0);
  EXPECT_EQ(events[1].view_id, 1);
}

TEST(AndroidEngineTest, SerializeSemanticsUpdateCompleteness) {
  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);
  flags.is_button = true;
  flags.is_enabled = kFlutterTristateTrue;
  flags.is_focused = kFlutterTristateTrue;

  FlutterLocaleStringAttribute locale_attr = {};
  locale_attr.struct_size = sizeof(FlutterLocaleStringAttribute);
  locale_attr.locale = "en-US";

  FlutterStringAttribute label_attr = {};
  label_attr.struct_size = sizeof(FlutterStringAttribute);
  label_attr.start = 0;
  label_attr.end = 5;
  label_attr.type = kLocale;
  label_attr.locale = &locale_attr;

  const FlutterStringAttribute* label_attrs[] = {&label_attr};

  int32_t traversal_children[] = {10, 11};
  int32_t hit_test_children[] = {11, 10};
  int32_t custom_actions[] = {101};

  FlutterTransformation transform = {
      1.0, 0.0, 0.0,  // scaleX, skewX, transX
      0.0, 1.0, 0.0,  // skewY, scaleY, transY
      0.0, 0.0, 1.0   // pers0, pers1, pers2
  };
  FlutterTransformation hit_test_transform = {
      2.0, 0.0, 5.0,  // scaleX, skewX, transX
      0.0, 2.0, 6.0,  // skewY, scaleY, transY
      0.0, 0.0, 1.0   // pers0, pers1, pers2
  };

  FlutterSemanticsNode2 node = {};
  node.struct_size = sizeof(FlutterSemanticsNode2);
  node.id = 42;
  node.flags2 = &flags;
  node.actions = static_cast<FlutterSemanticsAction>(
      kFlutterSemanticsActionTap | kFlutterSemanticsActionLongPress);
  node.max_value_length = 50;
  node.current_value_length = 10;
  node.text_selection_base = 2;
  node.text_selection_extent = 7;
  node.platform_view_id = 77;
  node.scroll_child_count = 5;
  node.scroll_index = 1;
  node.traversal_parent = 0;
  node.scroll_position = 100.0;
  node.scroll_extent_max = 500.0;
  node.scroll_extent_min = 0.0;
  node.role = kFlutterSemanticsRoleDialog;
  node.identifier = "node_identifier";
  node.label = "Button Label";
  node.label_attribute_count = 1;
  node.label_attributes = label_attrs;
  node.value = "Active Value";
  node.hint = "Button Hint";
  node.tooltip = "Tooltip text";
  node.link_url = "https://flutter.dev";
  node.locale = "en";
  node.min_value = "0";
  node.max_value = "100";
  node.heading_level = 2;
  node.text_direction = kFlutterTextDirectionLTR;
  node.rect = {10.0, 20.0, 110.0, 70.0};
  node.transform = transform;
  node.hit_test_transform = hit_test_transform;
  node.child_count = 2;
  node.children_in_traversal_order = traversal_children;
  node.children_in_hit_test_order = hit_test_children;
  node.custom_accessibility_actions_count = 1;
  node.custom_accessibility_actions = custom_actions;

  FlutterSemanticsNode2* nodes[] = {&node};

  FlutterSemanticsCustomAction2 custom_action = {};
  custom_action.struct_size = sizeof(FlutterSemanticsCustomAction2);
  custom_action.id = 101;
  custom_action.override_action = kFlutterSemanticsActionTap;
  custom_action.label = "Custom Tap";
  custom_action.hint = "Custom Hint";

  FlutterSemanticsCustomAction2* actions[] = {&custom_action};

  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = nodes;
  update.custom_action_count = 1;
  update.custom_actions = actions;

  std::vector<uint8_t> buffer;
  std::vector<std::string> strings;
  std::vector<std::vector<uint8_t>> string_attribute_args;
  std::vector<uint8_t> actions_buffer;
  std::vector<std::string> action_strings;

  AndroidEngine::SerializeSemanticsUpdate(&update, buffer, strings,
                                          string_attribute_args, actions_buffer,
                                          action_strings);

  EXPECT_FALSE(buffer.empty());
  EXPECT_FALSE(strings.empty());
  EXPECT_FALSE(string_attribute_args.empty());
  EXPECT_FALSE(actions_buffer.empty());
  EXPECT_FALSE(action_strings.empty());

  const int32_t* buffer_int32 = reinterpret_cast<const int32_t*>(buffer.data());
  EXPECT_EQ(buffer_int32[0], 42);  // node id
  EXPECT_EQ(buffer_int32[3],
            static_cast<int32_t>(kFlutterSemanticsActionTap |
                                 kFlutterSemanticsActionLongPress));  // actions
  EXPECT_EQ(buffer_int32[4], 50);  // max_value_length
  EXPECT_EQ(buffer_int32[5], 10);  // current_value_length
  EXPECT_EQ(buffer_int32[6], 2);   // text_selection_base
  EXPECT_EQ(buffer_int32[7], 7);   // text_selection_extent
  EXPECT_EQ(buffer_int32[8], 77);  // platform_view_id
  EXPECT_EQ(buffer_int32[9], 5);   // scroll_child_count
  EXPECT_EQ(buffer_int32[10], 1);  // scroll_index
  EXPECT_EQ(buffer_int32[11], 0);  // traversal_parent

  const float* buffer_float32 = reinterpret_cast<const float*>(buffer.data());
  EXPECT_FLOAT_EQ(buffer_float32[12], 100.0f);  // scroll_position
  EXPECT_FLOAT_EQ(buffer_float32[13], 500.0f);  // scroll_extent_max
  EXPECT_FLOAT_EQ(buffer_float32[14], 0.0f);    // scroll_extent_min

  // Verify custom actions buffer
  const int32_t* actions_int32 =
      reinterpret_cast<const int32_t*>(actions_buffer.data());
  EXPECT_EQ(actions_int32[0], 101);
  EXPECT_EQ(actions_int32[1], static_cast<int32_t>(kFlutterSemanticsActionTap));
}

TEST(AndroidEngineTest, SetViewportMetricsAndDisplayFeatures) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  AndroidEngine::ViewportMetrics metrics;
  metrics.device_pixel_ratio = 2.5;
  metrics.physical_width = 1080;
  metrics.physical_height = 1920;
  metrics.physical_padding_top = 48;
  metrics.physical_padding_bottom = 96;
  metrics.physical_view_inset_bottom = 200;
  metrics.physical_display_features_bounds = {0, 950, 1080, 970};
  metrics.physical_display_features_type = {1};
  metrics.physical_display_features_state = {2};

  engine.SetViewportMetrics(0, metrics);
}

TEST(AndroidEngineTest, PointerDataPacketDispatch) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  std::vector<uint8_t> short_buffer(100, 0);
  engine.DispatchPointerDataPacket(short_buffer.data(), short_buffer.size());

  std::vector<uint8_t> valid_buffer(288, 0);
  int64_t* int_fields = reinterpret_cast<int64_t*>(valid_buffer.data());
  double* double_fields = reinterpret_cast<double*>(valid_buffer.data());

  int_fields[1] = 1000000;   // timestamp (us)
  int_fields[2] = 4;         // change: kDown
  int_fields[3] = 0;         // kind: kTouch
  int_fields[5] = 1;         // device id
  double_fields[7] = 150.0;  // physical_x
  double_fields[8] = 250.0;  // physical_y
  double_fields[14] = 1.0;   // pressure

  engine.DispatchPointerDataPacket(valid_buffer.data(), valid_buffer.size());
}

TEST(AndroidEngineTest, PlatformMessageDispatchAndResponse) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  engine.DispatchEmptyPlatformMessage(nullptr, "test/channel", 0);

  std::vector<uint8_t> response_bytes{1, 2, 3, 4};
  auto mapping = std::make_unique<fml::DataMapping>(response_bytes);
  engine.SendPlatformMessageResponse(100, std::move(mapping));
  engine.SendEmptyPlatformMessageResponse(101);
}

TEST(AndroidEngineTest, AccessibilityAndSemantics) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  engine.SetSemanticsEnabled(true);
  engine.SetAccessibilityFeatures(0x01 | 0x02);
  engine.DispatchSemanticsAction(nullptr, 42, 1, nullptr, 0);
}

TEST(AndroidEngineTest, ExternalTexturesAndFrameScheduling) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  engine.RegisterExternalTexture(1001, null_ref);
  engine.RegisterImageTexture(1002, null_ref, 0);
  engine.MarkTextureFrameAvailable(1001);
  engine.ScheduleFrame();
  engine.UnregisterTexture(1001);
  engine.UnregisterTexture(1002);
}

TEST(AndroidEngineTest, DeferredLibraryLoading) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  std::vector<uint8_t> data{0xDE, 0xAD, 0xBE, 0xEF};
  auto data_mapping = std::make_unique<fml::DataMapping>(data);
  auto instr_mapping = std::make_unique<fml::DataMapping>(data);

  engine.LoadDartDeferredLibrary(123, std::move(data_mapping),
                                 std::move(instr_mapping));
  engine.LoadDartDeferredLibraryError(123, "Test error message", true);
}

TEST(AndroidEngineTest, ConcurrentMultiInstanceThreadSafety) {
  constexpr size_t kThreadCount = 4;
  std::vector<std::thread> threads;
  threads.reserve(kThreadCount);

  for (size_t i = 0; i < kThreadCount; ++i) {
    threads.emplace_back([i]() {
      auto jni = std::make_shared<JNIMock>();
      auto api = (i % 2 == 0) ? AndroidRenderingAPI::kSoftware
                              : AndroidRenderingAPI::kImpellerOpenGLES;
      AndroidEngine engine(Settings(), jni, api);

      auto window = fml::MakeRefCounted<AndroidNativeWindow>(
          nullptr, /*is_fake_window=*/true);
      engine.NotifySurfaceCreated(window);
      engine.NotifySurfaceChanged(100, 100);
      engine.SetSemanticsEnabled(true);
      engine.ScheduleFrame();
      engine.NotifySurfaceDestroyed();
    });
  }

  for (auto& t : threads) {
    t.join();
  }
}

TEST(AndroidEngineTest, LaunchAndSpawnValidation) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  EXPECT_FALSE(engine.IsValid());
  EXPECT_EQ(engine.GetEmbedderEngineHandle(), nullptr);

  auto spawned = engine.Spawn(jni, "main", "", "", {}, 42);
  EXPECT_EQ(spawned, nullptr);
}

TEST(AndroidEngineTest, ScreenshotUninitialized) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  auto screenshot = engine.Screenshot(
      AndroidEngine::ScreenshotType::kUncompressedImage, false);
  EXPECT_EQ(screenshot.data, nullptr);
  EXPECT_EQ(screenshot.frame_size.width, 0u);
  EXPECT_EQ(screenshot.frame_size.height, 0u);
}

TEST(AndroidEngineTest, LowMemoryAndAssetResolver) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  engine.NotifyLowMemoryWarning();
  engine.UpdateAssetResolverByType(nullptr, 0);
}

TEST(AndroidEngineTest, CompositorDelegation) {
  auto jni = std::make_shared<JNIMock>();
  AndroidEngine engine(Settings(), jni, AndroidRenderingAPI::kImpellerOpenGLES);

  FlutterPoint offset = {10.0, 20.0};
  FlutterSize size = {300.0, 400.0};

  FlutterPlatformViewMutation m1 = {};
  m1.type = kFlutterPlatformViewMutationTypeTransformation;
  m1.transformation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};

  FlutterPlatformViewMutation m2 = {};
  m2.type = kFlutterPlatformViewMutationTypeOpacity;
  m2.opacity = 0.5;

  FlutterPlatformViewMutation m3 = {};
  m3.type = kFlutterPlatformViewMutationTypeClipRect;
  m3.clip_rect = {0.0, 0.0, 100.0, 100.0};

  const FlutterPlatformViewMutation* mutations[] = {&m1, &m2, &m3};

  engine.OnPlatformViewPresented(101, offset, size, 3, mutations);
  engine.OnFramePresented();
  engine.UpdateDisplayMetrics();
}

TEST(AndroidEngineTest, CompositorDelegateLifecycle) {
  auto jni = std::make_shared<JNIMock>();
  std::shared_ptr<AndroidCompositor> retained_compositor;
  {
    AndroidEngine engine(Settings(), jni,
                         AndroidRenderingAPI::kImpellerOpenGLES);
    retained_compositor = engine.GetCompositor();
    ASSERT_NE(retained_compositor, nullptr);
  }
  // Engine is now destroyed. Presenting layers through retained_compositor must
  // not crash or dereference a dangling delegate.
  FlutterBackingStore backing_store = {};
  backing_store.struct_size = sizeof(FlutterBackingStore);
  backing_store.type = kFlutterBackingStoreTypeOpenGL;
  backing_store.open_gl.type = kFlutterOpenGLTargetTypeFramebuffer;
  backing_store.open_gl.framebuffer.name = 0;

  FlutterLayer layer = {};
  layer.struct_size = sizeof(FlutterLayer);
  layer.type = kFlutterLayerContentTypeBackingStore;
  layer.backing_store = &backing_store;
  layer.offset = {0.0, 0.0};
  layer.size = {100.0, 100.0};

  const FlutterLayer* layers[] = {&layer};
  retained_compositor->PresentLayers(layers, 1);
}

TEST(AndroidEngineTest, DualFlagMatrixTest) {
  for (bool embedder_api_enabled : {false, true}) {
    FlutterMain::SetEmbedderAPIEnabledForTesting(embedder_api_enabled);
    EXPECT_EQ(FlutterMain::IsEmbedderAPIEnabled(), embedder_api_enabled);

    auto jni = std::make_shared<JNIMock>();
    AndroidEngine engine(Settings(), jni,
                         AndroidRenderingAPI::kImpellerOpenGLES);

    auto window = fml::MakeRefCounted<AndroidNativeWindow>(
        nullptr, /*is_fake_window=*/true);
    engine.NotifySurfaceCreated(window);
    engine.NotifySurfaceDestroyed();
  }
  FlutterMain::ResetEmbedderAPIEnabledForTesting();
}

}  // namespace testing
}  // namespace flutter
