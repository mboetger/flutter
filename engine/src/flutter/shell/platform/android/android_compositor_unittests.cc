// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <cmath>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "flutter/fml/thread.h"
#include "flutter/shell/platform/android/android_compositor.h"
#include "flutter/shell/platform/android/android_surface_manager.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(AndroidCompositorTest, GetCompositorConfig) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  FlutterCompositor config = compositor.GetCompositorConfig();
  EXPECT_EQ(config.struct_size, sizeof(FlutterCompositor));
  EXPECT_EQ(config.user_data, &compositor);
  EXPECT_NE(config.create_backing_store_callback, nullptr);
  EXPECT_NE(config.collect_backing_store_callback, nullptr);
  EXPECT_NE(config.present_view_callback, nullptr);
  EXPECT_FALSE(config.avoid_backing_store_cache);
}

TEST(AndroidCompositorTest, FeatureFlagGating) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  EXPECT_FALSE(compositor.IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(compositor.IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(compositor.IsEmbedderAPIEnabled());

  FlutterMain::ResetEmbedderAPIEnabledForTesting();
}

TEST(AndroidCompositorTest, CreateAndCollectBackingStore) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  surface_manager->SetNativeWindow(window);

  AndroidCompositor compositor(surface_manager);
  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{320, 240};
  config.view_id = 0;

  FlutterBackingStore store = {};
  EXPECT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                      callbacks.user_data));
  EXPECT_EQ(store.type, kFlutterBackingStoreTypeSoftware);
  EXPECT_NE(store.software.allocation, nullptr);
  EXPECT_EQ(store.software.height, 240U);
  EXPECT_EQ(store.software.row_bytes, 320U * 4);

  EXPECT_TRUE(
      callbacks.collect_backing_store_callback(&store, callbacks.user_data));
}

TEST(AndroidCompositorTest, PresentBackingStoreLayers) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  surface_manager->SetNativeWindow(window);

  AndroidCompositor compositor(surface_manager);
  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{400, 300};
  config.view_id = 0;

  FlutterBackingStore store = {};
  ASSERT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                      callbacks.user_data));

  FlutterLayer layer = {};
  layer.struct_size = sizeof(FlutterLayer);
  layer.type = kFlutterLayerContentTypeBackingStore;
  layer.backing_store = &store;
  layer.offset = FlutterPoint{0, 0};
  layer.size = FlutterSize{400, 300};

  const FlutterLayer* layers[] = {&layer};

  FlutterPresentViewInfo present_info = {};
  present_info.struct_size = sizeof(FlutterPresentViewInfo);
  present_info.view_id = 0;
  present_info.layers = layers;
  present_info.layers_count = 1;
  present_info.user_data = callbacks.user_data;

  EXPECT_EQ(compositor.GetPresentCount(), 0U);
  EXPECT_TRUE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(compositor.GetPresentCount(), 1U);

  // Present a second frame.
  EXPECT_TRUE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(compositor.GetPresentCount(), 2U);

  EXPECT_TRUE(
      callbacks.collect_backing_store_callback(&store, callbacks.user_data));
}

TEST(AndroidCompositorTest, SurfaceLifecycleDefense) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);
  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{100, 100};
  config.view_id = 0;

  FlutterBackingStore store = {};
  ASSERT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                      callbacks.user_data));

  FlutterLayer layer = {};
  layer.struct_size = sizeof(FlutterLayer);
  layer.type = kFlutterLayerContentTypeBackingStore;
  layer.backing_store = &store;
  layer.offset = FlutterPoint{0, 0};
  layer.size = FlutterSize{100, 100};

  const FlutterLayer* layers[] = {&layer};

  FlutterPresentViewInfo present_info = {};
  present_info.struct_size = sizeof(FlutterPresentViewInfo);
  present_info.view_id = 0;
  present_info.layers = layers;
  present_info.layers_count = 1;
  present_info.user_data = callbacks.user_data;

  // Presentation must fail gracefully before surface is attached.
  EXPECT_FALSE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(compositor.GetPresentCount(), 0U);

  // Attach surface.
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  compositor.OnSurfaceCreated(window);

  EXPECT_TRUE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(compositor.GetPresentCount(), 1U);

  // Detach surface.
  compositor.OnSurfaceDestroyed();

  // Subsequent presentation must fail gracefully.
  EXPECT_FALSE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(compositor.GetPresentCount(), 1U);

  EXPECT_TRUE(
      callbacks.collect_backing_store_callback(&store, callbacks.user_data));
}

TEST(AndroidCompositorTest, SynchronousSurfaceDetachBarrierWithRasterThread) {
  fml::Thread raster_thread("AndroidCompositorRasterThread");
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager,
                               /*jni_facade=*/nullptr,
                               raster_thread.GetTaskRunner());

  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);

  // Call OnSurfaceCreated from platform thread.
  compositor.OnSurfaceCreated(window);
  EXPECT_TRUE(surface_manager->HasNativeWindow());

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{200, 200};
  config.view_id = 0;

  FlutterBackingStore store = {};
  ASSERT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                      callbacks.user_data));

  FlutterLayer layer = {};
  layer.struct_size = sizeof(FlutterLayer);
  layer.type = kFlutterLayerContentTypeBackingStore;
  layer.backing_store = &store;
  layer.offset = FlutterPoint{0, 0};
  layer.size = FlutterSize{200, 200};

  const FlutterLayer* layers[] = {&layer};

  FlutterPresentViewInfo present_info = {};
  present_info.struct_size = sizeof(FlutterPresentViewInfo);
  present_info.view_id = 0;
  present_info.layers = layers;
  present_info.layers_count = 1;
  present_info.user_data = callbacks.user_data;

  EXPECT_TRUE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(compositor.GetPresentCount(), 1U);

  // Trigger synchronous surface detach barrier from platform thread.
  compositor.OnSurfaceDestroyed();

  // Surface manager must immediately report HasNativeWindow == false.
  EXPECT_FALSE(surface_manager->HasNativeWindow());

  // Subsequent frame presentation must fail gracefully.
  EXPECT_FALSE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(compositor.GetPresentCount(), 1U);

  EXPECT_TRUE(
      callbacks.collect_backing_store_callback(&store, callbacks.user_data));
}

TEST(AndroidCompositorTest, SurfaceWindowChangedAndResized) {
  fml::Thread raster_thread("AndroidCompositorRasterThread");
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager,
                               /*jni_facade=*/nullptr,
                               raster_thread.GetTaskRunner());

  auto window1 = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  auto window2 = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);

  compositor.OnSurfaceCreated(window1);
  EXPECT_EQ(surface_manager->GetNativeWindow().get(), window1.get());

  compositor.OnSurfaceWindowChanged(window2);
  EXPECT_EQ(surface_manager->GetNativeWindow().get(), window2.get());

  // Allocate and cache backing store.
  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{150, 150};
  config.view_id = 0;

  FlutterBackingStore store = {};
  ASSERT_TRUE(compositor.CreateBackingStore(&config, &store));
  ASSERT_TRUE(compositor.CollectBackingStore(&store));
  EXPECT_EQ(surface_manager->GetCachedBackingStoreCount(), 1U);

  // Resize window.
  compositor.OnSurfaceResized(FlutterSize{300, 300});
  EXPECT_EQ(surface_manager->GetCachedBackingStoreCount(), 0U);

  compositor.OnSurfaceDestroyed();
  EXPECT_FALSE(surface_manager->HasNativeWindow());
}

TEST(AndroidCompositorTest, PlatformViewLayerPresentation) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  surface_manager->SetNativeWindow(window);

  AndroidCompositor compositor(surface_manager);
  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{500, 500};
  config.view_id = 0;

  FlutterBackingStore store = {};
  ASSERT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                      callbacks.user_data));

  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(FlutterPlatformView);
  platform_view.identifier = 42;

  FlutterLayer backing_layer = {};
  backing_layer.struct_size = sizeof(FlutterLayer);
  backing_layer.type = kFlutterLayerContentTypeBackingStore;
  backing_layer.backing_store = &store;
  backing_layer.offset = FlutterPoint{0, 0};
  backing_layer.size = FlutterSize{500, 500};

  FlutterLayer platform_view_layer = {};
  platform_view_layer.struct_size = sizeof(FlutterLayer);
  platform_view_layer.type = kFlutterLayerContentTypePlatformView;
  platform_view_layer.platform_view = &platform_view;
  platform_view_layer.offset = FlutterPoint{10, 20};
  platform_view_layer.size = FlutterSize{200, 150};

  const FlutterLayer* layers[] = {&backing_layer, &platform_view_layer};

  FlutterPresentViewInfo present_info = {};
  present_info.struct_size = sizeof(FlutterPresentViewInfo);
  present_info.view_id = 0;
  present_info.layers = layers;
  present_info.layers_count = 2;
  present_info.user_data = callbacks.user_data;

  int rendered_view_id = -1;
  size_t rendered_layer_index = 999;
  compositor.SetPlatformViewRendererCallback(
      [&](const FlutterPlatformView* pv, const FlutterLayer& layer,
          size_t index) -> bool {
        rendered_view_id = pv->identifier;
        rendered_layer_index = index;
        return true;
      });

  EXPECT_TRUE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(rendered_view_id, 42);
  EXPECT_EQ(rendered_layer_index, 1U);
  EXPECT_EQ(compositor.GetPresentCount(), 1U);

  // Test failure in platform view renderer causes PresentView to fail.
  compositor.SetPlatformViewRendererCallback(
      [](const FlutterPlatformView* pv, const FlutterLayer& layer,
         size_t index) -> bool { return false; });

  EXPECT_FALSE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(compositor.GetPresentCount(), 1U);

  EXPECT_TRUE(
      callbacks.collect_backing_store_callback(&store, callbacks.user_data));
}

TEST(AndroidCompositorTest, InvalidArgumentsAndStructSizes) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  surface_manager->SetNativeWindow(window);

  AndroidCompositor compositor(surface_manager);
  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  // Null present_info.
  EXPECT_FALSE(callbacks.present_view_callback(nullptr));

  // Null user_data.
  FlutterPresentViewInfo bad_present = {};
  bad_present.struct_size = sizeof(FlutterPresentViewInfo);
  bad_present.user_data = nullptr;
  EXPECT_FALSE(callbacks.present_view_callback(&bad_present));

  // Invalid struct_size on present_info.
  bad_present.user_data = callbacks.user_data;
  bad_present.struct_size = 0;
  EXPECT_FALSE(callbacks.present_view_callback(&bad_present));

  // Null layers with layers_count > 0.
  bad_present.struct_size = sizeof(FlutterPresentViewInfo);
  bad_present.layers = nullptr;
  bad_present.layers_count = 1;
  EXPECT_FALSE(callbacks.present_view_callback(&bad_present));

  // Layer array containing null.
  const FlutterLayer* null_layer_array[] = {nullptr};
  bad_present.layers = null_layer_array;
  bad_present.layers_count = 1;
  EXPECT_FALSE(callbacks.present_view_callback(&bad_present));

  // Layer with invalid struct_size.
  FlutterLayer bad_layer = {};
  bad_layer.struct_size = 0;
  const FlutterLayer* bad_layer_array[] = {&bad_layer};
  bad_present.layers = bad_layer_array;
  bad_present.layers_count = 1;
  EXPECT_FALSE(callbacks.present_view_callback(&bad_present));

  // Backing store layer with null backing store.
  FlutterLayer bad_bs_layer = {};
  bad_bs_layer.struct_size = sizeof(FlutterLayer);
  bad_bs_layer.type = kFlutterLayerContentTypeBackingStore;
  bad_bs_layer.backing_store = nullptr;
  const FlutterLayer* bad_bs_layer_array[] = {&bad_bs_layer};
  bad_present.layers = bad_bs_layer_array;
  bad_present.layers_count = 1;
  EXPECT_FALSE(callbacks.present_view_callback(&bad_present));

  // Platform view layer with null platform view.
  FlutterLayer bad_pv_layer = {};
  bad_pv_layer.struct_size = sizeof(FlutterLayer);
  bad_pv_layer.type = kFlutterLayerContentTypePlatformView;
  bad_pv_layer.platform_view = nullptr;
  const FlutterLayer* bad_pv_layer_array[] = {&bad_pv_layer};
  bad_present.layers = bad_pv_layer_array;
  bad_present.layers_count = 1;
  EXPECT_FALSE(callbacks.present_view_callback(&bad_present));

  // Backing store creation with invalid config.
  FlutterBackingStore store = {};
  EXPECT_FALSE(
      callbacks.create_backing_store_callback(nullptr, &store, callbacks.user_data));

  FlutterBackingStoreConfig bad_config = {};
  bad_config.struct_size = 0;
  EXPECT_FALSE(callbacks.create_backing_store_callback(&bad_config, &store,
                                                      callbacks.user_data));

  // Backing store collection with null.
  EXPECT_FALSE(
      callbacks.collect_backing_store_callback(nullptr, callbacks.user_data));

  FlutterBackingStore bad_store = {};
  bad_store.struct_size = 0;
  EXPECT_FALSE(callbacks.collect_backing_store_callback(&bad_store,
                                                       callbacks.user_data));
}

TEST(AndroidCompositorTest, MultithreadedPresentationAndLifecycle) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware,
                                              /*max_cached_backing_stores=*/8);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  surface_manager->SetNativeWindow(window);

  AndroidCompositor compositor(surface_manager);
  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  constexpr int kNumThreads = 4;
  constexpr int kIterations = 20;
  std::vector<std::thread> workers;

  for (int t = 0; t < kNumThreads; ++t) {
    workers.emplace_back([&callbacks, t]() {
      for (int i = 0; i < kIterations; ++i) {
        FlutterBackingStoreConfig config = {};
        config.struct_size = sizeof(FlutterBackingStoreConfig);
        config.size = FlutterSize{100.0 + (t % 2) * 50.0, 200.0};
        config.view_id = 0;

        FlutterBackingStore store = {};
        EXPECT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                            callbacks.user_data));

        FlutterLayer layer = {};
        layer.struct_size = sizeof(FlutterLayer);
        layer.type = kFlutterLayerContentTypeBackingStore;
        layer.backing_store = &store;
        layer.offset = FlutterPoint{0, 0};
        layer.size = config.size;

        const FlutterLayer* layers[] = {&layer};

        FlutterPresentViewInfo present_info = {};
        present_info.struct_size = sizeof(FlutterPresentViewInfo);
        present_info.view_id = 0;
        present_info.layers = layers;
        present_info.layers_count = 1;
        present_info.user_data = callbacks.user_data;

        EXPECT_TRUE(callbacks.present_view_callback(&present_info));
        EXPECT_TRUE(
            callbacks.collect_backing_store_callback(&store, callbacks.user_data));
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  EXPECT_EQ(compositor.GetPresentCount(),
            static_cast<size_t>(kNumThreads * kIterations));
}

TEST(AndroidCompositorTest, InterleavedLayerPresentation) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  surface_manager->SetNativeWindow(window);

  AndroidCompositor compositor(surface_manager);
  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{600, 600};
  config.view_id = 0;

  FlutterBackingStore store_bottom = {};
  FlutterBackingStore store_top = {};
  ASSERT_TRUE(callbacks.create_backing_store_callback(&config, &store_bottom,
                                                      callbacks.user_data));
  ASSERT_TRUE(callbacks.create_backing_store_callback(&config, &store_top,
                                                      callbacks.user_data));

  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(FlutterPlatformView);
  platform_view.identifier = 101;

  FlutterLayer bottom_backing = {};
  bottom_backing.struct_size = sizeof(FlutterLayer);
  bottom_backing.type = kFlutterLayerContentTypeBackingStore;
  bottom_backing.backing_store = &store_bottom;
  bottom_backing.offset = FlutterPoint{0, 0};
  bottom_backing.size = FlutterSize{600, 600};

  FlutterLayer middle_pv = {};
  middle_pv.struct_size = sizeof(FlutterLayer);
  middle_pv.type = kFlutterLayerContentTypePlatformView;
  middle_pv.platform_view = &platform_view;
  middle_pv.offset = FlutterPoint{50, 50};
  middle_pv.size = FlutterSize{200, 200};

  FlutterLayer top_backing = {};
  top_backing.struct_size = sizeof(FlutterLayer);
  top_backing.type = kFlutterLayerContentTypeBackingStore;
  top_backing.backing_store = &store_top;
  top_backing.offset = FlutterPoint{0, 0};
  top_backing.size = FlutterSize{600, 600};

  const FlutterLayer* layers[] = {&bottom_backing, &middle_pv, &top_backing};

  FlutterPresentViewInfo present_info = {};
  present_info.struct_size = sizeof(FlutterPresentViewInfo);
  present_info.view_id = 0;
  present_info.layers = layers;
  present_info.layers_count = 3;
  present_info.user_data = callbacks.user_data;

  std::vector<int> rendered_views;
  std::vector<size_t> rendered_indices;
  compositor.SetPlatformViewRendererCallback(
      [&](const FlutterPlatformView* pv, const FlutterLayer& layer,
          size_t index) -> bool {
        rendered_views.push_back(pv->identifier);
        rendered_indices.push_back(index);
        return true;
      });

  EXPECT_TRUE(callbacks.present_view_callback(&present_info));
  ASSERT_EQ(rendered_views.size(), 1U);
  EXPECT_EQ(rendered_views[0], 101);
  EXPECT_EQ(rendered_indices[0], 1U);
  EXPECT_EQ(compositor.GetPresentCount(), 1U);

  EXPECT_TRUE(callbacks.collect_backing_store_callback(&store_bottom,
                                                       callbacks.user_data));
  EXPECT_TRUE(callbacks.collect_backing_store_callback(&store_top,
                                                       callbacks.user_data));
}

TEST(AndroidCompositorTest, ZeroLayerPresentation) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  surface_manager->SetNativeWindow(window);

  AndroidCompositor compositor(surface_manager);
  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  FlutterPresentViewInfo present_info = {};
  present_info.struct_size = sizeof(FlutterPresentViewInfo);
  present_info.view_id = 0;
  present_info.layers = nullptr;
  present_info.layers_count = 0;
  present_info.user_data = callbacks.user_data;

  EXPECT_TRUE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(compositor.GetPresentCount(), 1U);
}

TEST(AndroidCompositorTest, OpenGLAndVulkanCompositorBackends) {
  // Test OpenGL backend compositor.
  {
    auto gl_manager = std::make_shared<AndroidSurfaceManager>(
        AndroidRenderingAPI::kImpellerOpenGLES);
    auto window = fml::MakeRefCounted<AndroidNativeWindow>(
        nullptr, /*is_fake_window=*/true);
    gl_manager->SetNativeWindow(window);

    AndroidCompositor gl_compositor(gl_manager);
    FlutterCompositor callbacks = gl_compositor.GetCompositorConfig();

    FlutterBackingStoreConfig config = {};
    config.struct_size = sizeof(FlutterBackingStoreConfig);
    config.size = FlutterSize{800, 600};
    config.view_id = 0;

    FlutterBackingStore store = {};
    EXPECT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                        callbacks.user_data));
    EXPECT_EQ(store.type, kFlutterBackingStoreTypeOpenGL);

    FlutterLayer layer = {};
    layer.struct_size = sizeof(FlutterLayer);
    layer.type = kFlutterLayerContentTypeBackingStore;
    layer.backing_store = &store;
    layer.offset = FlutterPoint{0, 0};
    layer.size = FlutterSize{800, 600};

    const FlutterLayer* layers[] = {&layer};
    FlutterPresentViewInfo present_info = {};
    present_info.struct_size = sizeof(FlutterPresentViewInfo);
    present_info.view_id = 0;
    present_info.layers = layers;
    present_info.layers_count = 1;
    present_info.user_data = callbacks.user_data;

    EXPECT_TRUE(callbacks.present_view_callback(&present_info));
    EXPECT_EQ(gl_compositor.GetPresentCount(), 1U);
    EXPECT_TRUE(
        callbacks.collect_backing_store_callback(&store, callbacks.user_data));
  }

  // Test Vulkan backend compositor.
  {
    auto vk_manager = std::make_shared<AndroidSurfaceManager>(
        AndroidRenderingAPI::kImpellerVulkan);
    auto window = fml::MakeRefCounted<AndroidNativeWindow>(
        nullptr, /*is_fake_window=*/true);
    vk_manager->SetNativeWindow(window);

    AndroidCompositor vk_compositor(vk_manager);
    FlutterCompositor callbacks = vk_compositor.GetCompositorConfig();

    FlutterBackingStoreConfig config = {};
    config.struct_size = sizeof(FlutterBackingStoreConfig);
    config.size = FlutterSize{1080, 1920};
    config.view_id = 0;

    FlutterBackingStore store = {};
    EXPECT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                        callbacks.user_data));
    EXPECT_EQ(store.type, kFlutterBackingStoreTypeVulkan);
    ASSERT_NE(store.vulkan.image, nullptr);

    FlutterLayer layer = {};
    layer.struct_size = sizeof(FlutterLayer);
    layer.type = kFlutterLayerContentTypeBackingStore;
    layer.backing_store = &store;
    layer.offset = FlutterPoint{0, 0};
    layer.size = FlutterSize{1080, 1920};

    const FlutterLayer* layers[] = {&layer};
    FlutterPresentViewInfo present_info = {};
    present_info.struct_size = sizeof(FlutterPresentViewInfo);
    present_info.view_id = 0;
    present_info.layers = layers;
    present_info.layers_count = 1;
    present_info.user_data = callbacks.user_data;

    EXPECT_TRUE(callbacks.present_view_callback(&present_info));
    EXPECT_EQ(vk_compositor.GetPresentCount(), 1U);
    EXPECT_TRUE(
        callbacks.collect_backing_store_callback(&store, callbacks.user_data));
  }
}

TEST(AndroidCompositorTest, InvalidLayerGeometryAndResizeBounds) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  surface_manager->SetNativeWindow(window);

  AndroidCompositor compositor(surface_manager);
  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{200, 200};
  config.view_id = 0;

  FlutterBackingStore store = {};
  ASSERT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                      callbacks.user_data));

  // Layer with NaN width.
  FlutterLayer nan_layer = {};
  nan_layer.struct_size = sizeof(FlutterLayer);
  nan_layer.type = kFlutterLayerContentTypeBackingStore;
  nan_layer.backing_store = &store;
  nan_layer.offset = FlutterPoint{0, 0};
  nan_layer.size = FlutterSize{std::numeric_limits<double>::quiet_NaN(), 200};

  const FlutterLayer* nan_layer_array[] = {&nan_layer};
  FlutterPresentViewInfo present_info = {};
  present_info.struct_size = sizeof(FlutterPresentViewInfo);
  present_info.view_id = 0;
  present_info.layers = nan_layer_array;
  present_info.layers_count = 1;
  present_info.user_data = callbacks.user_data;

  EXPECT_FALSE(callbacks.present_view_callback(&present_info));

  // Layer with negative size.
  FlutterLayer neg_layer = {};
  neg_layer.struct_size = sizeof(FlutterLayer);
  neg_layer.type = kFlutterLayerContentTypeBackingStore;
  neg_layer.backing_store = &store;
  neg_layer.offset = FlutterPoint{0, 0};
  neg_layer.size = FlutterSize{200, -10};

  const FlutterLayer* neg_layer_array[] = {&neg_layer};
  present_info.layers = neg_layer_array;
  EXPECT_FALSE(callbacks.present_view_callback(&present_info));

  // Layer with infinite offset.
  FlutterLayer inf_layer = {};
  inf_layer.struct_size = sizeof(FlutterLayer);
  inf_layer.type = kFlutterLayerContentTypeBackingStore;
  inf_layer.backing_store = &store;
  inf_layer.offset =
      FlutterPoint{std::numeric_limits<double>::infinity(), 0};
  inf_layer.size = FlutterSize{200, 200};

  const FlutterLayer* inf_layer_array[] = {&inf_layer};
  present_info.layers = inf_layer_array;
  EXPECT_FALSE(callbacks.present_view_callback(&present_info));

  // OnSurfaceResized invalid bounds.
  compositor.OnSurfaceResized(
      FlutterSize{std::numeric_limits<double>::quiet_NaN(), 100});
  compositor.OnSurfaceResized(FlutterSize{-50, 100});
  compositor.OnSurfaceResized(FlutterSize{0, 0});

  EXPECT_TRUE(
      callbacks.collect_backing_store_callback(&store, callbacks.user_data));
}

}  // namespace testing
}  // namespace flutter
