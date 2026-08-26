// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <memory>
#include <vector>

#include "flutter/common/task_runners.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/surface/android_surface_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

namespace {

void PostTaskSync(const fml::RefPtr<fml::TaskRunner>& runner,
                  const fml::closure& task) {
  fml::AutoResetWaitableEvent latch;
  runner->PostTask([&]() {
    task();
    latch.Signal();
  });
  latch.Wait();
}

}  // namespace

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

class TestAndroidSurfaceFactory : public AndroidSurfaceFactory {
 public:
  using TestSurfaceProducer =
      std::function<std::unique_ptr<AndroidSurface>(void)>;
  explicit TestAndroidSurfaceFactory(TestSurfaceProducer&& surface_producer)
      : surface_producer_(std::move(surface_producer)) {}

  ~TestAndroidSurfaceFactory() override = default;

  std::unique_ptr<AndroidSurface> CreateSurface() override {
    if (surface_producer_) {
      return surface_producer_();
    }
    return nullptr;
  }

 private:
  TestSurfaceProducer surface_producer_;
};

class AndroidCompositorTest : public ::testing::Test {
 public:
  AndroidCompositorTest()
      : thread_host_("android_compositor_test",
                     ThreadHost::Type::kPlatform | ThreadHost::Type::kRaster |
                         ThreadHost::Type::kUi),
        task_runners_("android_compositor_test",
                      thread_host_.platform_thread->GetTaskRunner(),
                      thread_host_.raster_thread->GetTaskRunner(),
                      thread_host_.ui_thread->GetTaskRunner(),
                      thread_host_.raster_thread->GetTaskRunner()) {}

  void SetUp() override {
    jni_mock_ = std::make_shared<JNIMock>();
    android_context_ = std::make_shared<AndroidContext>(
        AndroidRenderingAPI::kImpellerOpenGLES);
    surface_factory_ = std::make_shared<TestAndroidSurfaceFactory>(
        []() { return std::make_unique<AndroidSurfaceMock>(); });
    compositor_ = std::make_shared<AndroidCompositor>(
        android_context_, jni_mock_, surface_factory_, task_runners_);
    compositor_->SetSurfaceControlEnabledForTesting(true);
  }

  void TearDown() override {
    if (compositor_) {
      compositor_->Teardown();
      compositor_.reset();
    }
  }

 protected:
  ThreadHost thread_host_;
  TaskRunners task_runners_;
  std::shared_ptr<JNIMock> jni_mock_;
  std::shared_ptr<AndroidContext> android_context_;
  std::shared_ptr<AndroidSurfaceFactory> surface_factory_;
  std::shared_ptr<AndroidCompositor> compositor_;
};

TEST_F(AndroidCompositorTest, GetFlutterCompositorCallbacks) {
  FlutterCompositor c = compositor_->GetFlutterCompositor();
  EXPECT_EQ(c.struct_size, sizeof(FlutterCompositor));
  EXPECT_EQ(c.user_data, compositor_.get());
  EXPECT_NE(c.create_backing_store_callback, nullptr);
  EXPECT_NE(c.collect_backing_store_callback, nullptr);
  EXPECT_NE(c.present_view_callback, nullptr);
  EXPECT_FALSE(c.avoid_backing_store_cache);
}

TEST_F(AndroidCompositorTest, CreateAndCollectBackingStoreOpenGL) {
  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  config.size = {800, 600};
  config.view_id = 0;

  FlutterBackingStore backing_store = {};
  FlutterCompositor c = compositor_->GetFlutterCompositor();

  EXPECT_TRUE(
      c.create_backing_store_callback(&config, &backing_store, c.user_data));
  EXPECT_EQ(backing_store.type, kFlutterBackingStoreTypeOpenGL);
  EXPECT_EQ(backing_store.open_gl.type, kFlutterOpenGLTargetTypeFramebuffer);
  EXPECT_EQ(backing_store.open_gl.framebuffer.target, 0x8058u);
  EXPECT_EQ(backing_store.open_gl.framebuffer.name, 0u);

  EXPECT_TRUE(c.collect_backing_store_callback(&backing_store, c.user_data));
}

#if !SLIMPELLER
TEST_F(AndroidCompositorTest, CreateAndCollectBackingStoreSoftware) {
  auto software_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);
  auto soft_compositor = std::make_shared<AndroidCompositor>(
      software_context, jni_mock_, surface_factory_, task_runners_);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  config.size = {100, 200};
  config.view_id = 0;

  FlutterBackingStore backing_store = {};
  FlutterCompositor c = soft_compositor->GetFlutterCompositor();

  EXPECT_TRUE(
      c.create_backing_store_callback(&config, &backing_store, c.user_data));
  EXPECT_EQ(backing_store.type, kFlutterBackingStoreTypeSoftware);
  EXPECT_NE(backing_store.software.allocation, nullptr);
  EXPECT_EQ(backing_store.software.height, 200u);
  EXPECT_EQ(backing_store.software.row_bytes, 400u);

  EXPECT_TRUE(c.collect_backing_store_callback(&backing_store, c.user_data));
}
#endif  // !SLIMPELLER

TEST_F(AndroidCompositorTest, CreateAndCollectBackingStoreVulkan) {
  auto vk_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kImpellerVulkan);
  auto vk_compositor = std::make_shared<AndroidCompositor>(
      vk_context, jni_mock_, surface_factory_, task_runners_);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  config.size = {100, 200};
  config.view_id = 0;

  FlutterBackingStore backing_store = {};
  FlutterCompositor c = vk_compositor->GetFlutterCompositor();

  EXPECT_TRUE(
      c.create_backing_store_callback(&config, &backing_store, c.user_data));
  EXPECT_EQ(backing_store.type, kFlutterBackingStoreTypeVulkan);
  EXPECT_EQ(backing_store.vulkan.struct_size,
            sizeof(FlutterVulkanBackingStore));

  EXPECT_TRUE(c.collect_backing_store_callback(&backing_store, c.user_data));
}

TEST_F(AndroidCompositorTest, PresentEmptyLayers) {
  EXPECT_CALL(*jni_mock_, swapTransaction()).Times(1);
  EXPECT_CALL(*jni_mock_, onEndFrame2()).Times(1);

  FlutterPresentViewInfo info = {};
  info.struct_size = sizeof(info);
  info.view_id = 0;
  info.layers = nullptr;
  info.layers_count = 0;
  info.user_data = compositor_.get();

  FlutterCompositor c = compositor_->GetFlutterCompositor();
  EXPECT_TRUE(c.present_view_callback(&info));
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});
}

TEST_F(AndroidCompositorTest, PresentSingleBackingStoreLayer) {
  FlutterBackingStore backing_store = {};
  backing_store.struct_size = sizeof(backing_store);
  backing_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer layer = {};
  layer.struct_size = sizeof(layer);
  layer.type = kFlutterLayerContentTypeBackingStore;
  layer.backing_store = &backing_store;
  layer.offset = {0, 0};
  layer.size = {800, 600};

  const FlutterLayer* layers[] = {&layer};

  EXPECT_CALL(*jni_mock_, hideOverlaySurface2()).Times(0);
  EXPECT_CALL(*jni_mock_, swapTransaction()).Times(1);
  EXPECT_CALL(*jni_mock_, onEndFrame2()).Times(1);

  FlutterPresentViewInfo info = {};
  info.struct_size = sizeof(info);
  info.view_id = 0;
  info.layers = layers;
  info.layers_count = 1;
  info.user_data = compositor_.get();

  FlutterCompositor c = compositor_->GetFlutterCompositor();
  EXPECT_TRUE(c.present_view_callback(&info));

  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});
  EXPECT_FALSE(compositor_->IsOverlayLayerShown());
}

TEST_F(AndroidCompositorTest, PresentWithPlatformViewsAndOverlays) {
  FlutterBackingStore base_store = {};
  base_store.struct_size = sizeof(base_store);
  base_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer base_layer = {};
  base_layer.struct_size = sizeof(base_layer);
  base_layer.type = kFlutterLayerContentTypeBackingStore;
  base_layer.backing_store = &base_store;
  base_layer.offset = {0, 0};
  base_layer.size = {800, 600};

  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(platform_view);
  platform_view.identifier = 42;
  platform_view.mutations_count = 0;
  platform_view.mutations = nullptr;

  FlutterLayer pv_layer = {};
  pv_layer.struct_size = sizeof(pv_layer);
  pv_layer.type = kFlutterLayerContentTypePlatformView;
  pv_layer.platform_view = &platform_view;
  pv_layer.offset = {50, 100};
  pv_layer.size = {200, 300};

  FlutterBackingStore overlay_store = {};
  overlay_store.struct_size = sizeof(overlay_store);
  overlay_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer overlay_layer = {};
  overlay_layer.struct_size = sizeof(overlay_layer);
  overlay_layer.type = kFlutterLayerContentTypeBackingStore;
  overlay_layer.backing_store = &overlay_store;
  overlay_layer.offset = {0, 0};
  overlay_layer.size = {800, 600};

  const FlutterLayer* layers[] = {&base_layer, &pv_layer, &overlay_layer};

  EXPECT_CALL(*jni_mock_, showOverlaySurface2()).Times(1);
  EXPECT_CALL(*jni_mock_,
              onDisplayPlatformView2(42, 50, 100, 200, 300, _, _, _))
      .Times(1);
  EXPECT_CALL(*jni_mock_, swapTransaction()).Times(1);
  EXPECT_CALL(*jni_mock_, onEndFrame2()).Times(1);

  EXPECT_TRUE(compositor_->Present(0, layers, 3));
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});

  EXPECT_TRUE(compositor_->IsOverlayLayerShown());
}

TEST_F(AndroidCompositorTest,
       PresentHidesRemovedPlatformViewsInSubsequentFrame) {
  FlutterBackingStore base_store = {};
  base_store.struct_size = sizeof(base_store);
  base_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer base_layer = {};
  base_layer.struct_size = sizeof(base_layer);
  base_layer.type = kFlutterLayerContentTypeBackingStore;
  base_layer.backing_store = &base_store;
  base_layer.offset = {0, 0};
  base_layer.size = {800, 600};

  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(platform_view);
  platform_view.identifier = 42;
  platform_view.mutations_count = 0;
  platform_view.mutations = nullptr;

  FlutterLayer pv_layer = {};
  pv_layer.struct_size = sizeof(pv_layer);
  pv_layer.type = kFlutterLayerContentTypePlatformView;
  pv_layer.platform_view = &platform_view;
  pv_layer.offset = {50, 100};
  pv_layer.size = {200, 300};

  FlutterBackingStore overlay_store = {};
  overlay_store.struct_size = sizeof(overlay_store);
  overlay_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer overlay_layer = {};
  overlay_layer.struct_size = sizeof(overlay_layer);
  overlay_layer.type = kFlutterLayerContentTypeBackingStore;
  overlay_layer.backing_store = &overlay_store;
  overlay_layer.offset = {0, 0};
  overlay_layer.size = {800, 600};

  const FlutterLayer* layers[] = {&base_layer, &pv_layer, &overlay_layer};

  EXPECT_CALL(*jni_mock_, showOverlaySurface2()).Times(1);
  EXPECT_CALL(*jni_mock_,
              onDisplayPlatformView2(42, 50, 100, 200, 300, _, _, _))
      .Times(1);
  EXPECT_CALL(*jni_mock_, swapTransaction()).Times(1);
  EXPECT_CALL(*jni_mock_, onEndFrame2()).Times(1);

  EXPECT_TRUE(compositor_->Present(0, layers, 3));
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});

  EXPECT_TRUE(compositor_->IsOverlayLayerShown());

  // Second frame: only base layer (platform view 42 removed).
  const FlutterLayer* single_layers[] = {&base_layer};

  EXPECT_CALL(*jni_mock_, hidePlatformView2(42)).Times(1);
  EXPECT_CALL(*jni_mock_, hideOverlaySurface2()).Times(1);
  EXPECT_CALL(*jni_mock_, swapTransaction()).Times(1);
  EXPECT_CALL(*jni_mock_, onEndFrame2()).Times(1);

  EXPECT_TRUE(compositor_->Present(0, single_layers, 1));
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});

  EXPECT_FALSE(compositor_->IsOverlayLayerShown());
}

TEST_F(AndroidCompositorTest, SurfaceLifecycleAndResize) {
  auto mock_surface = std::make_unique<AndroidSurfaceMock>();
  auto* mock_surface_ptr = mock_surface.get();

  EXPECT_CALL(*mock_surface_ptr, OnScreenSurfaceResize(DlISize{1080, 1920}))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_surface_ptr, TeardownOnScreenContext())
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(*jni_mock_, MaybeResizeSurfaceView(1080, 1920)).Times(1);

  compositor_->SetAndroidSurface(std::move(mock_surface));
  EXPECT_EQ(compositor_->GetAndroidSurface(), mock_surface_ptr);

  EXPECT_TRUE(compositor_->OnScreenSurfaceResize(DlISize{1080, 1920}));
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});

  compositor_->Teardown();
}

TEST_F(AndroidCompositorTest, ToDlRectConversion) {
  FlutterRect rect = {10.0, 20.0, 110.0, 220.0};
  DlRect dl_rect = AndroidCompositor::ToDlRect(rect);
  EXPECT_FLOAT_EQ(dl_rect.GetLeft(), 10.0f);
  EXPECT_FLOAT_EQ(dl_rect.GetTop(), 20.0f);
  EXPECT_FLOAT_EQ(dl_rect.GetRight(), 110.0f);
  EXPECT_FLOAT_EQ(dl_rect.GetBottom(), 220.0f);
}

TEST_F(AndroidCompositorTest, ToDlRoundRectConversion) {
  FlutterRoundedRect rrect = {};
  rrect.rect = {10.0, 20.0, 110.0, 220.0};
  rrect.upper_left_corner_radius = {5.0, 6.0};
  rrect.upper_right_corner_radius = {7.0, 8.0};
  rrect.lower_right_corner_radius = {9.0, 10.0};
  rrect.lower_left_corner_radius = {11.0, 12.0};

  DlRoundRect dl_rrect = AndroidCompositor::ToDlRoundRect(rrect);
  EXPECT_FLOAT_EQ(dl_rrect.GetBounds().GetLeft(), 10.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetBounds().GetTop(), 20.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetBounds().GetRight(), 110.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetBounds().GetBottom(), 220.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetRadii().top_left.width, 5.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetRadii().top_left.height, 6.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetRadii().top_right.width, 7.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetRadii().top_right.height, 8.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetRadii().bottom_right.width, 9.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetRadii().bottom_right.height, 10.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetRadii().bottom_left.width, 11.0f);
  EXPECT_FLOAT_EQ(dl_rrect.GetRadii().bottom_left.height, 12.0f);
}

TEST_F(AndroidCompositorTest, ToDlMatrixConversion) {
  FlutterTransformation t = {
      2.0,  0.5,  10.0,  // scaleX, skewX, transX
      0.2,  3.0,  20.0,  // skewY, scaleY, transY
      0.01, 0.02, 1.0    // pers0, pers1, pers2
  };

  DlMatrix m = AndroidCompositor::ToDlMatrix(t);
  EXPECT_FLOAT_EQ(m.m[0], 2.0f);    // scaleX
  EXPECT_FLOAT_EQ(m.m[1], 0.2f);    // skewY
  EXPECT_FLOAT_EQ(m.m[3], 0.01f);   // pers0
  EXPECT_FLOAT_EQ(m.m[4], 0.5f);    // skewX
  EXPECT_FLOAT_EQ(m.m[5], 3.0f);    // scaleY
  EXPECT_FLOAT_EQ(m.m[7], 0.02f);   // pers1
  EXPECT_FLOAT_EQ(m.m[12], 10.0f);  // transX
  EXPECT_FLOAT_EQ(m.m[13], 20.0f);  // transY
  EXPECT_FLOAT_EQ(m.m[15], 1.0f);   // pers2
}

TEST_F(AndroidCompositorTest, ToMutatorsStackConversion) {
  FlutterPlatformViewMutation m1 = {};
  m1.type = kFlutterPlatformViewMutationTypeOpacity;
  m1.opacity = 0.5;

  FlutterPlatformViewMutation m2 = {};
  m2.type = kFlutterPlatformViewMutationTypeClipRect;
  m2.clip_rect = {10.0, 10.0, 100.0, 100.0};

  FlutterPlatformViewMutation m3 = {};
  m3.type = kFlutterPlatformViewMutationTypeTransformation;
  m3.transformation = {1.0, 0.0, 50.0, 0.0, 1.0, 50.0, 0.0, 0.0, 1.0};

  const FlutterPlatformViewMutation* mutations[] = {&m1, &m2, &m3};
  FlutterPlatformView pv = {};
  pv.struct_size = sizeof(pv);
  pv.identifier = 100;
  pv.mutations_count = 3;
  pv.mutations = mutations;

  MutatorsStack stack = AndroidCompositor::ToMutatorsStack(&pv);
  EXPECT_FALSE(stack.is_empty());
  EXPECT_EQ(stack.stack_count(), 3u);

  auto iter = stack.Begin();
  EXPECT_EQ((*iter)->GetType(), MutatorType::kOpacity);
  EXPECT_EQ((*iter)->GetAlpha(), 128u);

  iter++;
  EXPECT_EQ((*iter)->GetType(), MutatorType::kClipRect);
  EXPECT_FLOAT_EQ((*iter)->GetRect().GetLeft(), 10.0f);

  iter++;
  EXPECT_EQ((*iter)->GetType(), MutatorType::kTransform);
  EXPECT_FLOAT_EQ((*iter)->GetMatrix().m[12], 50.0f);
}

TEST_F(AndroidCompositorTest, PresentWithPlatformViewMutatorsPassedToJNI) {
  FlutterBackingStore base_store = {};
  base_store.struct_size = sizeof(base_store);
  base_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer base_layer = {};
  base_layer.struct_size = sizeof(base_layer);
  base_layer.type = kFlutterLayerContentTypeBackingStore;
  base_layer.backing_store = &base_store;
  base_layer.offset = {0, 0};
  base_layer.size = {800, 600};

  FlutterPlatformViewMutation m1 = {};
  m1.type = kFlutterPlatformViewMutationTypeOpacity;
  m1.opacity = 0.75;

  const FlutterPlatformViewMutation* mutations[] = {&m1};
  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(platform_view);
  platform_view.identifier = 99;
  platform_view.mutations_count = 1;
  platform_view.mutations = mutations;

  FlutterLayer pv_layer = {};
  pv_layer.struct_size = sizeof(pv_layer);
  pv_layer.type = kFlutterLayerContentTypePlatformView;
  pv_layer.platform_view = &platform_view;
  pv_layer.offset = {20, 30};
  pv_layer.size = {150, 250};

  const FlutterLayer* layers[] = {&base_layer, &pv_layer};

  EXPECT_CALL(*jni_mock_, onDisplayPlatformView2(99, 20, 30, 150, 250, _, _, _))
      .WillOnce([](int32_t view_id, int32_t x, int32_t y, int32_t width,
                   int32_t height, int32_t view_width, int32_t view_height,
                   MutatorsStack stack) {
        EXPECT_EQ(stack.stack_count(), 1u);
        EXPECT_EQ((*stack.Begin())->GetType(), MutatorType::kOpacity);
      });
  EXPECT_CALL(*jni_mock_, swapTransaction()).Times(1);
  EXPECT_CALL(*jni_mock_, onEndFrame2()).Times(1);

  EXPECT_TRUE(compositor_->Present(0, layers, 2));
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});
}

TEST_F(AndroidCompositorTest, ToMutatorsStackEdgeCases) {
  // Test 1: Null platform_view
  MutatorsStack null_pv_stack = AndroidCompositor::ToMutatorsStack(nullptr);
  EXPECT_TRUE(null_pv_stack.is_empty());

  // Test 2: Null mutations array
  FlutterPlatformView pv_null_mutations = {};
  pv_null_mutations.struct_size = sizeof(pv_null_mutations);
  pv_null_mutations.mutations = nullptr;
  pv_null_mutations.mutations_count = 5;
  MutatorsStack null_muts_stack =
      AndroidCompositor::ToMutatorsStack(&pv_null_mutations);
  EXPECT_TRUE(null_muts_stack.is_empty());

  // Test 3: Null elements inside mutations array
  FlutterPlatformViewMutation m1 = {};
  m1.type = kFlutterPlatformViewMutationTypeOpacity;
  m1.opacity = 0.5;

  const FlutterPlatformViewMutation* mutations_with_null[] = {nullptr, &m1,
                                                              nullptr};
  FlutterPlatformView pv_with_null = {};
  pv_with_null.struct_size = sizeof(pv_with_null);
  pv_with_null.mutations = mutations_with_null;
  pv_with_null.mutations_count = 3;

  MutatorsStack with_null_stack =
      AndroidCompositor::ToMutatorsStack(&pv_with_null);
  EXPECT_EQ(with_null_stack.stack_count(), 1u);
  EXPECT_EQ((*with_null_stack.Begin())->GetType(), MutatorType::kOpacity);

  // Test 4: NaN, negative, and oversized opacity clamping
  FlutterPlatformViewMutation nan_opacity = {};
  nan_opacity.type = kFlutterPlatformViewMutationTypeOpacity;
  nan_opacity.opacity = std::numeric_limits<double>::quiet_NaN();

  FlutterPlatformViewMutation neg_opacity = {};
  neg_opacity.type = kFlutterPlatformViewMutationTypeOpacity;
  neg_opacity.opacity = -2.5;

  FlutterPlatformViewMutation high_opacity = {};
  high_opacity.type = kFlutterPlatformViewMutationTypeOpacity;
  high_opacity.opacity = 5.0;

  const FlutterPlatformViewMutation* opacity_muts[] = {
      &nan_opacity, &neg_opacity, &high_opacity};
  FlutterPlatformView pv_opacities = {};
  pv_opacities.struct_size = sizeof(pv_opacities);
  pv_opacities.mutations = opacity_muts;
  pv_opacities.mutations_count = 3;

  MutatorsStack op_stack = AndroidCompositor::ToMutatorsStack(&pv_opacities);
  EXPECT_EQ(op_stack.stack_count(), 3u);

  auto it = op_stack.Begin();
  EXPECT_EQ((*it)->GetAlpha(), 255u);  // NaN defaults to 1.0 (255)
  it++;
  EXPECT_EQ((*it)->GetAlpha(), 0u);  // -2.5 clamps to 0.0 (0)
  it++;
  EXPECT_EQ((*it)->GetAlpha(), 255u);  // 5.0 clamps to 1.0 (255)
}

TEST_F(AndroidCompositorTest, ToDlMatrixEdgeCases) {
  // Test reflection, shear, and perspective
  FlutterTransformation t = {
      -1.5,  0.25, -100.0,  // scaleX (negative), skewX, transX
      0.75,  -2.0, 200.0,   // skewY, scaleY (negative), transY
      -0.01, 0.05, 1.5      // pers0, pers1, pers2
  };

  DlMatrix m = AndroidCompositor::ToDlMatrix(t);
  EXPECT_FLOAT_EQ(m.m[0], -1.5f);
  EXPECT_FLOAT_EQ(m.m[1], 0.75f);
  EXPECT_FLOAT_EQ(m.m[3], -0.01f);
  EXPECT_FLOAT_EQ(m.m[4], 0.25f);
  EXPECT_FLOAT_EQ(m.m[5], -2.0f);
  EXPECT_FLOAT_EQ(m.m[7], 0.05f);
  EXPECT_FLOAT_EQ(m.m[12], -100.0f);
  EXPECT_FLOAT_EQ(m.m[13], 200.0f);
  EXPECT_FLOAT_EQ(m.m[15], 1.5f);
}

TEST_F(AndroidCompositorTest,
       PresentWithMultiplePlatformViewsAndOverlaysInterleaved) {
  FlutterBackingStore base_store = {};
  base_store.struct_size = sizeof(base_store);
  base_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer base_layer = {};
  base_layer.struct_size = sizeof(base_layer);
  base_layer.type = kFlutterLayerContentTypeBackingStore;
  base_layer.backing_store = &base_store;
  base_layer.offset = {0, 0};
  base_layer.size = {1080, 1920};

  FlutterPlatformView pv1 = {};
  pv1.struct_size = sizeof(pv1);
  pv1.identifier = 10;
  pv1.mutations_count = 0;
  pv1.mutations = nullptr;

  FlutterLayer pv_layer1 = {};
  pv_layer1.struct_size = sizeof(pv_layer1);
  pv_layer1.type = kFlutterLayerContentTypePlatformView;
  pv_layer1.platform_view = &pv1;
  pv_layer1.offset = {10, 20};
  pv_layer1.size = {300, 400};

  FlutterBackingStore overlay_store = {};
  overlay_store.struct_size = sizeof(overlay_store);
  overlay_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer overlay_layer = {};
  overlay_layer.struct_size = sizeof(overlay_layer);
  overlay_layer.type = kFlutterLayerContentTypeBackingStore;
  overlay_layer.backing_store = &overlay_store;
  overlay_layer.offset = {0, 0};
  overlay_layer.size = {1080, 1920};

  FlutterPlatformView pv2 = {};
  pv2.struct_size = sizeof(pv2);
  pv2.identifier = 20;
  pv2.mutations_count = 0;
  pv2.mutations = nullptr;

  FlutterLayer pv_layer2 = {};
  pv_layer2.struct_size = sizeof(pv_layer2);
  pv_layer2.type = kFlutterLayerContentTypePlatformView;
  pv_layer2.platform_view = &pv2;
  pv_layer2.offset = {50, 60};
  pv_layer2.size = {500, 600};

  const FlutterLayer* layers[] = {&base_layer, &pv_layer1, &overlay_layer,
                                  &pv_layer2};

  EXPECT_CALL(*jni_mock_, showOverlaySurface2()).Times(1);
  EXPECT_CALL(*jni_mock_,
              onDisplayPlatformView2(10, 10, 20, 300, 400, 300, 400, _))
      .Times(1);
  EXPECT_CALL(*jni_mock_,
              onDisplayPlatformView2(20, 50, 60, 500, 600, 500, 600, _))
      .Times(1);
  EXPECT_CALL(*jni_mock_, swapTransaction()).Times(1);
  EXPECT_CALL(*jni_mock_, onEndFrame2()).Times(1);

  EXPECT_TRUE(compositor_->Present(0, layers, 4));
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});
}

TEST_F(AndroidCompositorTest, PresentWithWeakPtrDestructionSafety) {
  auto temp_compositor = std::make_shared<AndroidCompositor>(
      android_context_, jni_mock_, surface_factory_, task_runners_);
  temp_compositor->SetSurfaceControlEnabledForTesting(true);

  FlutterBackingStore base_store = {};
  base_store.struct_size = sizeof(base_store);
  base_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer base_layer = {};
  base_layer.struct_size = sizeof(base_layer);
  base_layer.type = kFlutterLayerContentTypeBackingStore;
  base_layer.backing_store = &base_store;
  base_layer.offset = {0, 0};
  base_layer.size = {800, 600};

  const FlutterLayer* layers[] = {&base_layer};

  EXPECT_CALL(*jni_mock_, swapTransaction()).Times(1);
  EXPECT_CALL(*jni_mock_, onEndFrame2()).Times(1);

  EXPECT_TRUE(temp_compositor->Present(0, layers, 1));

  // Destroy compositor immediately before platform queue runs.
  temp_compositor.reset();

  // Flush platform task runner and verify no crash/use-after-free occurs.
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});
}

TEST_F(AndroidCompositorTest, HybridCompositionPresentWithPlatformViews) {
  compositor_->SetSurfaceControlEnabledForTesting(false);

  FlutterBackingStore base_store = {};
  base_store.struct_size = sizeof(base_store);
  base_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterLayer base_layer = {};
  base_layer.struct_size = sizeof(base_layer);
  base_layer.type = kFlutterLayerContentTypeBackingStore;
  base_layer.backing_store = &base_store;
  base_layer.offset = {0, 0};
  base_layer.size = {800, 600};

  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(platform_view);
  platform_view.identifier = 42;
  platform_view.mutations_count = 0;
  platform_view.mutations = nullptr;

  FlutterLayer pv_layer = {};
  pv_layer.struct_size = sizeof(pv_layer);
  pv_layer.type = kFlutterLayerContentTypePlatformView;
  pv_layer.platform_view = &platform_view;
  pv_layer.offset = {50, 100};
  pv_layer.size = {200, 300};

  const FlutterLayer* layers[] = {&base_layer, &pv_layer};

  EXPECT_CALL(*jni_mock_, FlutterViewBeginFrame()).Times(1);
  EXPECT_CALL(*jni_mock_,
              FlutterViewOnDisplayPlatformView(42, 50, 100, 200, 300, _, _, _))
      .Times(1);
  EXPECT_CALL(*jni_mock_, FlutterViewDestroyOverlaySurfaces()).Times(1);
  EXPECT_CALL(*jni_mock_, FlutterViewEndFrame()).Times(1);

  EXPECT_TRUE(compositor_->Present(0, layers, 2));
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});
}

}  // namespace testing
}  // namespace flutter
