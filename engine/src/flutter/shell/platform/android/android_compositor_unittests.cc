// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <memory>
#include <thread>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/fml/thread.h"
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

TEST(AndroidCompositor, FlutterCompositorCallbacks) {
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidCompositor compositor(surface_manager);

  FlutterCompositor flutter_compositor = compositor.GetFlutterCompositor();
  EXPECT_EQ(flutter_compositor.struct_size, sizeof(FlutterCompositor));
  EXPECT_EQ(flutter_compositor.user_data, &compositor);
  EXPECT_NE(flutter_compositor.create_backing_store_callback, nullptr);
  EXPECT_NE(flutter_compositor.collect_backing_store_callback, nullptr);
  EXPECT_NE(flutter_compositor.present_view_callback, nullptr);

  // Test create backing store through C callback.
  FlutterBackingStoreConfig config = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{800.0, 600.0},
  };
  FlutterBackingStore store = {};
  ASSERT_TRUE(flutter_compositor.create_backing_store_callback(
      &config, &store, flutter_compositor.user_data));
  EXPECT_TRUE(store.did_update);
  EXPECT_EQ(store.type, kFlutterBackingStoreTypeOpenGL);

  // Test collect backing store through C callback.
  ASSERT_TRUE(flutter_compositor.collect_backing_store_callback(
      &store, flutter_compositor.user_data));
  EXPECT_EQ(surface_manager->GetCachedBackingStoreCount(), 1u);
}

TEST(AndroidCompositor, PresentViewOnActiveSurface) {
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidCompositor compositor(surface_manager);

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  compositor.OnSurfaceCreated(window);
  EXPECT_FALSE(compositor.IsSurfaceDestroyed());

  FlutterBackingStoreConfig config = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{1080.0, 1920.0},
  };
  FlutterBackingStore store = {};
  ASSERT_TRUE(compositor.CreateBackingStore(&config, &store));

  FlutterLayer layer = {};
  layer.struct_size = sizeof(FlutterLayer);
  layer.type = kFlutterLayerContentTypeBackingStore;
  layer.backing_store = &store;

  const FlutterLayer* layers[] = {&layer};
  constexpr FlutterViewId kImplicitViewId = 0;
  FlutterPresentViewInfo present_info = {
      .struct_size = sizeof(FlutterPresentViewInfo),
      .view_id = kImplicitViewId,
      .layers = layers,
      .layers_count = 1,
      .user_data = &compositor,
  };

  EXPECT_TRUE(compositor.PresentView(&present_info));
  ASSERT_TRUE(compositor.CollectBackingStore(&store));
}

TEST(AndroidCompositor, PresentViewGracefulDropWhenDestroyed) {
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidCompositor compositor(surface_manager);

  EXPECT_TRUE(compositor.IsSurfaceDestroyed());

  FlutterLayer layer = {};
  layer.struct_size = sizeof(FlutterLayer);
  layer.type = kFlutterLayerContentTypeBackingStore;

  const FlutterLayer* layers[] = {&layer};
  FlutterPresentViewInfo present_info = {
      .struct_size = sizeof(FlutterPresentViewInfo),
      .view_id = 0,
      .layers = layers,
      .layers_count = 1,
      .user_data = &compositor,
  };

  // Presenting while surface is destroyed drops the frame gracefully and returns true.
  EXPECT_TRUE(compositor.PresentView(&present_info));
}

TEST(AndroidCompositor, SynchronousSurfaceDetachBarrier) {
  fml::Thread raster_thread("test_raster_thread");
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidCompositor compositor(surface_manager, nullptr,
                               raster_thread.GetTaskRunner());

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  compositor.OnSurfaceCreated(window);
  EXPECT_TRUE(surface_manager->HasNativeWindow());
  EXPECT_FALSE(compositor.IsSurfaceDestroyed());

  // Allocate a backing store to populate the pool.
  FlutterBackingStoreConfig config = {
      .struct_size = sizeof(FlutterBackingStoreConfig),
      .size = FlutterSize{500.0, 500.0},
  };
  FlutterBackingStore store = {};
  ASSERT_TRUE(compositor.CreateBackingStore(&config, &store));
  ASSERT_TRUE(compositor.CollectBackingStore(&store));
  EXPECT_EQ(surface_manager->GetCachedBackingStoreCount(), 1u);

  // Invoke OnSurfaceDestroyed from platform/main thread.
  // The synchronous barrier must block until the raster thread clears the window.
  compositor.OnSurfaceDestroyed();

  EXPECT_TRUE(compositor.IsSurfaceDestroyed());
  EXPECT_FALSE(surface_manager->HasNativeWindow());
  EXPECT_EQ(surface_manager->GetCachedBackingStoreCount(), 0u);
}

TEST(AndroidCompositor, DestructorExecutesSynchronousTeardownBarrierSafely) {
  fml::Thread raster_thread("test_raster_thread");
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  {
    auto compositor = std::make_unique<AndroidCompositor>(
        surface_manager, nullptr, raster_thread.GetTaskRunner());
    auto window = fml::MakeRefCounted<AndroidNativeWindow>(
        nullptr, /*is_fake_window=*/true);
    compositor->OnSurfaceCreated(window);
    EXPECT_TRUE(surface_manager->HasNativeWindow());
    // Destructor runs here and must not trigger a use-after-free on present_mutex_.
  }
  EXPECT_FALSE(surface_manager->HasNativeWindow());
}

TEST(AndroidCompositor, SurfaceWindowRecreationLifecycle) {
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidCompositor compositor(surface_manager);

  constexpr size_t kCycles = 5;
  for (size_t i = 0; i < kCycles; ++i) {
    auto window_a = fml::MakeRefCounted<AndroidNativeWindow>(
        nullptr, /*is_fake_window=*/true);
    compositor.OnSurfaceCreated(window_a);
    EXPECT_FALSE(compositor.IsSurfaceDestroyed());
    EXPECT_TRUE(surface_manager->HasNativeWindow());

    auto window_b = fml::MakeRefCounted<AndroidNativeWindow>(
        nullptr, /*is_fake_window=*/true);
    compositor.OnSurfaceWindowChanged(window_b);
    EXPECT_FALSE(compositor.IsSurfaceDestroyed());
    EXPECT_EQ(surface_manager->GetNativeWindow(), window_b);

    compositor.OnSurfaceDestroyed();
    EXPECT_TRUE(compositor.IsSurfaceDestroyed());
    EXPECT_FALSE(surface_manager->HasNativeWindow());
  }
}

TEST(AndroidCompositor, ArgumentValidation) {
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  AndroidCompositor compositor(surface_manager);

  // Null present view info.
  EXPECT_FALSE(compositor.PresentView(nullptr));

  // Invalid struct size in present view info.
  FlutterPresentViewInfo invalid_info = {
      .struct_size = sizeof(FlutterPresentViewInfo) - 1,
      .view_id = 0,
      .layers = nullptr,
      .layers_count = 0,
      .user_data = &compositor,
  };
  EXPECT_FALSE(compositor.PresentView(&invalid_info));

  // Null layers pointer when layers_count > 0.
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  compositor.OnSurfaceCreated(window);
  EXPECT_FALSE(compositor.Present(0, nullptr, 1));

  // C callback thunks validation.
  FlutterCompositor flutter_compositor = compositor.GetFlutterCompositor();
  EXPECT_FALSE(
      flutter_compositor.create_backing_store_callback(nullptr, nullptr, nullptr));
  EXPECT_FALSE(
      flutter_compositor.collect_backing_store_callback(nullptr, nullptr));
  EXPECT_FALSE(flutter_compositor.present_view_callback(nullptr));

  FlutterPresentViewInfo null_user_data_info = {
      .struct_size = sizeof(FlutterPresentViewInfo),
      .view_id = 0,
      .layers = nullptr,
      .layers_count = 0,
      .user_data = nullptr,
  };
  EXPECT_FALSE(flutter_compositor.present_view_callback(&null_user_data_info));
  EXPECT_FALSE(flutter_compositor.present_view_callback(&invalid_info));
}

TEST(AndroidCompositor, FeatureFlagGatingDualPathValidation) {
  ScopedEmbedderAPIOverrideReset reset_on_exit;
  Settings settings;

  // Path 1: Feature flag enabled.
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled(settings));
  {
    auto surface_manager = std::make_shared<AndroidSurfaceManager>(
        AndroidRenderingAPI::kImpellerOpenGLES);
    AndroidCompositor compositor(surface_manager);
    auto window = fml::MakeRefCounted<AndroidNativeWindow>(
        nullptr, /*is_fake_window=*/true);
    compositor.OnSurfaceCreated(window);
    EXPECT_FALSE(compositor.IsSurfaceDestroyed());

    FlutterBackingStoreConfig config = {
        .struct_size = sizeof(FlutterBackingStoreConfig),
        .size = FlutterSize{100.0, 100.0},
    };
    FlutterBackingStore store = {};
    EXPECT_TRUE(compositor.CreateBackingStore(&config, &store));
    EXPECT_TRUE(compositor.CollectBackingStore(&store));
    compositor.OnSurfaceDestroyed();
    EXPECT_TRUE(compositor.IsSurfaceDestroyed());
  }

  // Path 2: Feature flag disabled.
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled(settings));
  {
    auto surface_manager = std::make_shared<AndroidSurfaceManager>(
        AndroidRenderingAPI::kImpellerOpenGLES);
    AndroidCompositor compositor(surface_manager);
    EXPECT_TRUE(compositor.IsSurfaceDestroyed());
  }
}

}  // namespace testing
}  // namespace flutter
