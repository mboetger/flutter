// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/platform/android/android_engine.h"

#include <memory>
#include "flutter/common/settings.h"
#include "flutter/fml/message_loop.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

class MockPlatformMessageResponse : public PlatformMessageResponse {
 public:
  static fml::RefPtr<MockPlatformMessageResponse> Create() {
    return fml::AdoptRef(new MockPlatformMessageResponse());
  }

  void Complete(std::unique_ptr<fml::Mapping> data) override {
    is_completed_ = true;
    if (data) {
      completed_data_ = std::string(
          reinterpret_cast<const char*>(data->GetMapping()), data->GetSize());
    }
  }

  void CompleteEmpty() override {
    is_completed_ = true;
    completed_data_ = "";
  }

  bool is_completed() const { return is_completed_; }
  const std::string& completed_data() const { return completed_data_; }

 private:
  MockPlatformMessageResponse() = default;
  bool is_completed_ = false;
  std::string completed_data_;
};

TEST(AndroidEngineTest, InitializeAndLifecycle) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = true;
  auto jni = std::make_shared<JNIMock>();

  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kSoftware);

  EXPECT_EQ(engine.GetSettings().enable_software_rendering, true);
  EXPECT_NE(engine.GetAndroidCompositor(), nullptr);
  EXPECT_TRUE(engine.GetPlatformView());
  EXPECT_FALSE(engine.IsSurfaceControlEnabled());

  engine.UpdateDisplayMetrics();
  engine.NotifyLowMemoryWarning();
}

TEST(AndroidEngineTest, PointerEventDispatching) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = true;
  auto jni = std::make_shared<JNIMock>();

  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kSoftware);

  auto packet = std::make_unique<flutter::PointerDataPacket>(2);
  flutter::PointerData p1 = {};
  p1.Clear();
  p1.time_stamp = 1000;
  p1.change = PointerData::Change::kDown;
  p1.kind = PointerData::DeviceKind::kTouch;
  p1.physical_x = 50.0;
  p1.physical_y = 100.0;
  p1.device = 1;
  p1.pan_x = 10.0;
  p1.pan_y = 20.0;
  p1.scale = 1.5;
  p1.rotation = 0.5;
  p1.view_id = 0;
  p1.pressure = 0.8;
  p1.pressure_min = 0.0;
  p1.pressure_max = 1.0;
  packet->SetPointerData(0, p1);

  flutter::PointerData p2 = {};
  p2.Clear();
  p2.time_stamp = 2000;
  p2.change = PointerData::Change::kUp;
  p2.kind = PointerData::DeviceKind::kTouch;
  p2.physical_x = 60.0;
  p2.physical_y = 110.0;
  p2.device = 1;
  packet->SetPointerData(1, p2);

  // Dispatch pointer data packet.
  engine.OnPlatformViewDispatchPointerDataPacket(std::move(packet));
}

TEST(AndroidEngineTest, PlatformMessageDispatching) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = true;
  auto jni = std::make_shared<JNIMock>();

  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kSoftware);

  const std::string channel = "flutter/test_channel";
  const std::string message_data = "hello world";
  auto mapping =
      fml::MallocMapping::Copy(message_data.data(), message_data.size());
  auto response = MockPlatformMessageResponse::Create();
  auto message = std::make_unique<flutter::PlatformMessage>(
      channel, std::move(mapping), response);

  engine.OnPlatformViewDispatchPlatformMessage(std::move(message));
}

TEST(AndroidEngineTest, SemanticsAndAccessibility) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = true;
  auto jni = std::make_shared<JNIMock>();

  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kSoftware);

  engine.OnPlatformViewSetSemanticsEnabled(true);
  engine.OnPlatformViewSetAccessibilityFeatures(3);

  fml::MallocMapping empty_args;
  engine.OnPlatformViewDispatchSemanticsAction(0, 42, SemanticsAction::kTap,
                                               std::move(empty_args));
}

TEST(AndroidEngineTest, ExternalTextures) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = true;
  auto jni = std::make_shared<JNIMock>();

  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kSoftware);

  engine.OnPlatformViewMarkTextureFrameAvailable(101);
  engine.OnPlatformViewUnregisterTexture(101);
}

TEST(AndroidEngineTest, DartDeferredLibrary) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = true;
  auto jni = std::make_shared<JNIMock>();

  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kSoftware);

  engine.LoadDartDeferredLibrary(1, nullptr, nullptr);
  engine.LoadDartDeferredLibraryError(1, "Not found", false);
}

TEST(AndroidEngineTest, ViewportMetricsAndFrameScheduling) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = true;
  auto jni = std::make_shared<JNIMock>();

  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kSoftware);

  ViewportMetrics metrics;
  metrics.physical_width = 1080;
  metrics.physical_height = 1920;
  metrics.device_pixel_ratio = 2.5;
  metrics.physical_view_inset_top = 48.0;
  metrics.physical_view_inset_bottom = 96.0;
  metrics.display_id = 1;
  metrics.physical_min_width_constraint = 500;
  metrics.physical_max_width_constraint = 1080;
  metrics.physical_min_height_constraint = 500;
  metrics.physical_max_height_constraint = 1920;
  engine.OnPlatformViewSetViewportMetrics(0, metrics);

  engine.OnPlatformViewScheduleFrame();
  engine.OnPlatformViewSetNextFrameCallback([]() {});
}

TEST(AndroidEngineTest, SpawnEngine) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = true;
  auto jni = std::make_shared<JNIMock>();

  AndroidEngine engine(settings, jni, AndroidRenderingAPI::kSoftware);

  auto spawned = engine.Spawn(jni, "main", "", "/", {}, 2);
  // Spawn without initialized AOT snapshot in unit test environment returns
  // nullptr or engine. We verify that Spawn runs safely without crashing.
  if (spawned) {
    EXPECT_TRUE(spawned->IsValid());
    EXPECT_NE(spawned->GetAndroidCompositor(), nullptr);
  }
}

TEST(AndroidEngineTest, MultiBackendValidation) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_embedder_api = true;
  auto jni = std::make_shared<JNIMock>();

  // 1. Software backend
  {
    settings.enable_software_rendering = true;
    AndroidEngine engine_sw(settings, jni, AndroidRenderingAPI::kSoftware);
    EXPECT_EQ(engine_sw.GetSettings().enable_software_rendering, true);
    EXPECT_NE(engine_sw.GetAndroidCompositor(), nullptr);
    EXPECT_TRUE(engine_sw.GetPlatformView());
  }

  // 2. Impeller Vulkan backend
  {
    settings.enable_software_rendering = false;
    AndroidEngine engine_vk(settings, jni,
                            AndroidRenderingAPI::kImpellerVulkan);
    EXPECT_EQ(engine_vk.GetSettings().enable_software_rendering, false);
    EXPECT_NE(engine_vk.GetAndroidCompositor(), nullptr);
    EXPECT_TRUE(engine_vk.GetPlatformView());
  }

  // 3. Impeller OpenGLES backend
  {
    settings.enable_software_rendering = false;
    AndroidEngine engine_gl(settings, jni,
                            AndroidRenderingAPI::kImpellerOpenGLES);
    EXPECT_EQ(engine_gl.GetSettings().enable_software_rendering, false);
    EXPECT_NE(engine_gl.GetAndroidCompositor(), nullptr);
    EXPECT_TRUE(engine_gl.GetPlatformView());
  }
}

}  // namespace testing
}  // namespace flutter
