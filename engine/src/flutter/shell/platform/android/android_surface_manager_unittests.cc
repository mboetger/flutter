// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <cmath>
#include <limits>
#include <thread>
#include <vector>

#include "flutter/shell/platform/android/android_surface_manager.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(AndroidSurfaceManagerTest, NativeWindowLifecycle) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kSoftware);
  EXPECT_FALSE(manager.HasNativeWindow());
  EXPECT_EQ(manager.GetNativeWindow().get(), nullptr);
  auto empty_size = manager.GetSurfaceSize();
  EXPECT_EQ(empty_size.width, 0.0);
  EXPECT_EQ(empty_size.height, 0.0);

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  manager.SetNativeWindow(window);
  EXPECT_TRUE(manager.HasNativeWindow());
  EXPECT_EQ(manager.GetNativeWindow().get(), window.get());

  manager.ClearNativeWindow();
  EXPECT_FALSE(manager.HasNativeWindow());
  EXPECT_EQ(manager.GetNativeWindow().get(), nullptr);
}

TEST(AndroidSurfaceManagerTest, SurfaceLifecycleDefense) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);

  // Early make current calls before surface attachment must fail gracefully.
  EXPECT_FALSE(manager.MakeCurrent());
  EXPECT_FALSE(manager.SwapBuffers());

  // Background resource context isolation is always safe.
  EXPECT_TRUE(manager.MakeResourceCurrent());
  EXPECT_TRUE(manager.ClearResourceCurrent());

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  manager.SetNativeWindow(window);

  EXPECT_TRUE(manager.MakeCurrent());
  EXPECT_TRUE(manager.ClearCurrent());
  EXPECT_TRUE(manager.SwapBuffers());

  manager.ClearNativeWindow();
  EXPECT_FALSE(manager.MakeCurrent());
  EXPECT_FALSE(manager.SwapBuffers());
}

TEST(AndroidSurfaceManagerTest, FeatureFlagGating) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerVulkan);

  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  EXPECT_TRUE(manager.IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(manager.IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(manager.IsEmbedderAPIEnabled());

  FlutterMain::ResetEmbedderAPIEnabledForTesting();
}

TEST(AndroidSurfaceManagerTest, SoftwareBackingStorePoolAndRecycle) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kSoftware,
                                /*max_cached_backing_stores=*/2);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  manager.SetNativeWindow(window);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{100, 200};

  FlutterBackingStore store1 = {};
  EXPECT_TRUE(manager.CreateBackingStore(config, &store1));
  EXPECT_EQ(store1.type, kFlutterBackingStoreTypeSoftware);
  EXPECT_NE(store1.software.allocation, nullptr);
  EXPECT_EQ(store1.software.row_bytes, 100U * 4);
  EXPECT_EQ(store1.software.height, 200U);
  EXPECT_EQ(manager.GetAllocatedBackingStoreCount(), 1U);
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0U);

  // Return store1 to the pool.
  EXPECT_TRUE(manager.CollectBackingStore(&store1));
  EXPECT_EQ(manager.GetAllocatedBackingStoreCount(), 1U);
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1U);

  // Requesting the same size must recycle store1.
  FlutterBackingStore store2 = {};
  EXPECT_TRUE(manager.CreateBackingStore(config, &store2));
  EXPECT_EQ(store2.user_data, store1.user_data);
  EXPECT_EQ(manager.GetAllocatedBackingStoreCount(), 1U);
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0U);

  // Requesting a different size allocates a new backing store.
  FlutterBackingStoreConfig config_diff = {};
  config_diff.struct_size = sizeof(FlutterBackingStoreConfig);
  config_diff.size = FlutterSize{300, 400};

  FlutterBackingStore store3 = {};
  EXPECT_TRUE(manager.CreateBackingStore(config_diff, &store3));
  EXPECT_NE(store3.user_data, store2.user_data);
  EXPECT_EQ(manager.GetAllocatedBackingStoreCount(), 2U);
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0U);

  EXPECT_TRUE(manager.CollectBackingStore(&store2));
  EXPECT_TRUE(manager.CollectBackingStore(&store3));
  EXPECT_EQ(manager.GetAllocatedBackingStoreCount(), 2U);
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 2U);

  manager.ClearBackingStoreCache();
  EXPECT_EQ(manager.GetAllocatedBackingStoreCount(), 0U);
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0U);
}

TEST(AndroidSurfaceManagerTest, OpenGLBackingStoreCreation) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  manager.SetNativeWindow(window);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{800, 600};

  FlutterBackingStore store = {};
  EXPECT_TRUE(manager.CreateBackingStore(config, &store));
  EXPECT_EQ(store.type, kFlutterBackingStoreTypeOpenGL);
  EXPECT_EQ(store.open_gl.type, kFlutterOpenGLTargetTypeFramebuffer);
  EXPECT_GT(store.open_gl.framebuffer.name, 0U);
  EXPECT_EQ(store.open_gl.framebuffer.target, 0x8058U);  // GL_RGBA8

  EXPECT_TRUE(manager.CollectBackingStore(&store));
}

TEST(AndroidSurfaceManagerTest, VulkanBackingStoreCreation) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerVulkan);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  manager.SetNativeWindow(window);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{1080, 1920};

  FlutterBackingStore store = {};
  EXPECT_TRUE(manager.CreateBackingStore(config, &store));
  EXPECT_EQ(store.type, kFlutterBackingStoreTypeVulkan);
  EXPECT_EQ(store.vulkan.struct_size, sizeof(FlutterVulkanBackingStore));
  ASSERT_NE(store.vulkan.image, nullptr);
  EXPECT_EQ(store.vulkan.image->struct_size, sizeof(FlutterVulkanImage));
  EXPECT_NE(store.vulkan.image->image, 0U);
  EXPECT_EQ(store.vulkan.image->width, 1080U);
  EXPECT_EQ(store.vulkan.image->height, 1920U);
  EXPECT_EQ(store.vulkan.image->format, 0x8058U);

  EXPECT_TRUE(manager.CollectBackingStore(&store));
}

TEST(AndroidSurfaceManagerTest, ResizeCacheEvictionAndAntiPoisoning) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kSoftware,
                                /*max_cached_backing_stores=*/2);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  manager.SetNativeWindow(window);

  FlutterBackingStoreConfig config_a = {};
  config_a.struct_size = sizeof(FlutterBackingStoreConfig);
  config_a.size = FlutterSize{100, 100};

  FlutterBackingStore store_a1 = {};
  FlutterBackingStore store_a2 = {};
  EXPECT_TRUE(manager.CreateBackingStore(config_a, &store_a1));
  EXPECT_TRUE(manager.CreateBackingStore(config_a, &store_a2));
  EXPECT_TRUE(manager.CollectBackingStore(&store_a1));
  EXPECT_TRUE(manager.CollectBackingStore(&store_a2));

  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 2U);

  // Resize to new resolution B.
  FlutterBackingStoreConfig config_b = {};
  config_b.struct_size = sizeof(FlutterBackingStoreConfig);
  config_b.size = FlutterSize{200, 200};

  FlutterBackingStore store_b = {};
  EXPECT_TRUE(manager.CreateBackingStore(config_b, &store_b));
  EXPECT_TRUE(manager.CollectBackingStore(&store_b));

  // Stale entry A should have been evicted to make room for active entry B.
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 2U);

  // Re-requesting size B should reuse the cached size B buffer.
  FlutterBackingStore store_b2 = {};
  EXPECT_TRUE(manager.CreateBackingStore(config_b, &store_b2));
  EXPECT_EQ(store_b2.user_data, store_b.user_data);
  EXPECT_TRUE(manager.CollectBackingStore(&store_b2));
}

TEST(AndroidSurfaceManagerTest, ClearCacheWhileBackingStoreInUse) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  manager.SetNativeWindow(window);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{400, 300};

  FlutterBackingStore store = {};
  EXPECT_TRUE(manager.CreateBackingStore(config, &store));
  EXPECT_EQ(manager.GetAllocatedBackingStoreCount(), 1U);
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0U);

  // Simulate surface destruction on platform thread while rasterizer is
  // rendering.
  manager.ClearNativeWindow();

  // In-use backing store should not be freed prematurely (UAF defense).
  EXPECT_EQ(manager.GetAllocatedBackingStoreCount(), 1U);

  // When rasterizer finishes and collects the backing store, it should be
  // deleted cleanly.
  EXPECT_TRUE(manager.CollectBackingStore(&store));
  EXPECT_EQ(manager.GetAllocatedBackingStoreCount(), 0U);
}

TEST(AndroidSurfaceManagerTest, InvalidArgumentAndDimensionValidation) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kSoftware);

  FlutterBackingStore store = {};
  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{100, 100};

  // Null backing store out.
  EXPECT_FALSE(manager.CreateBackingStore(config, nullptr));

  // Invalid config struct size.
  FlutterBackingStoreConfig bad_config = config;
  bad_config.struct_size = 0;
  EXPECT_FALSE(manager.CreateBackingStore(bad_config, &store));

  // Zero or negative dimensions.
  bad_config = config;
  bad_config.size = FlutterSize{0, 100};
  EXPECT_FALSE(manager.CreateBackingStore(bad_config, &store));

  bad_config.size = FlutterSize{100, -1};
  EXPECT_FALSE(manager.CreateBackingStore(bad_config, &store));

  // NaN and Infinity dimensions.
  bad_config.size = FlutterSize{std::numeric_limits<double>::quiet_NaN(), 100};
  EXPECT_FALSE(manager.CreateBackingStore(bad_config, &store));

  bad_config.size = FlutterSize{100, std::numeric_limits<double>::infinity()};
  EXPECT_FALSE(manager.CreateBackingStore(bad_config, &store));

  // Excessively large dimensions (overflow guard).
  bad_config.size = FlutterSize{100000.0, 100000.0};
  EXPECT_FALSE(manager.CreateBackingStore(bad_config, &store));

  // Null collection pointer or invalid store.
  EXPECT_FALSE(manager.CollectBackingStore(nullptr));

  FlutterBackingStore bad_store = {};
  EXPECT_FALSE(manager.CollectBackingStore(&bad_store));
}

TEST(AndroidSurfaceManagerTest, ConcurrentBackingStoreAllocation) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kSoftware,
                                /*max_cached_backing_stores=*/8);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  manager.SetNativeWindow(window);

  constexpr int kNumThreads = 4;
  constexpr int kIterations = 25;
  std::vector<std::thread> workers;

  for (int t = 0; t < kNumThreads; ++t) {
    workers.emplace_back([&manager, t]() {
      for (int i = 0; i < kIterations; ++i) {
        FlutterBackingStoreConfig config = {};
        config.struct_size = sizeof(FlutterBackingStoreConfig);
        config.size = FlutterSize{100.0 + (t % 2) * 50.0, 200.0};

        FlutterBackingStore store = {};
        EXPECT_TRUE(manager.CreateBackingStore(config, &store));
        EXPECT_TRUE(manager.CollectBackingStore(&store));
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  EXPECT_LE(manager.GetAllocatedBackingStoreCount(),
            static_cast<size_t>(kNumThreads * 2));
}

}  // namespace testing
}  // namespace flutter
