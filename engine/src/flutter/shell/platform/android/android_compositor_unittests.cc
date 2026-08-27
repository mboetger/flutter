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

//------------------------------------------------------------------------------
// Phase 3.3: Direct JNI Platform View Mutator Mapping & DPR Normalization Tests
//------------------------------------------------------------------------------

TEST(AndroidCompositorMutationTest, MutatorMappingOpacity) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeOpacity;
  mutation.opacity = 0.75;

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView pv = {};
  pv.struct_size = sizeof(FlutterPlatformView);
  pv.identifier = 1;
  pv.mutations_count = 1;
  pv.mutations = mutations;

  AndroidPlatformViewMutatorsStack stack;
  EXPECT_TRUE(compositor.PopulateMutatorsStack(&pv, &stack));
  ASSERT_EQ(stack.Size(), 1U);
  EXPECT_EQ(stack.GetMutators()[0].type, AndroidMutatorType::kOpacity);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].opacity, 0.75f);

  // Test opacity clamping [0.0, 1.0].
  mutation.opacity = 1.5;
  EXPECT_TRUE(compositor.PopulateMutatorsStack(&pv, &stack));
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].opacity, 1.0f);

  mutation.opacity = -0.5;
  EXPECT_TRUE(compositor.PopulateMutatorsStack(&pv, &stack));
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].opacity, 0.0f);
}

TEST(AndroidCompositorMutationTest, MutatorMappingClipRect) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipRect;
  mutation.clip_rect = FlutterRect{10.0, 20.0, 110.0, 120.0};

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView pv = {};
  pv.struct_size = sizeof(FlutterPlatformView);
  pv.identifier = 2;
  pv.mutations_count = 1;
  pv.mutations = mutations;

  AndroidPlatformViewMutatorsStack stack;
  EXPECT_TRUE(compositor.PopulateMutatorsStack(&pv, &stack));
  ASSERT_EQ(stack.Size(), 1U);
  EXPECT_EQ(stack.GetMutators()[0].type, AndroidMutatorType::kClipRect);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.left, 10.0f);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.top, 20.0f);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.right, 110.0f);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.bottom, 120.0f);
}

TEST(AndroidCompositorMutationTest, MutatorMappingClipRRect) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipRoundedRect;
  mutation.clip_rounded_rect.rect = FlutterRect{0.0, 0.0, 200.0, 100.0};
  mutation.clip_rounded_rect.upper_left_corner_radius = FlutterSize{1.0, 2.0};
  mutation.clip_rounded_rect.upper_right_corner_radius = FlutterSize{3.0, 4.0};
  mutation.clip_rounded_rect.lower_right_corner_radius = FlutterSize{5.0, 6.0};
  mutation.clip_rounded_rect.lower_left_corner_radius = FlutterSize{7.0, 8.0};

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView pv = {};
  pv.struct_size = sizeof(FlutterPlatformView);
  pv.identifier = 3;
  pv.mutations_count = 1;
  pv.mutations = mutations;

  AndroidPlatformViewMutatorsStack stack;
  EXPECT_TRUE(compositor.PopulateMutatorsStack(&pv, &stack));
  ASSERT_EQ(stack.Size(), 1U);
  EXPECT_EQ(stack.GetMutators()[0].type, AndroidMutatorType::kClipRRect);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.right, 200.0f);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.bottom, 100.0f);

  const float expected_radii[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  for (int i = 0; i < 8; ++i) {
    EXPECT_FLOAT_EQ(stack.GetMutators()[0].radii[i], expected_radii[i]);
  }
}

TEST(AndroidCompositorMutationTest, MutatorMappingClipRSE) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipRoundedSuperellipse;
  mutation.clip_rounded_superellipse.rect = FlutterRect{5.0, 10.0, 250.0, 150.0};
  mutation.clip_rounded_superellipse.upper_left_corner_radius =
      FlutterSize{10.0, 12.0};
  mutation.clip_rounded_superellipse.upper_right_corner_radius =
      FlutterSize{14.0, 16.0};
  mutation.clip_rounded_superellipse.lower_right_corner_radius =
      FlutterSize{18.0, 20.0};
  mutation.clip_rounded_superellipse.lower_left_corner_radius =
      FlutterSize{22.0, 24.0};

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView pv = {};
  pv.struct_size = sizeof(FlutterPlatformView);
  pv.identifier = 4;
  pv.mutations_count = 1;
  pv.mutations = mutations;

  AndroidPlatformViewMutatorsStack stack;
  EXPECT_TRUE(compositor.PopulateMutatorsStack(&pv, &stack));
  ASSERT_EQ(stack.Size(), 1U);
  EXPECT_EQ(stack.GetMutators()[0].type, AndroidMutatorType::kClipRSE);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.left, 5.0f);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.top, 10.0f);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.right, 250.0f);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].rect.bottom, 150.0f);

  const float expected_radii[8] = {10.0f, 12.0f, 14.0f, 16.0f,
                                   18.0f, 20.0f, 22.0f, 24.0f};
  for (int i = 0; i < 8; ++i) {
    EXPECT_FLOAT_EQ(stack.GetMutators()[0].radii[i], expected_radii[i]);
  }
}

TEST(AndroidCompositorMutationTest, MutatorMappingTransformationAndDPRNormalization) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  FlutterTransformation transform = {
      .scaleX = 4.0,
      .skewX = 1.0,
      .transX = 100.0,
      .skewY = 2.0,
      .scaleY = 4.0,
      .transY = 200.0,
      .pers0 = 0.01,
      .pers1 = 0.02,
      .pers2 = 1.0,
  };

  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeTransformation;
  mutation.transformation = transform;

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView pv = {};
  pv.struct_size = sizeof(FlutterPlatformView);
  pv.identifier = 5;
  pv.mutations_count = 1;
  pv.mutations = mutations;

  // Row-major matrix layout in mutators stack.
  AndroidPlatformViewMutatorsStack stack;
  EXPECT_TRUE(compositor.PopulateMutatorsStack(&pv, &stack));
  ASSERT_EQ(stack.Size(), 1U);
  EXPECT_EQ(stack.GetMutators()[0].type, AndroidMutatorType::kTransform);
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].transform_matrix[0], 4.0f);   // scaleX
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].transform_matrix[1], 1.0f);   // skewX
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].transform_matrix[2], 100.0f); // transX
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].transform_matrix[3], 2.0f);   // skewY
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].transform_matrix[4], 4.0f);   // scaleY
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].transform_matrix[5], 200.0f); // transY
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].transform_matrix[6], 0.01f);  // pers0
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].transform_matrix[7], 0.02f);  // pers1
  EXPECT_FLOAT_EQ(stack.GetMutators()[0].transform_matrix[8], 1.0f);   // pers2

  // Test explicit NormalizeRootTransform helper across DPR scales.
  float normalized_dpr2[9] = {};
  AndroidCompositor::NormalizeRootTransform(transform, 2.0, normalized_dpr2);
  EXPECT_FLOAT_EQ(normalized_dpr2[0], 2.0f);   // 4.0 / 2.0
  EXPECT_FLOAT_EQ(normalized_dpr2[1], 0.5f);   // 1.0 / 2.0
  EXPECT_FLOAT_EQ(normalized_dpr2[2], 50.0f);  // 100.0 / 2.0
  EXPECT_FLOAT_EQ(normalized_dpr2[3], 1.0f);   // 2.0 / 2.0
  EXPECT_FLOAT_EQ(normalized_dpr2[4], 2.0f);   // 4.0 / 2.0
  EXPECT_FLOAT_EQ(normalized_dpr2[5], 100.0f); // 200.0 / 2.0
  EXPECT_FLOAT_EQ(normalized_dpr2[6], 0.01f);  // pers0 unscaled
  EXPECT_FLOAT_EQ(normalized_dpr2[7], 0.02f);  // pers1 unscaled
  EXPECT_FLOAT_EQ(normalized_dpr2[8], 1.0f);   // pers2 unscaled

  float normalized_dpr4[9] = {};
  AndroidCompositor::NormalizeRootTransform(transform, 4.0, normalized_dpr4);
  EXPECT_FLOAT_EQ(normalized_dpr4[0], 1.0f);   // 4.0 / 4.0
  EXPECT_FLOAT_EQ(normalized_dpr4[1], 0.25f);  // 1.0 / 4.0
  EXPECT_FLOAT_EQ(normalized_dpr4[2], 25.0f);  // 100.0 / 4.0
  EXPECT_FLOAT_EQ(normalized_dpr4[3], 0.5f);   // 2.0 / 4.0
  EXPECT_FLOAT_EQ(normalized_dpr4[4], 1.0f);   // 4.0 / 4.0
  EXPECT_FLOAT_EQ(normalized_dpr4[5], 50.0f);  // 200.0 / 4.0
}

TEST(AndroidCompositorMutationTest, MutatorMappingClipPath) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  // Note: points[1] and points[2] for Move/Line contain NaN/garbage, verifying
  // that verb-specific validation ignores unused point slots.
  FlutterPathSegment segments[6] = {
      {
          .verb = kFlutterPathVerbMove,
          .points = {FlutterPoint{0, 0},
                     FlutterPoint{std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN()},
                     FlutterPoint{std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN()}},
      },
      {
          .verb = kFlutterPathVerbLine,
          .points = {FlutterPoint{100, 0},
                     FlutterPoint{std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN()},
                     FlutterPoint{std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN()}},
      },
      {
          .verb = kFlutterPathVerbQuad,
          .points = {FlutterPoint{150, 50}, FlutterPoint{100, 100},
                     FlutterPoint{std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN()}},
      },
      {
          .verb = kFlutterPathVerbConic,
          .points = {FlutterPoint{50, 150}, FlutterPoint{0, 100},
                     FlutterPoint{std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN()}},
          .conic_weight = 0.707,
      },
      {
          .verb = kFlutterPathVerbCubic,
          .points = {FlutterPoint{-20, 80}, FlutterPoint{-20, 20},
                     FlutterPoint{0, 0}},
      },
      {
          .verb = kFlutterPathVerbClose,
      },
  };

  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipPath;
  mutation.clip_path.fill_type = kFlutterPathFillTypeEvenOdd;
  mutation.clip_path.segments_count = 6;
  mutation.clip_path.segments = segments;

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView pv = {};
  pv.struct_size = sizeof(FlutterPlatformView);
  pv.identifier = 6;
  pv.mutations_count = 1;
  pv.mutations = mutations;

  AndroidPlatformViewMutatorsStack stack;
  EXPECT_TRUE(compositor.PopulateMutatorsStack(&pv, &stack));
  ASSERT_EQ(stack.Size(), 1U);
  EXPECT_EQ(stack.GetMutators()[0].type, AndroidMutatorType::kClipPath);
  EXPECT_EQ(stack.GetMutators()[0].path.fill_type, kFlutterPathFillTypeEvenOdd);
  ASSERT_EQ(stack.GetMutators()[0].path.segments.size(), 6U);
  EXPECT_EQ(stack.GetMutators()[0].path.segments[0].verb, kFlutterPathVerbMove);
  EXPECT_EQ(stack.GetMutators()[0].path.segments[1].verb, kFlutterPathVerbLine);
  EXPECT_EQ(stack.GetMutators()[0].path.segments[2].verb, kFlutterPathVerbQuad);
  EXPECT_EQ(stack.GetMutators()[0].path.segments[3].verb, kFlutterPathVerbConic);
  EXPECT_DOUBLE_EQ(stack.GetMutators()[0].path.segments[3].conic_weight, 0.707);
  EXPECT_EQ(stack.GetMutators()[0].path.segments[4].verb, kFlutterPathVerbCubic);
  EXPECT_EQ(stack.GetMutators()[0].path.segments[5].verb, kFlutterPathVerbClose);
}

TEST(AndroidCompositorMutationTest, MixedMutatorsStackSequence) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  FlutterPlatformViewMutation mut_transform = {};
  mut_transform.type = kFlutterPlatformViewMutationTypeTransformation;
  mut_transform.transformation = FlutterTransformation{
      .scaleX = 1.0, .scaleY = 1.0, .pers2 = 1.0,
  };

  FlutterPlatformViewMutation mut_clip_rect = {};
  mut_clip_rect.type = kFlutterPlatformViewMutationTypeClipRect;
  mut_clip_rect.clip_rect = FlutterRect{0, 0, 100, 100};

  FlutterPlatformViewMutation mut_opacity = {};
  mut_opacity.type = kFlutterPlatformViewMutationTypeOpacity;
  mut_opacity.opacity = 0.5;

  FlutterPlatformViewMutation mut_clip_rrect = {};
  mut_clip_rrect.type = kFlutterPlatformViewMutationTypeClipRoundedRect;
  mut_clip_rrect.clip_rounded_rect.rect = FlutterRect{10, 10, 80, 80};

  FlutterPlatformViewMutation mut_clip_rse = {};
  mut_clip_rse.type = kFlutterPlatformViewMutationTypeClipRoundedSuperellipse;
  mut_clip_rse.clip_rounded_superellipse.rect = FlutterRect{15, 15, 75, 75};

  const FlutterPlatformViewMutation* mutations[] = {
      &mut_transform, &mut_clip_rect, &mut_opacity, &mut_clip_rrect, &mut_clip_rse};

  FlutterPlatformView pv = {};
  pv.struct_size = sizeof(FlutterPlatformView);
  pv.identifier = 7;
  pv.mutations_count = 5;
  pv.mutations = mutations;

  AndroidPlatformViewMutatorsStack stack;
  EXPECT_TRUE(compositor.PopulateMutatorsStack(&pv, &stack));
  ASSERT_EQ(stack.Size(), 5U);
  EXPECT_EQ(stack.GetMutators()[0].type, AndroidMutatorType::kTransform);
  EXPECT_EQ(stack.GetMutators()[1].type, AndroidMutatorType::kClipRect);
  EXPECT_EQ(stack.GetMutators()[2].type, AndroidMutatorType::kOpacity);
  EXPECT_EQ(stack.GetMutators()[3].type, AndroidMutatorType::kClipRRect);
  EXPECT_EQ(stack.GetMutators()[4].type, AndroidMutatorType::kClipRSE);
}

TEST(AndroidCompositorMutationTest, MutatorMappingInvalidInputs) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  AndroidCompositor compositor(surface_manager);

  AndroidPlatformViewMutatorsStack stack;

  // Null platform view.
  EXPECT_FALSE(compositor.PopulateMutatorsStack(nullptr, &stack));

  // Null stack output.
  FlutterPlatformView pv = {};
  pv.struct_size = sizeof(FlutterPlatformView);
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, nullptr));

  // Null mutations array with non-zero count.
  pv.mutations_count = 2;
  pv.mutations = nullptr;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // Null mutation entry at index.
  const FlutterPlatformViewMutation* null_mutation_array[] = {nullptr};
  pv.mutations = null_mutation_array;
  pv.mutations_count = 1;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // NaN opacity.
  FlutterPlatformViewMutation bad_op = {};
  bad_op.type = kFlutterPlatformViewMutationTypeOpacity;
  bad_op.opacity = std::numeric_limits<double>::quiet_NaN();
  const FlutterPlatformViewMutation* bad_op_array[] = {&bad_op};
  pv.mutations = bad_op_array;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // NaN clip rect bounds.
  FlutterPlatformViewMutation bad_rect = {};
  bad_rect.type = kFlutterPlatformViewMutationTypeClipRect;
  bad_rect.clip_rect = FlutterRect{0, std::numeric_limits<double>::infinity(), 10, 10};
  const FlutterPlatformViewMutation* bad_rect_array[] = {&bad_rect};
  pv.mutations = bad_rect_array;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // Inverted clip rect bounds (right < left).
  FlutterPlatformViewMutation inv_rect = {};
  inv_rect.type = kFlutterPlatformViewMutationTypeClipRect;
  inv_rect.clip_rect = FlutterRect{100, 0, 50, 50};
  const FlutterPlatformViewMutation* inv_rect_array[] = {&inv_rect};
  pv.mutations = inv_rect_array;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // Inverted clip rrect bounds (bottom < top).
  FlutterPlatformViewMutation inv_rrect = {};
  inv_rrect.type = kFlutterPlatformViewMutationTypeClipRoundedRect;
  inv_rrect.clip_rounded_rect.rect = FlutterRect{0, 100, 50, 50};
  const FlutterPlatformViewMutation* inv_rrect_array[] = {&inv_rrect};
  pv.mutations = inv_rrect_array;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // Negative corner radius on clip rrect.
  FlutterPlatformViewMutation bad_rrect = {};
  bad_rrect.type = kFlutterPlatformViewMutationTypeClipRoundedRect;
  bad_rrect.clip_rounded_rect.rect = FlutterRect{0, 0, 10, 10};
  bad_rrect.clip_rounded_rect.upper_left_corner_radius = FlutterSize{-1.0, 5.0};
  const FlutterPlatformViewMutation* bad_rrect_array[] = {&bad_rrect};
  pv.mutations = bad_rrect_array;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // Inverted clip rse bounds.
  FlutterPlatformViewMutation inv_rse = {};
  inv_rse.type = kFlutterPlatformViewMutationTypeClipRoundedSuperellipse;
  inv_rse.clip_rounded_superellipse.rect = FlutterRect{100, 0, 50, 50};
  const FlutterPlatformViewMutation* inv_rse_array[] = {&inv_rse};
  pv.mutations = inv_rse_array;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // Null path segments with count > 0.
  FlutterPlatformViewMutation bad_path = {};
  bad_path.type = kFlutterPlatformViewMutationTypeClipPath;
  bad_path.clip_path.segments_count = 3;
  bad_path.clip_path.segments = nullptr;
  const FlutterPlatformViewMutation* bad_path_array[] = {&bad_path};
  pv.mutations = bad_path_array;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // Invalid conic weight (negative or non-finite).
  FlutterPathSegment bad_conic_seg = {
      .verb = kFlutterPathVerbConic,
      .points = {FlutterPoint{0, 0}, FlutterPoint{10, 10}},
      .conic_weight = -1.0,
  };
  FlutterPlatformViewMutation bad_conic_path = {};
  bad_conic_path.type = kFlutterPlatformViewMutationTypeClipPath;
  bad_conic_path.clip_path.segments_count = 1;
  bad_conic_path.clip_path.segments = &bad_conic_seg;
  const FlutterPlatformViewMutation* bad_conic_array[] = {&bad_conic_path};
  pv.mutations = bad_conic_array;
  EXPECT_FALSE(compositor.PopulateMutatorsStack(&pv, &stack));

  // Standalone stack methods: PushOpacity NaN test, PushClipRSE test.
  AndroidPlatformViewMutatorsStack direct_stack;
  direct_stack.PushOpacity(std::numeric_limits<float>::quiet_NaN());
  ASSERT_EQ(direct_stack.Size(), 1U);
  EXPECT_FLOAT_EQ(direct_stack.GetMutators()[0].opacity, 1.0f);

  float radii[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  direct_stack.PushClipRSE(0, 0, 10, 10, radii);
  ASSERT_EQ(direct_stack.Size(), 2U);
  EXPECT_EQ(direct_stack.GetMutators()[1].type, AndroidMutatorType::kClipRSE);
}

TEST(AndroidCompositorMutationTest, PresentWithMutatorsStackCallback) {
  auto surface_manager =
      std::make_shared<AndroidSurfaceManager>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  surface_manager->SetNativeWindow(window);

  AndroidCompositor compositor(surface_manager);
  compositor.SetDevicePixelRatio(2.0);
  EXPECT_DOUBLE_EQ(compositor.GetDevicePixelRatio(), 2.0);

  FlutterCompositor callbacks = compositor.GetCompositorConfig();

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(FlutterBackingStoreConfig);
  config.size = FlutterSize{400, 400};
  config.view_id = 0;

  FlutterBackingStore store = {};
  ASSERT_TRUE(callbacks.create_backing_store_callback(&config, &store,
                                                      callbacks.user_data));

  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeOpacity;
  mutation.opacity = 0.8;

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(FlutterPlatformView);
  platform_view.identifier = 77;
  platform_view.mutations_count = 1;
  platform_view.mutations = mutations;

  FlutterLayer backing_layer = {};
  backing_layer.struct_size = sizeof(FlutterLayer);
  backing_layer.type = kFlutterLayerContentTypeBackingStore;
  backing_layer.backing_store = &store;
  backing_layer.offset = FlutterPoint{0, 0};
  backing_layer.size = FlutterSize{400, 400};

  FlutterLayer pv_layer = {};
  pv_layer.struct_size = sizeof(FlutterLayer);
  pv_layer.type = kFlutterLayerContentTypePlatformView;
  pv_layer.platform_view = &platform_view;
  pv_layer.offset = FlutterPoint{20, 30};
  pv_layer.size = FlutterSize{200, 100};

  const FlutterLayer* layers[] = {&backing_layer, &pv_layer};
  FlutterPresentViewInfo present_info = {};
  present_info.struct_size = sizeof(FlutterPresentViewInfo);
  present_info.view_id = 0;
  present_info.layers = layers;
  present_info.layers_count = 2;
  present_info.user_data = callbacks.user_data;

  size_t received_mutators_count = 0;
  float received_opacity = 0.0f;
  compositor.SetPlatformViewMutatorsRendererCallback(
      [&](const FlutterPlatformView* pv, const FlutterLayer& layer,
          const AndroidPlatformViewMutatorsStack& stack, size_t index) -> bool {
        received_mutators_count = stack.Size();
        if (!stack.IsEmpty()) {
          received_opacity = stack.GetMutators()[0].opacity;
        }
        return true;
      });

  EXPECT_TRUE(callbacks.present_view_callback(&present_info));
  EXPECT_EQ(received_mutators_count, 1U);
  EXPECT_FLOAT_EQ(received_opacity, 0.8f);
  EXPECT_EQ(compositor.GetPresentCount(), 1U);

  EXPECT_TRUE(
      callbacks.collect_backing_store_callback(&store, callbacks.user_data));
}

}  // namespace testing
}  // namespace flutter
