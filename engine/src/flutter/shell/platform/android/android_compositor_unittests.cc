// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <memory>
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {
namespace android {

class MockAndroidContext : public AndroidContext {
 public:
  explicit MockAndroidContext(AndroidRenderingAPI rendering_api)
      : AndroidContext(rendering_api) {}

  ~MockAndroidContext() override = default;

  bool IsValid() const override { return true; }
};

TEST(AndroidCompositorTest, GetCompositorInitializesCorrectly) {
  auto context =
      std::make_shared<MockAndroidContext>(AndroidRenderingAPI::kImpellerOpenGLES);
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(context);
  AndroidCompositor compositor(surface_manager, nullptr);

  FlutterCompositor c_compositor = compositor.GetCompositor();
  EXPECT_EQ(c_compositor.struct_size, sizeof(FlutterCompositor));
  EXPECT_EQ(c_compositor.user_data, &compositor);
  EXPECT_NE(c_compositor.create_backing_store_callback, nullptr);
  EXPECT_NE(c_compositor.collect_backing_store_callback, nullptr);
  EXPECT_NE(c_compositor.present_view_callback, nullptr);
}

TEST(AndroidCompositorTest, CreateAndCollectBackingStoreViaCallbacks) {
  auto context =
      std::make_shared<MockAndroidContext>(AndroidRenderingAPI::kImpellerOpenGLES);
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(context);
  AndroidCompositor compositor(surface_manager, nullptr);

  FlutterCompositor c_compositor = compositor.GetCompositor();

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  // Dimensions 640x480 for compositor test frame
  config.size.width = 640;
  config.size.height = 480;

  FlutterBackingStore store = {};
  ASSERT_TRUE(c_compositor.create_backing_store_callback(
      &config, &store, c_compositor.user_data));

  EXPECT_EQ(store.type, kFlutterBackingStoreTypeOpenGL);
  EXPECT_EQ(surface_manager->GetPoolSize(), 1u);
  EXPECT_EQ(surface_manager->GetInUseCount(), 1u);

  ASSERT_TRUE(c_compositor.collect_backing_store_callback(
      &store, c_compositor.user_data));
  EXPECT_EQ(surface_manager->GetInUseCount(), 0u);
}

TEST(AndroidCompositorTest, PresentLayersWithBackingStoresAndPlatformViews) {
  auto context =
      std::make_shared<MockAndroidContext>(AndroidRenderingAPI::kImpellerOpenGLES);
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(context);
  AndroidCompositor compositor(surface_manager, nullptr);

  FlutterCompositor c_compositor = compositor.GetCompositor();

  FlutterBackingStore backing_store = {};
  backing_store.struct_size = sizeof(backing_store);
  backing_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(platform_view);
  // Platform view identifier 42
  platform_view.identifier = 42;

  FlutterLayer layer1 = {};
  layer1.struct_size = sizeof(layer1);
  layer1.type = kFlutterLayerContentTypeBackingStore;
  layer1.backing_store = &backing_store;
  // Presentation timestamp 1000000 nanoseconds
  layer1.presentation_time = 1000000;

  FlutterLayer layer2 = {};
  layer2.struct_size = sizeof(layer2);
  layer2.type = kFlutterLayerContentTypePlatformView;
  layer2.platform_view = &platform_view;

  const FlutterLayer* layers[] = {&layer1, &layer2};

  FlutterPresentViewInfo info = {};
  info.struct_size = sizeof(info);
  // View ID 1
  info.view_id = 1;
  info.layers = layers;
  // Count of 2 layers
  info.layers_count = 2;
  info.user_data = &compositor;

  ASSERT_TRUE(c_compositor.present_view_callback(&info));

  AndroidCompositor::PresentedFrame frame =
      compositor.GetLastPresentedFrame();
  EXPECT_EQ(frame.view_id, 1);
  EXPECT_EQ(frame.backing_store_count, 1u);
  EXPECT_EQ(frame.platform_view_count, 1u);
  ASSERT_EQ(frame.platform_view_ids.size(), 1u);
  EXPECT_EQ(frame.platform_view_ids[0], 42);
  EXPECT_EQ(frame.presentation_time, 1000000u);
}

TEST(AndroidCompositorTest, ViewRegistration) {
  auto context =
      std::make_shared<MockAndroidContext>(AndroidRenderingAPI::kImpellerOpenGLES);
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(context);
  AndroidCompositor compositor(surface_manager, nullptr);

  EXPECT_EQ(compositor.GetViewCount(), 0u);

  compositor.AddView(1);
  compositor.AddView(2);
  EXPECT_EQ(compositor.GetViewCount(), 2u);

  compositor.RemoveView(1);
  EXPECT_EQ(compositor.GetViewCount(), 1u);

  compositor.RemoveView(2);
  EXPECT_EQ(compositor.GetViewCount(), 0u);
}

TEST(AndroidCompositorTest, NullSafety) {
  AndroidCompositor compositor(nullptr, nullptr);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  config.size.width = 100;
  config.size.height = 100;

  FlutterBackingStore store = {};
  EXPECT_FALSE(compositor.CreateBackingStore(&config, &store));
  EXPECT_FALSE(compositor.CollectBackingStore(&store));

  FlutterCompositor c_compositor = compositor.GetCompositor();
  EXPECT_FALSE(
      c_compositor.create_backing_store_callback(&config, &store, nullptr));
  EXPECT_FALSE(
      c_compositor.collect_backing_store_callback(&store, nullptr));
  EXPECT_FALSE(c_compositor.present_view_callback(nullptr));
}

}  // namespace android
}  // namespace testing
}  // namespace flutter
