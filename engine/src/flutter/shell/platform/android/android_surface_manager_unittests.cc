// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_manager.h"

#include <thread>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
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

TEST(AndroidSurfaceManager, NativeWindowLifecycle) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_FALSE(manager.HasNativeWindow());
  EXPECT_EQ(manager.GetNativeWindow().get(), nullptr);

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  manager.SetNativeWindow(window);
  EXPECT_TRUE(manager.HasNativeWindow());
  EXPECT_EQ(manager.GetNativeWindow(), window);

  manager.ClearNativeWindow();
  EXPECT_FALSE(manager.HasNativeWindow());
  EXPECT_EQ(manager.GetNativeWindow().get(), nullptr);
}

#if !SLIMPELLER
TEST(AndroidSurfaceManager, SoftwareBackingStoreCreationAndRecycling) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kSoftware);
  EXPECT_EQ(manager.GetRenderingAPI(), AndroidRenderingAPI::kSoftware);

  constexpr double kWidth = 800.0;
  constexpr double kHeight = 600.0;
  FlutterBackingStoreConfig config = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{kWidth, kHeight},
  };

  FlutterBackingStore backing_store = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config, &backing_store));
  EXPECT_TRUE(backing_store.did_update);
  EXPECT_EQ(backing_store.type, kFlutterBackingStoreTypeSoftware);
  EXPECT_NE(backing_store.software.allocation, nullptr);
  // 4 bytes per pixel for RGBA8888.
  constexpr size_t kBytesPerPixel = 4;
  EXPECT_EQ(backing_store.software.row_bytes,
            static_cast<size_t>(kWidth) * kBytesPerPixel);
  EXPECT_EQ(backing_store.software.height, static_cast<size_t>(kHeight));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0u);

  // Collect and recycle the backing store.
  const void* first_allocation = backing_store.software.allocation;
  ASSERT_TRUE(manager.CollectBackingStore(&backing_store));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);

  // Double collection must fail safely.
  EXPECT_FALSE(manager.CollectBackingStore(&backing_store));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);

  // Allocate again with matching dimensions — must reuse cached allocation.
  FlutterBackingStore second_store = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config, &second_store));
  EXPECT_EQ(second_store.software.allocation, first_allocation);
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0u);

  // Clean up.
  ASSERT_TRUE(manager.CollectBackingStore(&second_store));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);
  manager.ClearBackingStorePool();
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0u);
}
#endif  // !SLIMPELLER

TEST(AndroidSurfaceManager, OpenGLBackingStoreCreationAndRecycling) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_EQ(manager.GetRenderingAPI(), AndroidRenderingAPI::kImpellerOpenGLES);

  FlutterBackingStoreConfig config = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{1080.0, 1920.0},
  };

  FlutterBackingStore store = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config, &store));
  EXPECT_TRUE(store.did_update);
  EXPECT_EQ(store.type, kFlutterBackingStoreTypeOpenGL);
  EXPECT_EQ(store.open_gl.type, kFlutterOpenGLTargetTypeFramebuffer);
  // Internal framebuffer format: GL_RGBA8 = 0x8058.
  constexpr uint32_t kGLFramebufferFormatRGBA8 = 0x8058;
  EXPECT_EQ(store.open_gl.framebuffer.target, kGLFramebufferFormatRGBA8);
  EXPECT_EQ(store.open_gl.framebuffer.name, 0u);

  ASSERT_TRUE(manager.CollectBackingStore(&store));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);

  // Double collection must fail safely.
  EXPECT_FALSE(manager.CollectBackingStore(&store));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);

  manager.ClearBackingStorePool();
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0u);
}

TEST(AndroidSurfaceManager, VulkanBackingStoreCreationAndRecycling) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerVulkan);
  EXPECT_EQ(manager.GetRenderingAPI(), AndroidRenderingAPI::kImpellerVulkan);

  constexpr double kWidth = 1440.0;
  constexpr double kHeight = 2560.0;
  FlutterBackingStoreConfig config = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{kWidth, kHeight},
  };

  FlutterBackingStore store = {};
  ASSERT_TRUE(manager.CreateBackingStore(&config, &store));
  EXPECT_TRUE(store.did_update);
  EXPECT_EQ(store.type, kFlutterBackingStoreTypeVulkan);
  ASSERT_NE(store.vulkan.image, nullptr);
  EXPECT_EQ(store.vulkan.image->width, static_cast<size_t>(kWidth));
  EXPECT_EQ(store.vulkan.image->height, static_cast<size_t>(kHeight));

  ASSERT_TRUE(manager.CollectBackingStore(&store));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);

  // Double collection must fail safely.
  EXPECT_FALSE(manager.CollectBackingStore(&store));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);

  manager.ClearBackingStorePool();
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0u);
}

TEST(AndroidSurfaceManager, SizeMatchedCachingDistinctSizes) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);

  FlutterBackingStoreConfig size_a = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{100.0, 100.0},
  };
  FlutterBackingStoreConfig size_b = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{200.0, 200.0},
  };

  FlutterBackingStore store_a = {};
  FlutterBackingStore store_b = {};
  ASSERT_TRUE(manager.CreateBackingStore(&size_a, &store_a));
  ASSERT_TRUE(manager.CreateBackingStore(&size_b, &store_b));

  ASSERT_TRUE(manager.CollectBackingStore(&store_a));
  ASSERT_TRUE(manager.CollectBackingStore(&store_b));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 2u);

  // Allocate for size_a: should fetch cached size_a.
  FlutterBackingStore store_a2 = {};
  ASSERT_TRUE(manager.CreateBackingStore(&size_a, &store_a2));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);

  // Allocate for size_b: should fetch cached size_b.
  FlutterBackingStore store_b2 = {};
  ASSERT_TRUE(manager.CreateBackingStore(&size_b, &store_b2));
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0u);

  ASSERT_TRUE(manager.CollectBackingStore(&store_a2));
  ASSERT_TRUE(manager.CollectBackingStore(&store_b2));
}

TEST(AndroidSurfaceManager, PoolTrimming) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);

  FlutterBackingStoreConfig config = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{300.0, 300.0},
  };

  // Create 5 backing stores of the same size and collect them all.
  constexpr size_t kTotalAllocations = 5;
  std::vector<FlutterBackingStore> stores(kTotalAllocations);
  for (size_t i = 0; i < kTotalAllocations; ++i) {
    ASSERT_TRUE(manager.CreateBackingStore(&config, &stores[i]));
  }
  for (size_t i = 0; i < kTotalAllocations; ++i) {
    ASSERT_TRUE(manager.CollectBackingStore(&stores[i]));
  }
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), kTotalAllocations);

  // Trim using default constant (retains 2 per size).
  manager.TrimBackingStorePool();
  EXPECT_EQ(manager.GetCachedBackingStoreCount(),
            AndroidSurfaceManager::kDefaultMaxCachedPerSize);

  manager.ClearBackingStorePool();
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0u);
}

TEST(AndroidSurfaceManager, ArgumentValidation) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);

  FlutterBackingStore store = {};
  FlutterBackingStoreConfig valid_config = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{100.0, 100.0},
  };

  // Null config.
  EXPECT_FALSE(manager.CreateBackingStore(nullptr, &store));

  // Invalid struct size on config.
  FlutterBackingStoreConfig invalid_config = valid_config;
  invalid_config.struct_size = sizeof(FlutterBackingStoreConfig) - 1;
  EXPECT_FALSE(manager.CreateBackingStore(&invalid_config, &store));

  // Null store output.
  EXPECT_FALSE(manager.CreateBackingStore(&valid_config, nullptr));

  // Zero dimensions.
  FlutterBackingStoreConfig zero_config = valid_config;
  zero_config.size = FlutterSize{0.0, 100.0};
  EXPECT_FALSE(manager.CreateBackingStore(&zero_config, &store));

  // Null collect arguments.
  EXPECT_FALSE(manager.CollectBackingStore(nullptr));

  // Invalid struct size on collect.
  FlutterBackingStore invalid_store = {};
  invalid_store.struct_size = sizeof(FlutterBackingStore) - 1;
  EXPECT_FALSE(manager.CollectBackingStore(&invalid_store));

  // Null user data on collect.
  FlutterBackingStore null_user_data_store = {};
  null_user_data_store.struct_size = sizeof(FlutterBackingStore);
  null_user_data_store.user_data = nullptr;
  EXPECT_FALSE(manager.CollectBackingStore(&null_user_data_store));
}

TEST(AndroidSurfaceManager, MultiThreadedConcurrentCreationAndCollection) {
  AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);

  constexpr size_t kNumThreads = 4;
  constexpr size_t kIterationsPerThread = 50;

  std::vector<std::thread> workers;
  workers.reserve(kNumThreads);

  for (size_t t = 0; t < kNumThreads; ++t) {
    workers.emplace_back([&manager, t]() {
      // Alternate width/height per thread.
      const double width = 100.0 + static_cast<double>(t * 10);
      const double height = 100.0 + static_cast<double>(t * 10);
      FlutterBackingStoreConfig config = {
          .struct_size = sizeof(FlutterBackingStoreConfig),
          .size = FlutterSize{width, height},
      };

      for (size_t i = 0; i < kIterationsPerThread; ++i) {
        FlutterBackingStore store = {};
        EXPECT_TRUE(manager.CreateBackingStore(&config, &store));
        EXPECT_TRUE(manager.CollectBackingStore(&store));
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  EXPECT_GT(manager.GetCachedBackingStoreCount(), 0u);
  manager.ClearBackingStorePool();
  EXPECT_EQ(manager.GetCachedBackingStoreCount(), 0u);
}

TEST(AndroidSurfaceManager, FeatureFlagGatingDualPathValidation) {
  ScopedEmbedderAPIOverrideReset reset_on_exit;

  Settings settings;

  // Path 1: Feature flag enabled.
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled(settings));
  {
    AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);
    FlutterBackingStoreConfig config = {
        .struct_size = sizeof(FlutterBackingStoreConfig),
        .size = FlutterSize{400.0, 400.0},
    };
    FlutterBackingStore store = {};
    EXPECT_TRUE(manager.CreateBackingStore(&config, &store));
    EXPECT_TRUE(manager.CollectBackingStore(&store));
    EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);
  }

  // Path 2: Feature flag disabled.
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled(settings));
  {
    AndroidSurfaceManager manager(AndroidRenderingAPI::kImpellerOpenGLES);
    FlutterBackingStoreConfig config = {
        .struct_size = sizeof(FlutterBackingStoreConfig),
        .size = FlutterSize{400.0, 400.0},
    };
    FlutterBackingStore store = {};
    EXPECT_TRUE(manager.CreateBackingStore(&config, &store));
    EXPECT_TRUE(manager.CollectBackingStore(&store));
    EXPECT_EQ(manager.GetCachedBackingStoreCount(), 1u);
  }
}

}  // namespace testing
}  // namespace flutter
