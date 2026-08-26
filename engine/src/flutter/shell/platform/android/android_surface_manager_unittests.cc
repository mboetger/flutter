// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_manager.h"

#include <memory>
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

TEST(AndroidSurfaceManagerTest, NullAndInvalidArgs) {
  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidSurfaceManager manager(context);

  FlutterBackingStore backing_store = {};
  EXPECT_FALSE(manager.CreateBackingStore(nullptr, &backing_store));

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  // Dimensions 100x200 for test surface configuration
  config.size.width = 100;
  config.size.height = 200;

  EXPECT_FALSE(manager.CreateBackingStore(&config, nullptr));
  EXPECT_FALSE(manager.CollectBackingStore(nullptr));

  AndroidSurfaceManager null_context_manager(nullptr);
  EXPECT_FALSE(
      null_context_manager.CreateBackingStore(&config, &backing_store));
}

TEST(AndroidSurfaceManagerTest, CreateOpenGLBackingStoreAndRecycle) {
  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidSurfaceManager manager(context);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  // Target width 300 and height 600 for test surface allocation
  config.size.width = 300;
  config.size.height = 600;

  FlutterBackingStore backing_store1 = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config, &backing_store1));

  EXPECT_EQ(backing_store1.struct_size, sizeof(FlutterBackingStore));
  EXPECT_EQ(backing_store1.type, kFlutterBackingStoreTypeOpenGL);
  EXPECT_EQ(backing_store1.open_gl.type, kFlutterOpenGLTargetTypeFramebuffer);
  // Sized format 0x8058 (GL_RGBA8)
  EXPECT_EQ(backing_store1.open_gl.framebuffer.target, 0x8058u);
  EXPECT_NE(backing_store1.open_gl.framebuffer.user_data, nullptr);
  EXPECT_TRUE(backing_store1.did_update);

  EXPECT_EQ(manager.GetPoolSize(), 1u);
  EXPECT_EQ(manager.GetInUseCount(), 1u);

  // Release backing store
  EXPECT_TRUE(manager.CollectBackingStore(&backing_store1));
  EXPECT_EQ(manager.GetPoolSize(), 1u);
  EXPECT_EQ(manager.GetInUseCount(), 0u);

  // Requesting same size should recycle the existing record
  FlutterBackingStore backing_store2 = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config, &backing_store2));
  EXPECT_EQ(manager.GetPoolSize(), 1u);
  EXPECT_EQ(manager.GetInUseCount(), 1u);
  EXPECT_EQ(backing_store1.user_data, backing_store2.user_data);

  // Requesting different size creates a second record
  FlutterBackingStoreConfig config2 = config;
  // Second surface dimensions 400x800
  config2.size.width = 400;
  config2.size.height = 800;

  FlutterBackingStore backing_store3 = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config2, &backing_store3));
  EXPECT_EQ(manager.GetPoolSize(), 2u);
  EXPECT_EQ(manager.GetInUseCount(), 2u);
}

TEST(AndroidSurfaceManagerTest, CreateVulkanBackingStore) {
  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerVulkan);
  AndroidSurfaceManager manager(context);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  // Test surface dimensions 500x500
  config.size.width = 500;
  config.size.height = 500;

  FlutterBackingStore backing_store = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config, &backing_store));

  EXPECT_EQ(backing_store.struct_size, sizeof(FlutterBackingStore));
  EXPECT_EQ(backing_store.type, kFlutterBackingStoreTypeVulkan);
  EXPECT_EQ(backing_store.vulkan.struct_size,
            sizeof(FlutterVulkanBackingStore));
  ASSERT_NE(backing_store.vulkan.image, nullptr);
  // VK_FORMAT_R8G8B8A8_UNORM = 44
  EXPECT_EQ(backing_store.vulkan.image->format, 44u);
  EXPECT_NE(backing_store.vulkan.user_data, nullptr);

  EXPECT_EQ(manager.GetPoolSize(), 1u);
  EXPECT_EQ(manager.GetInUseCount(), 1u);

  EXPECT_TRUE(manager.CollectBackingStore(&backing_store));
  EXPECT_EQ(manager.GetInUseCount(), 0u);
}

#if !SLIMPELLER
TEST(AndroidSurfaceManagerTest, CreateSoftwareBackingStore) {
  auto context =
      std::make_shared<MockAndroidContext>(AndroidRenderingAPI::kSoftware);
  AndroidSurfaceManager manager(context);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  // Test surface dimensions 200x100
  config.size.width = 200;
  config.size.height = 100;

  FlutterBackingStore backing_store = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config, &backing_store));

  EXPECT_EQ(backing_store.struct_size, sizeof(FlutterBackingStore));
  EXPECT_EQ(backing_store.type, kFlutterBackingStoreTypeSoftware2);
  EXPECT_EQ(backing_store.software2.struct_size,
            sizeof(FlutterSoftwareBackingStore2));
  ASSERT_NE(backing_store.software2.allocation, nullptr);
  // 4 bytes per pixel * 200 pixels = 800 bytes row stride
  EXPECT_EQ(backing_store.software2.row_bytes, 800u);
  EXPECT_EQ(backing_store.software2.height, 100u);
  EXPECT_EQ(backing_store.software2.pixel_format,
            kFlutterSoftwarePixelFormatRGBA8888);

  EXPECT_EQ(manager.GetPoolSize(), 1u);
  EXPECT_EQ(manager.GetInUseCount(), 1u);

  EXPECT_TRUE(manager.CollectBackingStore(&backing_store));
  EXPECT_EQ(manager.GetInUseCount(), 0u);
}
#endif  // !SLIMPELLER

TEST(AndroidSurfaceManagerTest, TrimAndClearBackingStores) {
  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidSurfaceManager manager(context);

  FlutterBackingStoreConfig config1 = {};
  config1.struct_size = sizeof(config1);
  config1.size.width = 100;
  config1.size.height = 100;

  FlutterBackingStoreConfig config2 = {};
  config2.struct_size = sizeof(config2);
  config2.size.width = 200;
  config2.size.height = 200;

  FlutterBackingStore store1 = {};
  FlutterBackingStore store2 = {};

  ASSERT_TRUE(manager.CreateBackingStore(&config1, &store1));
  ASSERT_TRUE(manager.CreateBackingStore(&config2, &store2));
  EXPECT_EQ(manager.GetPoolSize(), 2u);
  EXPECT_EQ(manager.GetInUseCount(), 2u);

  // Release store1 only
  EXPECT_TRUE(manager.CollectBackingStore(&store1));
  EXPECT_EQ(manager.GetPoolSize(), 2u);
  EXPECT_EQ(manager.GetInUseCount(), 1u);

  // Trim should remove store1 (not in use) and retain store2 (in use)
  manager.TrimBackingStores();
  EXPECT_EQ(manager.GetPoolSize(), 1u);
  EXPECT_EQ(manager.GetInUseCount(), 1u);

  // Clear should remove everything
  manager.ClearBackingStores();
  EXPECT_EQ(manager.GetPoolSize(), 0u);
  EXPECT_EQ(manager.GetInUseCount(), 0u);
}

TEST(AndroidSurfaceManagerTest, SurfaceLifecycleAndTeardown) {
  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidSurfaceManager manager(context);

  // Surface resize to 1080x1920
  EXPECT_TRUE(manager.OnScreenSurfaceResize(DlISize(1080, 1920)));

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  config.size.width = 1080;
  config.size.height = 1920;

  FlutterBackingStore store = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config, &store));
  EXPECT_EQ(manager.GetPoolSize(), 1u);

  manager.Teardown();
  EXPECT_EQ(manager.GetPoolSize(), 0u);
}

}  // namespace android
}  // namespace testing
}  // namespace flutter
