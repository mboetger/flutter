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
    compositor_ = std::make_unique<AndroidCompositor>(
        android_context_, jni_mock_, surface_factory_, task_runners_);
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
  std::unique_ptr<AndroidCompositor> compositor_;
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
  EXPECT_EQ(backing_store.open_gl.framebuffer.target, 0x8D40u);
  EXPECT_EQ(backing_store.open_gl.framebuffer.name, 0u);

  EXPECT_TRUE(c.collect_backing_store_callback(&backing_store, c.user_data));
}

#if !SLIMPELLER
TEST_F(AndroidCompositorTest, CreateAndCollectBackingStoreSoftware) {
  auto software_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);
  auto soft_compositor = std::make_unique<AndroidCompositor>(
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
  auto vk_compositor = std::make_unique<AndroidCompositor>(
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
  FlutterPresentViewInfo info = {};
  info.struct_size = sizeof(info);
  info.view_id = 0;
  info.layers = nullptr;
  info.layers_count = 0;
  info.user_data = compositor_.get();

  FlutterCompositor c = compositor_->GetFlutterCompositor();
  EXPECT_TRUE(c.present_view_callback(&info));
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
  EXPECT_CALL(*mock_surface_ptr, TeardownOnScreenContext()).Times(1);
  EXPECT_CALL(*jni_mock_, MaybeResizeSurfaceView(1080, 1920)).Times(1);

  compositor_->SetAndroidSurface(std::move(mock_surface));
  EXPECT_EQ(compositor_->GetAndroidSurface(), mock_surface_ptr);

  EXPECT_TRUE(compositor_->OnScreenSurfaceResize(DlISize{1080, 1920}));
  PostTaskSync(task_runners_.GetPlatformTaskRunner(), []() {});

  compositor_->Teardown();
}

}  // namespace testing
}  // namespace flutter
