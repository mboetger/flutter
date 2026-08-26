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

  const uint8_t dummy_data[] = {0, 1, 2, 3};
  engine.OnPlatformViewDispatchPointerDataPacket(dummy_data,
                                                 sizeof(dummy_data));
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
  engine.OnPlatformViewDispatchSemanticsAction(
      0, 42, kFlutterSemanticsActionTap, std::move(empty_args));
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

  double bounds[] = {0, 0, 1080, 50};
  int32_t types[] = {1};
  int32_t states[] = {2};

  FlutterWindowMetricsEvent metrics = {};
  metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics.width = 1080;
  metrics.height = 1920;
  metrics.pixel_ratio = 2.5;
  metrics.physical_view_inset_top = 48.0;
  metrics.physical_view_inset_bottom = 96.0;
  metrics.display_id = 1;
  metrics.has_constraints = true;
  metrics.min_width_constraint = 500;
  metrics.max_width_constraint = 1080;
  metrics.min_height_constraint = 500;
  metrics.max_height_constraint = 1920;
  metrics.display_features_count = 1;
  metrics.display_features_bounds = bounds;
  metrics.display_features_type = types;
  metrics.display_features_state = states;

  // Viewport metrics caching with display features deep-copy.
  engine.OnPlatformViewSetViewportMetrics(metrics);

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
