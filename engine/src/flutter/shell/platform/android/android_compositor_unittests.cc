// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <memory>
#include <thread>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/flow/embedded_views.h"
#include "flutter/fml/thread.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "gmock/gmock.h"
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

  // Presenting while surface is destroyed drops the frame gracefully and
  // returns true.
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
  // The synchronous barrier must block until the raster thread clears the
  // window.
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
    // Destructor runs here and must not trigger a use-after-free on
    // present_mutex_.
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

TEST(AndroidCompositor, MutationMappingTransformation) {
  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeTransformation;
  mutation.transformation = FlutterTransformation{
      .scaleX = 2.0,
      .skewX = 0.5,
      .transX = 100.0,
      .skewY = 0.25,
      .scaleY = 3.0,
      .transY = 200.0,
      .pers0 = 0.001,
      .pers1 = 0.002,
      .pers2 = 1.5,
  };

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView platform_view = {
      .struct_size = sizeof(FlutterPlatformView),
      .identifier = 42,
      .mutations_count = 1,
      .mutations = mutations,
  };

  MutatorsStack stack =
      AndroidCompositor::ConvertMutationsToMutatorsStack(&platform_view);
  EXPECT_EQ(stack.stack_count(), 1u);

  auto iter = stack.Begin();
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kTransform);

  const DlMatrix& matrix = (*iter)->GetMatrix();
  EXPECT_FLOAT_EQ(matrix.m[0], 2.0f);     // scaleX
  EXPECT_FLOAT_EQ(matrix.m[4], 0.5f);     // skewX
  EXPECT_FLOAT_EQ(matrix.m[12], 100.0f);  // transX
  EXPECT_FLOAT_EQ(matrix.m[1], 0.25f);    // skewY
  EXPECT_FLOAT_EQ(matrix.m[5], 3.0f);     // scaleY
  EXPECT_FLOAT_EQ(matrix.m[13], 200.0f);  // transY
  EXPECT_FLOAT_EQ(matrix.m[3], 0.001f);   // pers0
  EXPECT_FLOAT_EQ(matrix.m[7], 0.002f);   // pers1
  EXPECT_FLOAT_EQ(matrix.m[15], 1.5f);    // pers2
}

TEST(AndroidCompositor, MutationMappingClipRect) {
  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipRect;
  mutation.clip_rect = FlutterRect{
      .left = 10.0,
      .top = 20.0,
      .right = 110.0,
      .bottom = 220.0,
  };

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView platform_view = {
      .struct_size = sizeof(FlutterPlatformView),
      .identifier = 7,
      .mutations_count = 1,
      .mutations = mutations,
  };

  MutatorsStack stack =
      AndroidCompositor::ConvertMutationsToMutatorsStack(&platform_view);
  EXPECT_EQ(stack.stack_count(), 1u);

  auto iter = stack.Begin();
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kClipRect);
  EXPECT_EQ((*iter)->GetRect(), DlRect::MakeLTRB(10.0, 20.0, 110.0, 220.0));
}

TEST(AndroidCompositor, MutationMappingClipRoundedRect) {
  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipRoundedRect;
  mutation.clip_rounded_rect = FlutterRoundedRect{
      .rect = FlutterRect{0.0, 0.0, 300.0, 400.0},
      .upper_left_corner_radius = FlutterSize{12.0, 13.0},
      .upper_right_corner_radius = FlutterSize{14.0, 15.0},
      .lower_right_corner_radius = FlutterSize{16.0, 17.0},
      .lower_left_corner_radius = FlutterSize{18.0, 19.0},
  };

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView platform_view = {
      .struct_size = sizeof(FlutterPlatformView),
      .identifier = 8,
      .mutations_count = 1,
      .mutations = mutations,
  };

  MutatorsStack stack =
      AndroidCompositor::ConvertMutationsToMutatorsStack(&platform_view);
  EXPECT_EQ(stack.stack_count(), 1u);

  auto iter = stack.Begin();
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kClipRRect);
  const DlRoundRect& rrect = (*iter)->GetRRect();
  EXPECT_EQ(rrect.GetBounds(), DlRect::MakeLTRB(0.0, 0.0, 300.0, 400.0));
  EXPECT_DOUBLE_EQ(rrect.GetRadii().top_left.width, 12.0);
  EXPECT_DOUBLE_EQ(rrect.GetRadii().top_left.height, 13.0);
  EXPECT_DOUBLE_EQ(rrect.GetRadii().top_right.width, 14.0);
  EXPECT_DOUBLE_EQ(rrect.GetRadii().top_right.height, 15.0);
  EXPECT_DOUBLE_EQ(rrect.GetRadii().bottom_right.width, 16.0);
  EXPECT_DOUBLE_EQ(rrect.GetRadii().bottom_right.height, 17.0);
  EXPECT_DOUBLE_EQ(rrect.GetRadii().bottom_left.width, 18.0);
  EXPECT_DOUBLE_EQ(rrect.GetRadii().bottom_left.height, 19.0);
}

TEST(AndroidCompositor, MutationMappingClipRoundedSuperellipse) {
  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipRoundedSuperellipse;
  mutation.clip_rounded_superellipse = FlutterRoundedSuperellipse{
      .rect = FlutterRect{50.0, 50.0, 250.0, 350.0},
      .upper_left_corner_radius = FlutterSize{8.0, 9.0},
      .upper_right_corner_radius = FlutterSize{10.0, 11.0},
      .lower_right_corner_radius = FlutterSize{12.0, 13.0},
      .lower_left_corner_radius = FlutterSize{14.0, 15.0},
  };

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  FlutterPlatformView platform_view = {
      .struct_size = sizeof(FlutterPlatformView),
      .identifier = 9,
      .mutations_count = 1,
      .mutations = mutations,
  };

  MutatorsStack stack =
      AndroidCompositor::ConvertMutationsToMutatorsStack(&platform_view);
  EXPECT_EQ(stack.stack_count(), 1u);

  auto iter = stack.Begin();
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kClipRSE);
  const DlRoundRect& approx = (*iter)->GetRSEApproximation();
  EXPECT_EQ(approx.GetBounds(), DlRect::MakeLTRB(50.0, 50.0, 250.0, 350.0));
  EXPECT_DOUBLE_EQ(approx.GetRadii().top_left.width, 8.0);
  EXPECT_DOUBLE_EQ(approx.GetRadii().top_left.height, 9.0);
  EXPECT_DOUBLE_EQ(approx.GetRadii().top_right.width, 10.0);
  EXPECT_DOUBLE_EQ(approx.GetRadii().top_right.height, 11.0);
  EXPECT_DOUBLE_EQ(approx.GetRadii().bottom_right.width, 12.0);
  EXPECT_DOUBLE_EQ(approx.GetRadii().bottom_right.height, 13.0);
  EXPECT_DOUBLE_EQ(approx.GetRadii().bottom_left.width, 14.0);
  EXPECT_DOUBLE_EQ(approx.GetRadii().bottom_left.height, 15.0);
}

TEST(AndroidCompositor, MutationMappingOpacity) {
  // 1. Nominal opacity (0.5 -> 128)
  {
    FlutterPlatformViewMutation mutation = {};
    mutation.type = kFlutterPlatformViewMutationTypeOpacity;
    mutation.opacity = 0.5;
    const FlutterPlatformViewMutation* mutations[] = {&mutation};
    FlutterPlatformView platform_view = {
        .struct_size = sizeof(FlutterPlatformView),
        .identifier = 10,
        .mutations_count = 1,
        .mutations = mutations,
    };
    MutatorsStack stack =
        AndroidCompositor::ConvertMutationsToMutatorsStack(&platform_view);
    EXPECT_EQ(stack.stack_count(), 1u);
    auto iter = stack.Begin();
    ASSERT_NE(iter, stack.End());
    EXPECT_EQ((*iter)->GetType(), MutatorType::kOpacity);
    EXPECT_EQ((*iter)->GetAlpha(), 128u);
  }

  // 2. Negative opacity clamping (-1.0 -> 0)
  {
    FlutterPlatformViewMutation mutation = {};
    mutation.type = kFlutterPlatformViewMutationTypeOpacity;
    mutation.opacity = -1.0;
    const FlutterPlatformViewMutation* mutations[] = {&mutation};
    FlutterPlatformView platform_view = {
        .struct_size = sizeof(FlutterPlatformView),
        .identifier = 10,
        .mutations_count = 1,
        .mutations = mutations,
    };
    MutatorsStack stack =
        AndroidCompositor::ConvertMutationsToMutatorsStack(&platform_view);
    auto iter = stack.Begin();
    ASSERT_NE(iter, stack.End());
    EXPECT_EQ((*iter)->GetAlpha(), 0u);
  }

  // 3. Unity opacity (1.0 -> 255)
  {
    FlutterPlatformViewMutation mutation = {};
    mutation.type = kFlutterPlatformViewMutationTypeOpacity;
    mutation.opacity = 1.0;
    const FlutterPlatformViewMutation* mutations[] = {&mutation};
    FlutterPlatformView platform_view = {
        .struct_size = sizeof(FlutterPlatformView),
        .identifier = 10,
        .mutations_count = 1,
        .mutations = mutations,
    };
    MutatorsStack stack =
        AndroidCompositor::ConvertMutationsToMutatorsStack(&platform_view);
    auto iter = stack.Begin();
    ASSERT_NE(iter, stack.End());
    EXPECT_EQ((*iter)->GetAlpha(), 255u);
  }

  // 4. Greater than 1.0 opacity clamping (2.0 -> 255)
  {
    FlutterPlatformViewMutation mutation = {};
    mutation.type = kFlutterPlatformViewMutationTypeOpacity;
    mutation.opacity = 2.0;
    const FlutterPlatformViewMutation* mutations[] = {&mutation};
    FlutterPlatformView platform_view = {
        .struct_size = sizeof(FlutterPlatformView),
        .identifier = 10,
        .mutations_count = 1,
        .mutations = mutations,
    };
    MutatorsStack stack =
        AndroidCompositor::ConvertMutationsToMutatorsStack(&platform_view);
    auto iter = stack.Begin();
    ASSERT_NE(iter, stack.End());
    EXPECT_EQ((*iter)->GetAlpha(), 255u);
  }
}

TEST(AndroidCompositor, MutationMappingCombinedOrder) {
  FlutterPlatformViewMutation mutation1 = {};
  mutation1.type = kFlutterPlatformViewMutationTypeTransformation;
  mutation1.transformation = FlutterTransformation{
      .scaleX = 1.0,
      .skewX = 0.0,
      .transX = 50.0,
      .skewY = 0.0,
      .scaleY = 1.0,
      .transY = 50.0,
      .pers0 = 0.0,
      .pers1 = 0.0,
      .pers2 = 1.0,
  };

  FlutterPlatformViewMutation mutation2 = {};
  mutation2.type = kFlutterPlatformViewMutationTypeClipRect;
  mutation2.clip_rect = FlutterRect{0.0, 0.0, 100.0, 100.0};

  FlutterPlatformViewMutation mutation3 = {};
  mutation3.type = kFlutterPlatformViewMutationTypeOpacity;
  mutation3.opacity = 0.8;

  const FlutterPlatformViewMutation* mutations[] = {&mutation1, &mutation2,
                                                    &mutation3};
  FlutterPlatformView platform_view = {
      .struct_size = sizeof(FlutterPlatformView),
      .identifier = 11,
      .mutations_count = 3,
      .mutations = mutations,
  };

  MutatorsStack stack =
      AndroidCompositor::ConvertMutationsToMutatorsStack(&platform_view);
  EXPECT_EQ(stack.stack_count(), 3u);

  auto iter = stack.Begin();
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kTransform);
  ++iter;
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kClipRect);
  ++iter;
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kOpacity);
}

TEST(AndroidCompositor, PlatformViewPresentJniDispatch) {
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  auto jni = std::make_shared<JNIMock>();
  AndroidCompositor compositor(surface_manager, jni);

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  compositor.OnSurfaceCreated(window);

  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipRect;
  mutation.clip_rect = FlutterRect{0.0, 0.0, 200.0, 200.0};

  const FlutterPlatformViewMutation* mutations[] = {&mutation};
  constexpr FlutterPlatformViewIdentifier kPlatformViewId = 99;
  FlutterPlatformView platform_view = {
      .struct_size = sizeof(FlutterPlatformView),
      .identifier = kPlatformViewId,
      .mutations_count = 1,
      .mutations = mutations,
  };

  FlutterLayer layer = {};
  layer.struct_size = sizeof(FlutterLayer);
  layer.type = kFlutterLayerContentTypePlatformView;
  layer.platform_view = &platform_view;
  // Test fractional coordinates to verify std::lround rounding defense against
  // seams.
  layer.offset = FlutterPoint{10.4, 20.6};
  layer.size = FlutterSize{199.8, 299.7};

  const FlutterLayer* layers[] = {&layer};
  FlutterPresentViewInfo present_info = {
      .struct_size = sizeof(FlutterPresentViewInfo),
      .view_id = 0,
      .layers = layers,
      .layers_count = 1,
      .user_data = &compositor,
  };

  EXPECT_CALL(*jni, FlutterViewOnDisplayPlatformView(
                        kPlatformViewId, /*x=*/10, /*y=*/21, /*width=*/200,
                        /*height=*/300, /*viewWidth=*/200, /*viewHeight=*/300,
                        ::testing::_))
      .Times(1);

  EXPECT_TRUE(compositor.PresentView(&present_info));
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
  EXPECT_FALSE(flutter_compositor.create_backing_store_callback(
      nullptr, nullptr, nullptr));
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

  // Null platform view conversion.
  EXPECT_TRUE(
      AndroidCompositor::ConvertMutationsToMutatorsStack(nullptr).is_empty());

  // Invalid struct size platform view conversion.
  FlutterPlatformView invalid_pv = {
      .struct_size = sizeof(FlutterPlatformView) - 1,
  };
  EXPECT_TRUE(AndroidCompositor::ConvertMutationsToMutatorsStack(&invalid_pv)
                  .is_empty());
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
