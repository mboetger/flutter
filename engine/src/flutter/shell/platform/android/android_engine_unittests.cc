// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_engine.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/thread.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"

namespace flutter {
namespace testing {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;

class AndroidEngineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FlutterMain::ResetForTesting();
    jni_facade_ = std::make_shared<JNIMock>();
    memset(&mock_proc_table_, 0, sizeof(FlutterEngineProcTable));
    mock_proc_table_.struct_size = sizeof(FlutterEngineProcTable);

    mock_proc_table_.Initialize =
        [](size_t version, const FlutterRendererConfig* config,
           const FlutterProjectArgs* args, void* user_data,
           FLUTTER_API_SYMBOL(FlutterEngine) *
               engine_out) -> FlutterEngineResult {
      *engine_out = reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(0x1234);
      return kSuccess;
    };

    mock_proc_table_.RunInitialized = [](FLUTTER_API_SYMBOL(FlutterEngine)
                                             engine) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.Shutdown = [](FLUTTER_API_SYMBOL(FlutterEngine)
                                       engine) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.Deinitialize = [](FLUTTER_API_SYMBOL(FlutterEngine)
                                           engine) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.SendWindowMetricsEvent =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           const FlutterWindowMetricsEvent* event) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.SendPointerEvent =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           const FlutterPointerEvent* events,
           size_t events_count) -> FlutterEngineResult { return kSuccess; };

    mock_proc_table_.SendPlatformMessage =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           const FlutterPlatformMessage* message) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.PlatformMessageCreateResponseHandle =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           FlutterDataCallback data_callback, void* user_data,
           FlutterPlatformMessageResponseHandle** response_out)
        -> FlutterEngineResult {
      *response_out =
          reinterpret_cast<FlutterPlatformMessageResponseHandle*>(0x5678);
      return kSuccess;
    };

    mock_proc_table_.PlatformMessageReleaseResponseHandle =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           FlutterPlatformMessageResponseHandle* response)
        -> FlutterEngineResult { return kSuccess; };

    mock_proc_table_.SendPlatformMessageResponse =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           const FlutterPlatformMessageResponseHandle* handle,
           const uint8_t* data,
           size_t data_length) -> FlutterEngineResult { return kSuccess; };

    mock_proc_table_.UpdateSemanticsEnabled =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           bool enabled) -> FlutterEngineResult { return kSuccess; };

    mock_proc_table_.UpdateAccessibilityFeatures =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           FlutterAccessibilityFeature features) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.DispatchSemanticsAction =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine, uint64_t id,
           FlutterSemanticsAction action, const uint8_t* data,
           size_t data_length) -> FlutterEngineResult { return kSuccess; };

    mock_proc_table_.LoadDartDeferredLibrary =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           const FlutterDeferredLibraryInfo* info) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.LoadDartDeferredLibraryError =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           const FlutterDeferredLibraryErrorInfo* info) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.RegisterExternalTexture =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           int64_t texture_identifier) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.UnregisterExternalTexture =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           int64_t texture_identifier) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.MarkExternalTextureFrameAvailable =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           int64_t texture_identifier) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.ScheduleFrame = [](FLUTTER_API_SYMBOL(FlutterEngine)
                                            engine) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.NotifyLowMemoryWarning =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine) -> FlutterEngineResult {
      return kSuccess;
    };

    mock_proc_table_.Screenshot =
        [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           FlutterEngineScreenshotType type, bool base64_encode,
           FlutterEngineScreenshotCallback callback,
           void* user_data) -> FlutterEngineResult { return kSuccess; };

    mock_proc_table_.Spawn = [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                const FlutterEngineSpawnInfo* spawn_info,
                                FLUTTER_API_SYMBOL(FlutterEngine) *
                                    engine_out) -> FlutterEngineResult {
      *engine_out = reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(0x9ABC);
      return kSuccess;
    };
  }

  void TearDown() override { FlutterMain::ResetForTesting(); }

  std::unique_ptr<AndroidEngine> CreateEngine(
      AndroidRenderingAPI rendering_api) {
    auto surface_manager =
        std::make_shared<AndroidSurfaceManager>(rendering_api);
    auto compositor =
        std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
    return std::make_unique<AndroidEngine>(
        settings_, jni_facade_, surface_manager, std::move(compositor),
        &mock_proc_table_);
  }

  std::shared_ptr<JNIMock> jni_facade_;
  FlutterEngineProcTable mock_proc_table_;
  flutter::Settings settings_;
};

TEST_F(AndroidEngineTest, InitializationAndValidity) {
  AndroidEngine engine(settings_, jni_facade_, AndroidRenderingAPI::kSoftware,
                       nullptr, nullptr);
  EXPECT_TRUE(engine.IsValid());
  EXPECT_FALSE(engine.IsRunning());
  EXPECT_EQ(engine.GetRenderingAPI(), AndroidRenderingAPI::kSoftware);
}

TEST_F(AndroidEngineTest, LaunchSuccess) {
  static bool s_initialize_called = false;
  static bool s_run_initialized_called = false;
  s_initialize_called = false;
  s_run_initialized_called = false;

  mock_proc_table_.Initialize =
      [](size_t version, const FlutterRendererConfig* config,
         const FlutterProjectArgs* args, void* user_data,
         FLUTTER_API_SYMBOL(FlutterEngine) *
             engine_out) -> FlutterEngineResult {
    s_initialize_called = true;
    EXPECT_EQ(version, static_cast<size_t>(FLUTTER_ENGINE_VERSION));
    EXPECT_NE(config, nullptr);
    EXPECT_NE(args, nullptr);
    EXPECT_NE(args->custom_task_runners, nullptr);
    EXPECT_EQ(args->custom_task_runners->thread_priority_setter,
              &AndroidSetThreadPriority);
    EXPECT_NE(args->compositor, nullptr);
    EXPECT_NE(user_data, nullptr);
    *engine_out = reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(0x1234);
    return kSuccess;
  };

  mock_proc_table_.RunInitialized = [](FLUTTER_API_SYMBOL(FlutterEngine)
                                           engine) -> FlutterEngineResult {
    s_run_initialized_called = true;
    EXPECT_EQ(engine,
              reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(0x1234));
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);

  std::vector<std::string> args = {"--test-arg=1"};
  bool launched = engine.Launch(nullptr, "custom_main", "custom_lib", args, 42);
  EXPECT_TRUE(launched);
  EXPECT_TRUE(s_initialize_called);
  EXPECT_TRUE(s_run_initialized_called);
  EXPECT_TRUE(engine.IsRunning());
  EXPECT_TRUE(engine.IsValid());
}

TEST_F(AndroidEngineTest, LaunchFailureOnInitialize) {
  mock_proc_table_.Initialize =
      [](size_t version, const FlutterRendererConfig* config,
         const FlutterProjectArgs* args, void* user_data,
         FLUTTER_API_SYMBOL(FlutterEngine) *
             engine_out) -> FlutterEngineResult { return kInvalidArguments; };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);

  bool launched = engine.Launch(nullptr, "", "", {}, 0);
  EXPECT_FALSE(launched);
  EXPECT_FALSE(engine.IsRunning());
  EXPECT_FALSE(engine.IsValid());
}

TEST_F(AndroidEngineTest, MultiEngineSpawnSuccess) {
  static bool s_spawn_called = false;
  s_spawn_called = false;

  mock_proc_table_.Spawn = [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                              const FlutterEngineSpawnInfo* spawn_info,
                              FLUTTER_API_SYMBOL(FlutterEngine) *
                                  engine_out) -> FlutterEngineResult {
    s_spawn_called = true;
    EXPECT_NE(spawn_info, nullptr);
    EXPECT_EQ(spawn_info->struct_size, sizeof(FlutterEngineSpawnInfo));
    EXPECT_STREQ(spawn_info->entrypoint, "spawned_main");
    EXPECT_STREQ(spawn_info->library_uri, "spawned_uri");
    EXPECT_STREQ(spawn_info->initial_route, "/spawned_route");
    EXPECT_EQ(spawn_info->entrypoint_argc, 1);
    EXPECT_STREQ(spawn_info->entrypoint_argv[0], "arg1");
    EXPECT_EQ(spawn_info->engine_id, 100);
    *engine_out = reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(0x9999);
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine parent_engine(settings_, jni_facade_, surface_manager,
                              std::move(compositor), &mock_proc_table_);

  EXPECT_TRUE(parent_engine.Launch(nullptr, "main", "", {}, 1));

  auto spawned_jni = std::make_shared<JNIMock>();
  std::vector<std::string> spawn_args = {"arg1"};
  auto spawned_engine =
      parent_engine.Spawn(spawned_jni, "spawned_main", "spawned_uri",
                          "/spawned_route", spawn_args, 100);

  EXPECT_TRUE(s_spawn_called);
  ASSERT_NE(spawned_engine, nullptr);
  EXPECT_TRUE(spawned_engine->IsValid());
  EXPECT_TRUE(spawned_engine->IsRunning());
}

TEST_F(AndroidEngineTest, SpawnFailsWhenParentNotRunning) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine parent_engine(settings_, jni_facade_, surface_manager,
                              std::move(compositor), &mock_proc_table_);

  auto spawned_engine = parent_engine.Spawn(jni_facade_, "", "", "", {}, 0);
  EXPECT_EQ(spawned_engine, nullptr);
}

TEST_F(AndroidEngineTest, ViewportMetricsForwarding) {
  static bool s_metrics_called = false;
  static FlutterWindowMetricsEvent s_captured_metrics = {};
  s_metrics_called = false;

  mock_proc_table_.SendWindowMetricsEvent =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterWindowMetricsEvent* event) -> FlutterEngineResult {
    s_metrics_called = true;
    s_captured_metrics = *event;
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  AndroidViewportMetrics metrics = {};
  metrics.device_pixel_ratio = 2.5;
  metrics.physical_width = 1080.0;
  metrics.physical_height = 1920.0;
  metrics.physical_view_inset_top = 48.0;
  metrics.physical_view_inset_bottom = 96.0;
  metrics.physical_min_width = 500.0;
  metrics.physical_max_width = 1080.0;
  metrics.display_id = 1;

  engine.SetViewportMetrics(42, metrics);

  EXPECT_TRUE(s_metrics_called);
  EXPECT_EQ(s_captured_metrics.view_id, 42);
  EXPECT_EQ(s_captured_metrics.width, 1080u);
  EXPECT_EQ(s_captured_metrics.height, 1920u);
  EXPECT_DOUBLE_EQ(s_captured_metrics.pixel_ratio, 2.5);
  EXPECT_DOUBLE_EQ(s_captured_metrics.physical_view_inset_top, 48.0);
  EXPECT_DOUBLE_EQ(s_captured_metrics.physical_view_inset_bottom, 96.0);
  EXPECT_TRUE(s_captured_metrics.has_constraints);
  EXPECT_EQ(s_captured_metrics.min_width_constraint, 500u);
  EXPECT_EQ(s_captured_metrics.max_width_constraint, 1080u);
  EXPECT_EQ(s_captured_metrics.display_id, 1u);
}

TEST_F(AndroidEngineTest, PointerDataPacketParsingAndDispatch) {
  static bool s_pointer_called = false;
  static std::vector<FlutterPointerEvent> s_captured_events;
  s_pointer_called = false;
  s_captured_events.clear();

  mock_proc_table_.SendPointerEvent =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterPointerEvent* events,
         size_t events_count) -> FlutterEngineResult {
    s_pointer_called = true;
    s_captured_events.assign(events, events + events_count);
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  constexpr size_t kRecordSize = 288;
  std::vector<uint8_t> buffer(kRecordSize, 0);

  int64_t time_stamp = 123456789;
  int64_t change = 4;  // down
  int64_t kind = 0;    // touch
  int64_t signal_kind = 0;
  int64_t device = 1;
  double physical_x = 350.0;
  double physical_y = 700.0;
  int64_t buttons = 1;
  double pressure = 0.8;
  double pressure_min = 0.0;
  double pressure_max = 1.0;
  int64_t view_id = 5;

  memcpy(buffer.data() + 8, &time_stamp, sizeof(int64_t));
  memcpy(buffer.data() + 16, &change, sizeof(int64_t));
  memcpy(buffer.data() + 24, &kind, sizeof(int64_t));
  memcpy(buffer.data() + 32, &signal_kind, sizeof(int64_t));
  memcpy(buffer.data() + 40, &device, sizeof(int64_t));
  memcpy(buffer.data() + 56, &physical_x, sizeof(double));
  memcpy(buffer.data() + 64, &physical_y, sizeof(double));
  memcpy(buffer.data() + 88, &buttons, sizeof(int64_t));
  memcpy(buffer.data() + 112, &pressure, sizeof(double));
  memcpy(buffer.data() + 120, &pressure_min, sizeof(double));
  memcpy(buffer.data() + 128, &pressure_max, sizeof(double));
  memcpy(buffer.data() + 280, &view_id, sizeof(int64_t));

  engine.DispatchPointerDataPacket(buffer.data(), buffer.size());

  EXPECT_TRUE(s_pointer_called);
  ASSERT_EQ(s_captured_events.size(), 1u);
  EXPECT_EQ(s_captured_events[0].phase, FlutterPointerPhase::kDown);
  EXPECT_EQ(s_captured_events[0].device_kind,
            FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch);
  EXPECT_DOUBLE_EQ(s_captured_events[0].x, 350.0);
  EXPECT_DOUBLE_EQ(s_captured_events[0].y, 700.0);
  EXPECT_DOUBLE_EQ(s_captured_events[0].pressure, 0.8);
  EXPECT_EQ(s_captured_events[0].view_id, 5);
}

TEST_F(AndroidEngineTest, PointerDataPacketCorruptBufferIgnored) {
  static bool s_pointer_called = false;
  s_pointer_called = false;

  mock_proc_table_.SendPointerEvent =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterPointerEvent* events,
         size_t events_count) -> FlutterEngineResult {
    s_pointer_called = true;
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  std::vector<uint8_t> corrupted_buffer(100, 0);  // not a multiple of 288
  engine.DispatchPointerDataPacket(corrupted_buffer.data(),
                                   corrupted_buffer.size());
  EXPECT_FALSE(s_pointer_called);
}

TEST_F(AndroidEngineTest, PlatformMessageSendingAndResponse) {
  static bool s_message_sent = false;
  static std::string s_captured_channel;
  s_message_sent = false;

  mock_proc_table_.SendPlatformMessage =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterPlatformMessage* message) -> FlutterEngineResult {
    s_message_sent = true;
    s_captured_channel = message->channel;
    EXPECT_NE(message->response_handle, nullptr);
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  const std::string payload = "Hello Flutter";
  engine.SendPlatformMessage("flutter/test_channel",
                             reinterpret_cast<const uint8_t*>(payload.data()),
                             payload.size(), 42);

  EXPECT_TRUE(s_message_sent);
  EXPECT_EQ(s_captured_channel, "flutter/test_channel");
}

TEST_F(AndroidEngineTest, SemanticsAndAccessibilityForwarding) {
  static bool s_semantics_enabled_called = false;
  static bool s_semantics_enabled_val = false;
  static bool s_accessibility_called = false;
  static FlutterAccessibilityFeature s_captured_features =
      kFlutterAccessibilityFeatureAccessibleNavigation;
  static bool s_action_called = false;

  mock_proc_table_.UpdateSemanticsEnabled =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         bool enabled) -> FlutterEngineResult {
    s_semantics_enabled_called = true;
    s_semantics_enabled_val = enabled;
    return kSuccess;
  };

  mock_proc_table_.UpdateAccessibilityFeatures =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         FlutterAccessibilityFeature features) -> FlutterEngineResult {
    s_accessibility_called = true;
    s_captured_features = features;
    return kSuccess;
  };

  mock_proc_table_.DispatchSemanticsAction =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine, uint64_t id,
         FlutterSemanticsAction action, const uint8_t* data,
         size_t data_length) -> FlutterEngineResult {
    s_action_called = true;
    EXPECT_EQ(id, 123u);
    EXPECT_EQ(action, kFlutterSemanticsActionTap);
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  EXPECT_CALL(*jni_facade_, FlutterViewSetSemanticsTreeEnabled(true)).Times(1);

  engine.SetSemanticsEnabled(true);
  EXPECT_TRUE(s_semantics_enabled_called);
  EXPECT_TRUE(s_semantics_enabled_val);

  engine.SetAccessibilityFeatures(
      kFlutterAccessibilityFeatureAccessibleNavigation);
  EXPECT_TRUE(s_accessibility_called);
  EXPECT_EQ(s_captured_features,
            kFlutterAccessibilityFeatureAccessibleNavigation);

  engine.DispatchSemanticsAction(123, kFlutterSemanticsActionTap, nullptr, 0);
  EXPECT_TRUE(s_action_called);
}

TEST_F(AndroidEngineTest, DartDeferredLibrariesForwarding) {
  static bool s_load_called = false;
  static bool s_error_called = false;

  mock_proc_table_.LoadDartDeferredLibrary =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterDeferredLibraryInfo* info) -> FlutterEngineResult {
    s_load_called = true;
    EXPECT_NE(info, nullptr);
    if (info != nullptr) {
      EXPECT_EQ(info->struct_size, sizeof(FlutterDeferredLibraryInfo));
      EXPECT_EQ(info->loading_unit_id, 42);
      EXPECT_EQ(info->snapshot_data_size, 10u);
      EXPECT_EQ(info->snapshot_instructions_size, 20u);
    }
    return kSuccess;
  };

  mock_proc_table_.LoadDartDeferredLibraryError =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterDeferredLibraryErrorInfo* error_info)
      -> FlutterEngineResult {
    s_error_called = true;
    EXPECT_NE(error_info, nullptr);
    if (error_info != nullptr) {
      EXPECT_EQ(error_info->struct_size,
                sizeof(FlutterDeferredLibraryErrorInfo));
      EXPECT_EQ(error_info->loading_unit_id, 42);
      EXPECT_STREQ(error_info->error_message, "Download failed");
      EXPECT_TRUE(error_info->transient);
    }
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  std::vector<uint8_t> data(10, 0xAA);
  std::vector<uint8_t> inst(20, 0xBB);

  bool loaded = engine.LoadDartDeferredLibrary(
      42, std::make_unique<fml::NonOwnedMapping>(data.data(), data.size()),
      std::make_unique<fml::NonOwnedMapping>(inst.data(), inst.size()));
  EXPECT_TRUE(loaded);
  EXPECT_TRUE(s_load_called);

  bool errored =
      engine.LoadDartDeferredLibraryError(42, "Download failed", true);
  EXPECT_TRUE(errored);
  EXPECT_TRUE(s_error_called);
}

TEST_F(AndroidEngineTest, ExternalTextureAndSchedulingForwarding) {
  static bool s_reg_called = false;
  static bool s_unreg_called = false;
  static bool s_mark_called = false;
  static bool s_sched_called = false;

  mock_proc_table_.RegisterExternalTexture =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         int64_t texture_identifier) -> FlutterEngineResult {
    s_reg_called = true;
    EXPECT_EQ(texture_identifier, 777);
    return kSuccess;
  };

  mock_proc_table_.UnregisterExternalTexture =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         int64_t texture_identifier) -> FlutterEngineResult {
    s_unreg_called = true;
    EXPECT_EQ(texture_identifier, 777);
    return kSuccess;
  };

  mock_proc_table_.MarkExternalTextureFrameAvailable =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         int64_t texture_identifier) -> FlutterEngineResult {
    s_mark_called = true;
    EXPECT_EQ(texture_identifier, 777);
    return kSuccess;
  };

  mock_proc_table_.ScheduleFrame = [](FLUTTER_API_SYMBOL(FlutterEngine)
                                          engine) -> FlutterEngineResult {
    s_sched_called = true;
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  engine.RegisterExternalTexture(777);
  EXPECT_TRUE(s_reg_called);

  engine.MarkTextureFrameAvailable(777);
  EXPECT_TRUE(s_mark_called);

  engine.ScheduleFrame();
  EXPECT_TRUE(s_sched_called);

  engine.UnregisterTexture(777);
  EXPECT_TRUE(s_unreg_called);
}

TEST_F(AndroidEngineTest, LowMemoryAndScreenshotForwarding) {
  static bool s_low_mem_called = false;
  static bool s_screenshot_called = false;

  mock_proc_table_.NotifyLowMemoryWarning =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine) -> FlutterEngineResult {
    s_low_mem_called = true;
    return kSuccess;
  };

  mock_proc_table_.Screenshot = [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                   FlutterEngineScreenshotType type,
                                   bool base64_encode,
                                   FlutterEngineScreenshotCallback callback,
                                   void* user_data) -> FlutterEngineResult {
    s_screenshot_called = true;
    EXPECT_EQ(type, kFlutterEngineScreenshotTypeUncompressedImage);
    EXPECT_FALSE(base64_encode);
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  engine.NotifyLowMemoryWarning();
  EXPECT_TRUE(s_low_mem_called);

  bool shot = engine.Screenshot(
      kFlutterEngineScreenshotTypeUncompressedImage, false,
      [](const FlutterEngineScreenshotInfo*, void*) {}, nullptr);
  EXPECT_TRUE(shot);
  EXPECT_TRUE(s_screenshot_called);
}

TEST_F(AndroidEngineTest, FeatureFlagDualStateGating) {
  // Flag = false
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  AndroidEngine engine_disabled(
      settings_, jni_facade_, AndroidRenderingAPI::kSoftware, nullptr, nullptr);
  EXPECT_FALSE(engine_disabled.IsEmbedderAPIEnabled());

  // Flag = true
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  AndroidEngine engine_enabled(
      settings_, jni_facade_, AndroidRenderingAPI::kSoftware, nullptr, nullptr);
  EXPECT_TRUE(engine_enabled.IsEmbedderAPIEnabled());
}

TEST_F(AndroidEngineTest, MultiPacketPointerDataParsing) {
  static bool s_pointer_called = false;
  static std::vector<FlutterPointerEvent> s_captured_events;
  s_pointer_called = false;
  s_captured_events.clear();

  mock_proc_table_.SendPointerEvent =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterPointerEvent* events,
         size_t events_count) -> FlutterEngineResult {
    s_pointer_called = true;
    s_captured_events.assign(events, events + events_count);
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  constexpr size_t kRecordSize = 288;
  std::vector<uint8_t> buffer(kRecordSize * 2, 0);

  // Packet 0: Down at (100, 200)
  int64_t time_stamp_0 = 1000;
  int64_t change_0 = 4;  // down
  int64_t kind_0 = 0;    // touch
  int64_t signal_kind_0 = 0;
  int64_t device_0 = 1;
  double x_0 = 100.0;
  double y_0 = 200.0;
  int64_t buttons_0 = 1;
  double pressure_0 = 0.5;
  int64_t view_id_0 = 10;

  memcpy(buffer.data() + 8, &time_stamp_0, sizeof(int64_t));
  memcpy(buffer.data() + 16, &change_0, sizeof(int64_t));
  memcpy(buffer.data() + 24, &kind_0, sizeof(int64_t));
  memcpy(buffer.data() + 32, &signal_kind_0, sizeof(int64_t));
  memcpy(buffer.data() + 40, &device_0, sizeof(int64_t));
  memcpy(buffer.data() + 56, &x_0, sizeof(double));
  memcpy(buffer.data() + 64, &y_0, sizeof(double));
  memcpy(buffer.data() + 88, &buttons_0, sizeof(int64_t));
  memcpy(buffer.data() + 112, &pressure_0, sizeof(double));
  memcpy(buffer.data() + 280, &view_id_0, sizeof(int64_t));

  // Packet 1: Move at (150, 250)
  int64_t time_stamp_1 = 2000;
  int64_t change_1 = 5;  // move
  int64_t kind_1 = 0;    // touch
  int64_t signal_kind_1 = 0;
  int64_t device_1 = 1;
  double x_1 = 150.0;
  double y_1 = 250.0;
  int64_t buttons_1 = 1;
  double pressure_1 = 0.6;
  int64_t view_id_1 = 10;

  size_t offset_1 = kRecordSize;
  memcpy(buffer.data() + offset_1 + 8, &time_stamp_1, sizeof(int64_t));
  memcpy(buffer.data() + offset_1 + 16, &change_1, sizeof(int64_t));
  memcpy(buffer.data() + offset_1 + 24, &kind_1, sizeof(int64_t));
  memcpy(buffer.data() + offset_1 + 32, &signal_kind_1, sizeof(int64_t));
  memcpy(buffer.data() + offset_1 + 40, &device_1, sizeof(int64_t));
  memcpy(buffer.data() + offset_1 + 56, &x_1, sizeof(double));
  memcpy(buffer.data() + offset_1 + 64, &y_1, sizeof(double));
  memcpy(buffer.data() + offset_1 + 88, &buttons_1, sizeof(int64_t));
  memcpy(buffer.data() + offset_1 + 112, &pressure_1, sizeof(double));
  memcpy(buffer.data() + offset_1 + 280, &view_id_1, sizeof(int64_t));

  engine.DispatchPointerDataPacket(buffer.data(), buffer.size());

  EXPECT_TRUE(s_pointer_called);
  ASSERT_EQ(s_captured_events.size(), 2u);
  EXPECT_EQ(s_captured_events[0].phase, FlutterPointerPhase::kDown);
  EXPECT_DOUBLE_EQ(s_captured_events[0].x, 100.0);
  EXPECT_DOUBLE_EQ(s_captured_events[0].y, 200.0);
  EXPECT_EQ(s_captured_events[0].view_id, 10);

  EXPECT_EQ(s_captured_events[1].phase, FlutterPointerPhase::kMove);
  EXPECT_DOUBLE_EQ(s_captured_events[1].x, 150.0);
  EXPECT_DOUBLE_EQ(s_captured_events[1].y, 250.0);
  EXPECT_EQ(s_captured_events[1].view_id, 10);
}

TEST_F(AndroidEngineTest, SurfaceLifecycleForwarding) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  engine.OnSurfaceCreated(nullptr);
  engine.OnSurfaceWindowChanged(nullptr);
  engine.OnSurfaceResized(1080, 1920);
  engine.OnSurfaceDestroyed();
}

TEST_F(AndroidEngineTest, IncomingPlatformMessageAndResponseRouting) {
  static bool s_send_response_called = false;
  static const FlutterPlatformMessageResponseHandle* s_captured_handle =
      nullptr;
  static std::vector<uint8_t> s_captured_response_data;
  s_send_response_called = false;
  s_captured_handle = nullptr;
  s_captured_response_data.clear();

  mock_proc_table_.SendPlatformMessageResponse =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterPlatformMessageResponseHandle* handle,
         const uint8_t* data, size_t data_length) -> FlutterEngineResult {
    s_send_response_called = true;
    s_captured_handle = handle;
    if (data != nullptr && data_length > 0) {
      s_captured_response_data.assign(data, data + data_length);
    }
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  int32_t captured_response_id = 0;
  EXPECT_CALL(*jni_facade_,
              FlutterViewHandlePlatformMessage("flutter/test_channel", _, _, _))
      .WillOnce([&captured_response_id](const std::string& channel,
                                        const uint8_t* message, size_t size,
                                        int32_t response_id) {
        captured_response_id = response_id;
      });

  const auto* mock_handle =
      reinterpret_cast<const FlutterPlatformMessageResponseHandle*>(0x7777);
  std::string message_payload = "Request payload";
  FlutterPlatformMessage incoming_msg = {};
  incoming_msg.struct_size = sizeof(FlutterPlatformMessage);
  incoming_msg.channel = "flutter/test_channel";
  incoming_msg.message =
      reinterpret_cast<const uint8_t*>(message_payload.data());
  incoming_msg.message_size = message_payload.size();
  incoming_msg.response_handle =
      const_cast<FlutterPlatformMessageResponseHandle*>(mock_handle);

  engine.HandlePlatformMessage(&incoming_msg);

  EXPECT_GT(captured_response_id, 0);

  // Send response back
  std::string response_payload = "Response payload";
  engine.SendPlatformMessageResponse(
      captured_response_id,
      reinterpret_cast<const uint8_t*>(response_payload.data()),
      response_payload.size());

  EXPECT_TRUE(s_send_response_called);
  EXPECT_EQ(s_captured_handle, mock_handle);
  EXPECT_EQ(std::string(s_captured_response_data.begin(),
                        s_captured_response_data.end()),
            "Response payload");

  // Subsequent call with the same response ID does nothing because handle was
  // consumed
  s_send_response_called = false;
  engine.SendPlatformMessageResponse(
      captured_response_id,
      reinterpret_cast<const uint8_t*>(response_payload.data()),
      response_payload.size());
  EXPECT_FALSE(s_send_response_called);
}

TEST_F(AndroidEngineTest, IncomingPlatformMessageEmptyResponse) {
  static bool s_send_response_called = false;
  static const FlutterPlatformMessageResponseHandle* s_captured_handle =
      nullptr;
  s_send_response_called = false;
  s_captured_handle = nullptr;

  mock_proc_table_.SendPlatformMessageResponse =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterPlatformMessageResponseHandle* handle,
         const uint8_t* data, size_t data_length) -> FlutterEngineResult {
    s_send_response_called = true;
    s_captured_handle = handle;
    EXPECT_EQ(data, nullptr);
    EXPECT_EQ(data_length, 0u);
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  int32_t captured_response_id = 0;
  EXPECT_CALL(*jni_facade_,
              FlutterViewHandlePlatformMessage("flutter/ping", _, _, _))
      .WillOnce([&captured_response_id](const std::string& channel,
                                        const uint8_t* message, size_t size,
                                        int32_t response_id) {
        captured_response_id = response_id;
      });

  const auto* mock_handle =
      reinterpret_cast<const FlutterPlatformMessageResponseHandle*>(0x6666);
  FlutterPlatformMessage incoming_msg = {};
  incoming_msg.struct_size = sizeof(FlutterPlatformMessage);
  incoming_msg.channel = "flutter/ping";
  incoming_msg.response_handle =
      const_cast<FlutterPlatformMessageResponseHandle*>(mock_handle);

  engine.HandlePlatformMessage(&incoming_msg);

  EXPECT_GT(captured_response_id, 0);

  engine.CompletePlatformMessageEmptyResponse(captured_response_id);
  EXPECT_TRUE(s_send_response_called);
  EXPECT_EQ(s_captured_handle, mock_handle);
}

TEST_F(AndroidEngineTest, DestructorCleansUpPendingIncomingResponses) {
  static bool s_send_response_called = false;
  static const FlutterPlatformMessageResponseHandle* s_captured_handle =
      nullptr;
  s_send_response_called = false;
  s_captured_handle = nullptr;

  mock_proc_table_.SendPlatformMessageResponse =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterPlatformMessageResponseHandle* handle,
         const uint8_t* data, size_t data_length) -> FlutterEngineResult {
    s_send_response_called = true;
    s_captured_handle = handle;
    EXPECT_EQ(data, nullptr);
    EXPECT_EQ(data_length, 0u);
    return kSuccess;
  };

  const auto* mock_handle =
      reinterpret_cast<const FlutterPlatformMessageResponseHandle*>(0x8888);

  {
    auto surface_manager =
        std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
    auto compositor =
        std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
    AndroidEngine engine(settings_, jni_facade_, surface_manager,
                         std::move(compositor), &mock_proc_table_);
    EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

    FlutterPlatformMessage incoming_msg = {};
    incoming_msg.struct_size = sizeof(FlutterPlatformMessage);
    incoming_msg.channel = "flutter/test";
    incoming_msg.response_handle =
        const_cast<FlutterPlatformMessageResponseHandle*>(mock_handle);

    engine.HandlePlatformMessage(&incoming_msg);
    // engine destroyed at end of scope without calling
    // SendPlatformMessageResponse
  }

  EXPECT_TRUE(s_send_response_called);
  EXPECT_EQ(s_captured_handle, mock_handle);
}

TEST_F(AndroidEngineTest, OutgoingPlatformMessageWeakPtrSafety) {
  static FlutterDataCallback s_captured_data_callback = nullptr;
  static void* s_captured_user_data = nullptr;

  mock_proc_table_.PlatformMessageCreateResponseHandle =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         FlutterDataCallback data_callback, void* user_data,
         FlutterPlatformMessageResponseHandle** response_out)
      -> FlutterEngineResult {
    s_captured_data_callback = data_callback;
    s_captured_user_data = user_data;
    *response_out =
        reinterpret_cast<FlutterPlatformMessageResponseHandle*>(0x5555);
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto local_jni = std::make_shared<JNIMock>();
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, local_jni);
  AndroidEngine engine(settings_, local_jni, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  const std::string payload = "Hello";
  engine.SendPlatformMessage("flutter/test",
                             reinterpret_cast<const uint8_t*>(payload.data()),
                             payload.size(), 100);

  ASSERT_NE(s_captured_data_callback, nullptr);
  ASSERT_NE(s_captured_user_data, nullptr);

  // Destroy local_jni to expire the weak pointer
  local_jni.reset();

  // Invoking data callback with expired weak pointer should not crash or call
  // JNI
  std::string reply = "Reply";
  s_captured_data_callback(reinterpret_cast<const uint8_t*>(reply.data()),
                           reply.size(), s_captured_user_data);
}

TEST_F(AndroidEngineTest, SemanticsUpdateComplexTreeAndCustomActions) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor =
      std::make_unique<AndroidCompositor>(surface_manager, jni_facade_);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_);
  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));

  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);
  flags.is_button = true;
  flags.is_focused = kFlutterTristateTrue;

  FlutterStringAttribute label_attr = {};
  label_attr.struct_size = sizeof(FlutterStringAttribute);
  label_attr.start = 0;
  label_attr.end = 6;
  label_attr.type = kSpellOut;
  const FlutterStringAttribute* label_attrs[] = {&label_attr};

  FlutterLocaleStringAttribute loc_attr = {};
  loc_attr.struct_size = sizeof(FlutterLocaleStringAttribute);
  loc_attr.locale = "en-US";
  FlutterStringAttribute value_attr = {};
  value_attr.struct_size = sizeof(FlutterStringAttribute);
  value_attr.start = 0;
  value_attr.end = 2;
  value_attr.type = kLocale;
  value_attr.locale = &loc_attr;
  const FlutterStringAttribute* value_attrs[] = {&value_attr};

  int32_t traversal_children[] = {2, 3};
  int32_t hit_test_children[] = {3, 2};
  int32_t custom_actions[] = {101};

  FlutterSemanticsNode2 node = {};
  node.struct_size = sizeof(FlutterSemanticsNode2);
  node.id = 1;
  node.flags2 = &flags;
  node.actions = kFlutterSemanticsActionTap;
  node.label = "Button";
  node.label_attribute_count = 1;
  node.label_attributes = label_attrs;
  node.value = "42";
  node.value_attribute_count = 1;
  node.value_attributes = value_attrs;
  node.child_count = 2;
  node.children_in_traversal_order = traversal_children;
  node.children_in_hit_test_order = hit_test_children;
  node.custom_accessibility_actions_count = 1;
  node.custom_accessibility_actions = custom_actions;
  node.rect = FlutterRect{0.0, 0.0, 100.0, 50.0};
  node.transform =
      FlutterTransformation{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  node.hit_test_transform =
      FlutterTransformation{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};

  FlutterSemanticsCustomAction2 action = {};
  action.struct_size = sizeof(FlutterSemanticsCustomAction2);
  action.id = 101;
  action.override_action = kFlutterSemanticsActionTap;
  action.label = "CustomAction";
  action.hint = "CustomHint";

  FlutterSemanticsNode2* nodes[] = {&node};
  FlutterSemanticsCustomAction2* actions[] = {&action};

  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = nodes;
  update.custom_action_count = 1;
  update.custom_actions = actions;

  EXPECT_CALL(*jni_facade_, FlutterViewUpdateSemantics(_, _, _))
      .WillOnce([](std::vector<uint8_t> buffer,
                   std::vector<std::string> strings,
                   std::vector<std::vector<uint8_t>> string_attribute_args) {
        EXPECT_FALSE(buffer.empty());
        EXPECT_GE(strings.size(), 2u);
        EXPECT_GE(string_attribute_args.size(), 1u);
      });

  EXPECT_CALL(*jni_facade_, FlutterViewUpdateCustomAccessibilityActions(_, _))
      .WillOnce([](std::vector<uint8_t> actions_buffer,
                   std::vector<std::string> action_strings) {
        EXPECT_FALSE(actions_buffer.empty());
        EXPECT_EQ(action_strings.size(), 2u);
      });

  engine.HandleSemanticsUpdate(&update);
}

TEST_F(AndroidEngineTest, CustomTaskRunnersInLaunch) {
  fml::Thread platform_thread("platform_thread");
  fml::Thread raster_thread("raster_thread");

  auto platform_task_runner = platform_thread.GetTaskRunner();
  auto raster_task_runner = raster_thread.GetTaskRunner();

  static bool s_platform_runs_called = false;
  static bool s_raster_runs_called = false;
  static bool s_platform_post_called = false;
  static bool s_raster_post_called = false;

  mock_proc_table_.Initialize =
      [](size_t version, const FlutterRendererConfig* config,
         const FlutterProjectArgs* args, void* user_data,
         FLUTTER_API_SYMBOL(FlutterEngine) *
             engine_out) -> FlutterEngineResult {
    EXPECT_NE(args->custom_task_runners, nullptr);
    EXPECT_NE(args->custom_task_runners->platform_task_runner, nullptr);
    EXPECT_NE(args->custom_task_runners->render_task_runner, nullptr);

    auto* plat_desc = args->custom_task_runners->platform_task_runner;
    s_platform_runs_called =
        plat_desc->runs_task_on_current_thread_callback(plat_desc->user_data);

    auto* rast_desc = args->custom_task_runners->render_task_runner;
    s_raster_runs_called =
        rast_desc->runs_task_on_current_thread_callback(rast_desc->user_data);

    FlutterTask task = {nullptr, 2};
    plat_desc->post_task_callback(task, 0, plat_desc->user_data);
    s_platform_post_called = true;

    rast_desc->post_task_callback(task, 0, rast_desc->user_data);
    s_raster_post_called = true;

    *engine_out = reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(0x1234);
    return kSuccess;
  };

  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto compositor = std::make_unique<AndroidCompositor>(
      surface_manager, jni_facade_, raster_task_runner, platform_task_runner);
  AndroidEngine engine(settings_, jni_facade_, surface_manager,
                       std::move(compositor), &mock_proc_table_,
                       platform_task_runner, raster_task_runner);

  EXPECT_TRUE(engine.Launch(nullptr, "", "", {}, 0));
  EXPECT_TRUE(s_platform_runs_called);
  EXPECT_TRUE(s_raster_runs_called);
  EXPECT_TRUE(s_platform_post_called);
  EXPECT_TRUE(s_raster_post_called);
}

struct AndroidEngineMatrixParam {
  AndroidRenderingAPI rendering_api;
  const char* name;
};

class AndroidEngineParameterizedTest
    : public AndroidEngineTest,
      public ::testing::WithParamInterface<AndroidEngineMatrixParam> {};

TEST_P(AndroidEngineParameterizedTest, MatrixLifecycleAndSurfaceTransitions) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->IsValid());

  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));
  EXPECT_TRUE(engine->IsRunning());

  auto native_window = fml::MakeRefCounted<AndroidNativeWindow>(
      reinterpret_cast<ANativeWindow*>(0x1234));
  engine->OnSurfaceCreated(native_window);
  engine->OnSurfaceWindowChanged(native_window);
  engine->OnSurfaceResized(1080, 1920);
  engine->OnSurfaceDestroyed();

  EXPECT_TRUE(engine->IsRunning());
}

TEST_P(AndroidEngineParameterizedTest, MatrixPlatformMessages) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));

  std::vector<uint8_t> data = {1, 2, 3, 4};
  engine->SendPlatformMessage("test_channel", data.data(), data.size(), 100);

  engine->SendPlatformMessageResponse(100, data.data(), data.size());
  engine->CompletePlatformMessageEmptyResponse(100);
}

TEST_P(AndroidEngineParameterizedTest, MatrixPointerEvents) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));

  std::vector<uint8_t> buffer(288, 0);
  int64_t change = 4;  // down
  memcpy(buffer.data() + 16, &change, sizeof(int64_t));

  engine->DispatchPointerDataPacket(buffer.data(), buffer.size());
}

TEST_P(AndroidEngineParameterizedTest, MatrixSemanticsAndAccessibility) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));

  engine->SetSemanticsEnabled(true);
  engine->SetAccessibilityFeatures(0x7);
  engine->DispatchSemanticsAction(1, 1, nullptr, 0);
}

TEST_P(AndroidEngineParameterizedTest, MatrixTextures) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));

  engine->RegisterExternalTexture(10);
  engine->MarkTextureFrameAvailable(10);
  engine->UnregisterTexture(10);
}

TEST_P(AndroidEngineParameterizedTest, MatrixDeferredLibraryLoading) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));

  mock_proc_table_.LoadDartDeferredLibrary =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine, intptr_t loading_unit_id,
         const uint8_t* data, size_t data_length, const uint8_t* instructions,
         size_t instructions_length) -> FlutterEngineResult {
    return kSuccess;
  };

  mock_proc_table_.LoadDartDeferredLibraryError =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine, intptr_t loading_unit_id,
         const char* error_message,
         bool is_transient) -> FlutterEngineResult { return kSuccess; };

  std::vector<uint8_t> dummy_data = {1, 2, 3};
  auto data_mapping = std::make_unique<fml::NonOwnedMapping>(dummy_data.data(),
                                                             dummy_data.size());
  auto inst_mapping = std::make_unique<fml::NonOwnedMapping>(dummy_data.data(),
                                                             dummy_data.size());

  EXPECT_TRUE(engine->LoadDartDeferredLibrary(1, std::move(data_mapping),
                                              std::move(inst_mapping)));
  EXPECT_TRUE(
      engine->LoadDartDeferredLibraryError(2, "Deferred load error", true));
}

TEST_P(AndroidEngineParameterizedTest, MatrixScreenshot) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));

  mock_proc_table_.Screenshot = [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                   FlutterEngineScreenshotType type,
                                   bool compressed,
                                   FlutterEngineScreenshotCallback callback,
                                   void* user_data) -> FlutterEngineResult {
    FlutterEngineScreenshotInfo info = {};
    info.struct_size = sizeof(FlutterEngineScreenshotInfo);
    info.width = 100;
    info.height = 100;
    info.data_size = 40000;
    std::vector<uint8_t> fake_pixels(40000, 0xFF);
    info.data = fake_pixels.data();
    if (callback) {
      callback(&info, user_data);
    }
    return kSuccess;
  };

  bool callback_invoked = false;
  EXPECT_TRUE(engine->Screenshot(
      kFlutterEngineScreenshotTypeUncompressedImage, false,
      [](const FlutterEngineScreenshotInfo* info, void* user_data) {
        *static_cast<bool*>(user_data) = true;
      },
      &callback_invoked));
  EXPECT_TRUE(callback_invoked);
}

TEST_P(AndroidEngineParameterizedTest, MatrixViewportMetrics) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));

  static bool s_metrics_called = false;
  s_metrics_called = false;

  mock_proc_table_.SendWindowMetricsEvent =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
         const FlutterWindowMetricsEvent* event) -> FlutterEngineResult {
    s_metrics_called = true;
    EXPECT_EQ(event->width, 1080u);
    EXPECT_EQ(event->height, 1920u);
    EXPECT_EQ(event->pixel_ratio, 2.0);
    return kSuccess;
  };

  AndroidViewportMetrics metrics = {};
  metrics.physical_width = 1080;
  metrics.physical_height = 1920;
  metrics.device_pixel_ratio = 2.0;

  engine->SetViewportMetrics(0, metrics);
  EXPECT_TRUE(s_metrics_called);
}

TEST_P(AndroidEngineParameterizedTest, MatrixLowMemoryAndScheduleFrame) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));

  static bool s_low_memory_called = false;
  static bool s_schedule_frame_called = false;
  s_low_memory_called = false;
  s_schedule_frame_called = false;

  mock_proc_table_.NotifyLowMemoryWarning =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine) -> FlutterEngineResult {
    s_low_memory_called = true;
    return kSuccess;
  };

  mock_proc_table_.ScheduleFrame = [](FLUTTER_API_SYMBOL(FlutterEngine)
                                          engine) -> FlutterEngineResult {
    s_schedule_frame_called = true;
    return kSuccess;
  };

  engine->NotifyLowMemoryWarning();
  EXPECT_TRUE(s_low_memory_called);

  engine->ScheduleFrame();
  EXPECT_TRUE(s_schedule_frame_called);
}

TEST_P(AndroidEngineParameterizedTest, MatrixSpawning) {
  const auto& param = GetParam();
  auto engine = CreateEngine(param.rendering_api);
  EXPECT_TRUE(engine->Launch(nullptr, "", "", {}, 0));

  auto spawned = engine->Spawn(jni_facade_, "", "", "", {}, 0);
  EXPECT_NE(spawned, nullptr);
  EXPECT_TRUE(spawned->IsValid());
}

INSTANTIATE_TEST_SUITE_P(
    AndroidEngineMatrix,
    AndroidEngineParameterizedTest,
    ::testing::Values(
        AndroidEngineMatrixParam{AndroidRenderingAPI::kImpellerOpenGLES,
                                 "ImpellerOpenGLES"},
        AndroidEngineMatrixParam{AndroidRenderingAPI::kImpellerVulkan,
                                 "ImpellerVulkan"},
        AndroidEngineMatrixParam{AndroidRenderingAPI::kSkiaOpenGLES,
                                 "SkiaOpenGLES"},
        AndroidEngineMatrixParam{AndroidRenderingAPI::kImpellerAutoselect,
                                 "ImpellerAutoselect"},
        AndroidEngineMatrixParam{AndroidRenderingAPI::kSoftware, "Software"}),
    [](const ::testing::TestParamInfo<AndroidEngineMatrixParam>& info) {
      return info.param.name;
    });

}  // namespace testing
}  // namespace flutter
