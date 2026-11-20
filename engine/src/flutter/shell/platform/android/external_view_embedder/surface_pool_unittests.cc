// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "flutter/shell/platform/android/external_view_embedder/surface_pool.h"

#include "flutter/fml/make_copyable.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/surface/android_surface_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

namespace flutter {
namespace testing {

using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Return;

class TestSurface : public Surface {
 public:
  bool IsValid() override { return true; }
  std::unique_ptr<SurfaceFrame> AcquireFrame(const DlISize& size) override {
    return nullptr;
  }
  DlMatrix GetRootTransformation() const override { return DlMatrix(); }
  GrDirectContext* GetContext() override { return nullptr; }
};

class TestAndroidSurfaceFactory : public AndroidSurfaceFactory {
 public:
  using TestSurfaceProducer =
      std::function<std::unique_ptr<AndroidSurface>(void)>;
  explicit TestAndroidSurfaceFactory(TestSurfaceProducer&& surface_producer) {
    surface_producer_ = surface_producer;
  }

  ~TestAndroidSurfaceFactory() override = default;

  std::unique_ptr<AndroidSurface> CreateSurface() override {
    return surface_producer_();
  }

 private:
  TestSurfaceProducer surface_producer_;
};

TEST(SurfacePool, GetLayerAllocateOneLayer) {
  auto pool = std::make_unique<SurfacePool>();

  auto gr_context = GrDirectContext::MakeMock(nullptr);
  auto android_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);

  auto jni_mock = std::make_shared<JNIMock>();
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, createOverlaySurface2())
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([gr_context, window]() {
        auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
        EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context.get()))
            .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
            .WillOnce(Return(true));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  auto layer = pool->GetLayer(gr_context.get(), *android_context, jni_mock,
                              surface_factory);

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_NE(nullptr, layer);
  ASSERT_EQ(reinterpret_cast<intptr_t>(gr_context.get()),
            layer->gr_context_key);
}

TEST(SurfacePool, GetUnusedLayers) {
  auto pool = std::make_unique<SurfacePool>();

  auto gr_context = GrDirectContext::MakeMock(nullptr);
  auto android_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);

  auto jni_mock = std::make_shared<JNIMock>();
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, createOverlaySurface2())
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([gr_context, window]() {
        auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
        EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context.get()))
            .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
            .WillOnce(Return(true));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  auto layer = pool->GetLayer(gr_context.get(), *android_context, jni_mock,
                              surface_factory);
  ASSERT_EQ(0UL, pool->GetUnusedLayers().size());

  pool->RecycleLayers();

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_EQ(1UL, pool->GetUnusedLayers().size());
  ASSERT_EQ(layer, pool->GetUnusedLayers()[0]);
}

TEST(SurfacePool, GetLayerRecycle) {
  auto pool = std::make_unique<SurfacePool>();

  auto gr_context_1 = GrDirectContext::MakeMock(nullptr);
  auto jni_mock = std::make_shared<JNIMock>();
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, createOverlaySurface2())
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto android_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);

  auto gr_context_2 = GrDirectContext::MakeMock(nullptr);
  auto surface_factory = std::make_shared<TestAndroidSurfaceFactory>(
      [gr_context_1, gr_context_2, window]() {
        auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
        // Allocate two GPU surfaces for each gr context.
        EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context_1.get()))
            .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
        EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context_2.get()))
            .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
        // Set the native window once.
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
            .WillOnce(Return(true));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  auto layer_1 = pool->GetLayer(gr_context_1.get(), *android_context, jni_mock,
                                surface_factory);

  pool->RecycleLayers();

  auto layer_2 = pool->GetLayer(gr_context_2.get(), *android_context, jni_mock,
                                surface_factory);

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_NE(nullptr, layer_1);
  ASSERT_EQ(layer_1, layer_2);
  ASSERT_EQ(reinterpret_cast<intptr_t>(gr_context_2.get()),
            layer_1->gr_context_key);
  ASSERT_EQ(reinterpret_cast<intptr_t>(gr_context_2.get()),
            layer_2->gr_context_key);
}

TEST(SurfacePool, GetLayerAllocateTwoLayers) {
  auto pool = std::make_unique<SurfacePool>();

  auto gr_context = GrDirectContext::MakeMock(nullptr);
  auto android_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);

  auto jni_mock = std::make_shared<JNIMock>();
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, createOverlaySurface2())
      .Times(2)
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))))
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              1, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([gr_context, window]() {
        auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
        EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context.get()))
            .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
            .WillOnce(Return(true));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  auto layer_1 = pool->GetLayer(gr_context.get(), *android_context, jni_mock,
                                surface_factory);
  auto layer_2 = pool->GetLayer(gr_context.get(), *android_context, jni_mock,
                                surface_factory);

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_NE(nullptr, layer_1);
  ASSERT_NE(nullptr, layer_2);
  ASSERT_NE(layer_1, layer_2);
  ASSERT_EQ(0, layer_1->id);
  ASSERT_EQ(1, layer_2->id);
}

TEST(SurfacePool, DestroyLayersNew) {
  auto pool = std::make_unique<SurfacePool>();
  auto jni_mock = std::make_shared<JNIMock>();

  EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(0);
  pool->DestroyLayers(jni_mock);

  auto gr_context = GrDirectContext::MakeMock(nullptr);
  auto android_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, createOverlaySurface2())
      .Times(1)
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([gr_context, window]() {
        auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
        EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context.get()))
            .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
            .WillOnce(Return(true));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  pool->GetLayer(gr_context.get(), *android_context, jni_mock, surface_factory);

  EXPECT_CALL(*jni_mock, destroyOverlaySurface2());

  ASSERT_TRUE(pool->HasLayers());
  pool->DestroyLayers(jni_mock);

  ASSERT_FALSE(pool->HasLayers());
  ASSERT_TRUE(pool->GetUnusedLayers().empty());
}

TEST(SurfacePool, DoesNotDestroyLayersFrameSizeChangedNew) {
  auto pool = std::make_unique<SurfacePool>();
  auto jni_mock = std::make_shared<JNIMock>();

  auto gr_context = GrDirectContext::MakeMock(nullptr);
  auto android_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([gr_context, window]() {
        auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
        EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context.get()))
            .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
            .WillOnce(Return(true));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        EXPECT_CALL(*android_surface_mock,
                    OnScreenSurfaceResize(DlISize(20, 20)))
            .Times(1);
        return android_surface_mock;
      });
  pool->SetFrameSize(DlISize(10, 10));
  EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(0);
  EXPECT_CALL(*jni_mock, createOverlaySurface2())
      .Times(1)
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  ASSERT_FALSE(pool->HasLayers());

  auto layer_1 = pool->GetLayer(gr_context.get(), *android_context, jni_mock,
                                surface_factory);

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_NE(nullptr, layer_1);

  pool->RecycleLayers();
  pool->SetFrameSize(DlISize(20, 20));
  auto layer_2 = pool->GetLayer(gr_context.get(), *android_context, jni_mock,
                                surface_factory);

  ASSERT_TRUE(pool->GetUnusedLayers().empty());
  ASSERT_TRUE(pool->HasLayers());
  ASSERT_EQ(layer_1, layer_2);
}

namespace {

class OrderTrackingAndroidSurface final : public AndroidSurface {
 public:
  explicit OrderTrackingAndroidSurface(std::vector<std::string>* events)
      : events_(events) {}

  bool IsValid() const override { return true; }
  void TeardownOnScreenContext() override {}
  std::unique_ptr<Surface> CreateGPUSurface(GrDirectContext*) override {
    events_->push_back("CreateGPUSurface");
    return std::make_unique<TestSurface>();
  }
  bool OnScreenSurfaceResize(const DlISize&) override { return true; }
  bool ResourceContextMakeCurrent() override { return true; }
  bool ResourceContextClearCurrent() override { return true; }
  void SetupImpellerSurface() override {
    events_->push_back("SetupImpellerSurface");
  }
  bool SetNativeWindow(
      fml::RefPtr<AndroidNativeWindow>,
      const std::shared_ptr<PlatformViewAndroidJNI>&) override {
    events_->push_back("SetNativeWindow");
    return true;
  }

 private:
  std::vector<std::string>* events_;
};

}  // namespace

TEST(SurfacePool, GetLayerCallsSetupImpellerSurfaceBeforeSetNativeWindow) {
  auto pool = std::make_unique<SurfacePool>();
  auto jni_mock = std::make_shared<JNIMock>();
  auto gr_context = GrDirectContext::MakeMock(nullptr);
  auto android_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);

  std::vector<std::string> events;
  EXPECT_CALL(*jni_mock, createOverlaySurface2())
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto factory = std::make_shared<TestAndroidSurfaceFactory>([&events]() {
    return std::make_unique<OrderTrackingAndroidSurface>(&events);
  });

  auto layer =
      pool->GetLayer(gr_context.get(), *android_context, jni_mock, factory);
  ASSERT_NE(layer, nullptr);
  EXPECT_THAT(events, ElementsAre("SetupImpellerSurface", "SetNativeWindow",
                                  "CreateGPUSurface"));
}

TEST(SurfacePool, GetLayerReturnsNullOnAllocationOrSurfaceFailures) {
  // Distinct overlay surface IDs used across failure sub-cases.
  constexpr int kNullWindowOverlayId = 5;
  constexpr int kSetNativeWindowFailOverlayId = 6;
  constexpr int kNullGpuSurfaceOverlayId = 7;
  constexpr int kMultiOverlayFirstId = 8;
  constexpr int kRecycleFailOverlayId = 9;
  constexpr int kHcppFailOverlayId = 10;
  constexpr int kFrameExtentPx = 50;

  auto pool = std::make_unique<SurfacePool>();
  auto jni_mock = std::make_shared<JNIMock>();
  auto gr_context_1 = GrDirectContext::MakeMock(nullptr);
  auto gr_context_2 = GrDirectContext::MakeMock(nullptr);
  auto android_context =
      std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);

  pool->SetFrameSize(DlISize(kFrameExtentPx, kFrameExtentPx));

  // Case 1: Invalid AndroidSurface returns nullptr without calling JNI.
  {
    auto factory = std::make_shared<TestAndroidSurfaceFactory>([]() {
      auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
      EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(false));
      return android_surface_mock;
    });
    EXPECT_CALL(*jni_mock, createOverlaySurface2()).Times(0);
    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(0);
    EXPECT_EQ(
        pool->GetLayer(gr_context_1.get(), *android_context, jni_mock, factory),
        nullptr);
    EXPECT_FALSE(pool->HasLayers());
    EXPECT_FALSE(pool->HasPendingDestroy());
  }

  // Case 2a: Null metadata (Java threw before returning an overlay) returns
  // nullptr without invoking destroyOverlaySurface2.
  {
    auto factory = std::make_shared<TestAndroidSurfaceFactory>([]() {
      auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
      EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
      return android_surface_mock;
    });
    EXPECT_CALL(*jni_mock, createOverlaySurface2())
        .WillOnce(Return(ByMove(nullptr)));
    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(0);
    EXPECT_EQ(
        pool->GetLayer(gr_context_1.get(), *android_context, jni_mock, factory),
        nullptr);
    EXPECT_FALSE(pool->HasLayers());
    EXPECT_FALSE(pool->HasPendingDestroy());
  }

  // Case 2b: Non-null metadata with null window destroys the orphaned Java
  // overlay immediately when no earlier layers are active.
  {
    auto factory = std::make_shared<TestAndroidSurfaceFactory>([]() {
      auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
      EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
      return android_surface_mock;
    });
    EXPECT_CALL(*jni_mock, createOverlaySurface2())
        .WillOnce(Return(
            ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
                kNullWindowOverlayId, nullptr))));
    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(1);
    EXPECT_EQ(
        pool->GetLayer(gr_context_1.get(), *android_context, jni_mock, factory),
        nullptr);
    EXPECT_FALSE(pool->HasLayers());
    EXPECT_FALSE(pool->HasPendingDestroy());
  }

  // Case 3: SetNativeWindow failure returns nullptr without calling
  // CreateGPUSurface and destroys the orphaned Java overlay immediately.
  {
    auto factory = std::make_shared<TestAndroidSurfaceFactory>([window]() {
      auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
      EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
      EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
          .WillOnce(Return(false));
      EXPECT_CALL(*android_surface_mock, CreateGPUSurface(_)).Times(0);
      return android_surface_mock;
    });
    EXPECT_CALL(*jni_mock, createOverlaySurface2())
        .WillOnce(Return(
            ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
                kSetNativeWindowFailOverlayId, window))));
    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(1);
    EXPECT_EQ(
        pool->GetLayer(gr_context_1.get(), *android_context, jni_mock, factory),
        nullptr);
    EXPECT_FALSE(pool->HasLayers());
    EXPECT_FALSE(pool->HasPendingDestroy());
  }

  // Case 4: Null GPU surface returns nullptr and destroys the orphaned Java
  // overlay immediately.
  {
    auto factory = std::make_shared<TestAndroidSurfaceFactory>([gr_context_1,
                                                                window]() {
      auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
      EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
      EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
          .WillOnce(Return(true));
      EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context_1.get()))
          .WillOnce(Return(ByMove(nullptr)));
      return android_surface_mock;
    });
    EXPECT_CALL(*jni_mock, createOverlaySurface2())
        .WillOnce(Return(
            ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
                kNullGpuSurfaceOverlayId, window))));
    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(1);
    EXPECT_EQ(
        pool->GetLayer(gr_context_1.get(), *android_context, jni_mock, factory),
        nullptr);
    EXPECT_FALSE(pool->HasLayers());
    EXPECT_FALSE(pool->HasPendingDestroy());
  }

  // Case 5: GPU surface recreation failure during recycle erases the broken
  // layer and marks pending Java cleanup for DestroyLayers.
  {
    auto factory = std::make_shared<TestAndroidSurfaceFactory>([gr_context_1,
                                                                gr_context_2,
                                                                window]() {
      auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
      EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
      EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
          .WillOnce(Return(true));
      EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context_1.get()))
          .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
      EXPECT_CALL(*android_surface_mock, CreateGPUSurface(gr_context_2.get()))
          .WillOnce(Return(ByMove(nullptr)));
      return android_surface_mock;
    });
    EXPECT_CALL(*jni_mock, createOverlaySurface2())
        .WillOnce(Return(
            ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
                kRecycleFailOverlayId, window))));
    ASSERT_NE(
        pool->GetLayer(gr_context_1.get(), *android_context, jni_mock, factory),
        nullptr);
    pool->RecycleLayers();
    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(0);
    EXPECT_EQ(
        pool->GetLayer(gr_context_2.get(), *android_context, jni_mock, factory),
        nullptr);
    EXPECT_FALSE(pool->HasLayers());
    EXPECT_TRUE(pool->HasPendingDestroy());

    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(1);
    pool->DestroyLayers(jni_mock);
    EXPECT_FALSE(pool->HasPendingDestroy());
  }

  // Case 6: Second overlay failing post-JNI in the same frame preserves active
  // overlay #0 during the frame and destroys Java overlays on DestroyLayers.
  {
    int allocation_call = 0;
    auto factory = std::make_shared<TestAndroidSurfaceFactory>(
        [&allocation_call, gr_context_1, window]() {
          auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
          EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
          if (allocation_call++ == 0) {
            EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
                .WillOnce(Return(true));
            EXPECT_CALL(*android_surface_mock,
                        CreateGPUSurface(gr_context_1.get()))
                .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
          } else {
            EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
                .WillOnce(Return(false));
          }
          return android_surface_mock;
        });
    EXPECT_CALL(*jni_mock, createOverlaySurface2())
        .WillOnce(Return(
            ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
                kMultiOverlayFirstId, window))))
        .WillOnce(Return(
            ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
                kSetNativeWindowFailOverlayId, window))));
    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(0);
    ASSERT_NE(
        pool->GetLayer(gr_context_1.get(), *android_context, jni_mock, factory),
        nullptr);
    EXPECT_EQ(
        pool->GetLayer(gr_context_1.get(), *android_context, jni_mock, factory),
        nullptr);
    EXPECT_TRUE(pool->HasLayers());

    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(1);
    pool->DestroyLayers(jni_mock);
    EXPECT_FALSE(pool->HasLayers());
    EXPECT_FALSE(pool->HasPendingDestroy());
  }

  // Case 7: ResetLayers() recycle failure leaves pending Java cleanup for
  // DestroyLayers, and initial post-JNI failure invokes destroyOverlaySurface2
  // immediately.
  {
    auto hcpp_pool = std::make_unique<SurfacePool>();
    auto fail_factory = std::make_shared<TestAndroidSurfaceFactory>([window]() {
      auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
      EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
      EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
          .WillOnce(Return(false));
      return android_surface_mock;
    });
    EXPECT_CALL(*jni_mock, createOverlaySurface2())
        .WillOnce(Return(
            ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
                kHcppFailOverlayId, window))));
    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(1);
    EXPECT_EQ(hcpp_pool->GetLayer(gr_context_1.get(), *android_context,
                                  jni_mock, fail_factory),
              nullptr);
    EXPECT_FALSE(hcpp_pool->HasLayers());
    EXPECT_FALSE(hcpp_pool->HasPendingDestroy());

    auto recycle_factory = std::make_shared<TestAndroidSurfaceFactory>(
        [gr_context_1, gr_context_2, window]() {
          auto android_surface_mock = std::make_unique<AndroidSurfaceMock>();
          EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
          EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _))
              .WillOnce(Return(true));
          EXPECT_CALL(*android_surface_mock,
                      CreateGPUSurface(gr_context_1.get()))
              .WillOnce(Return(ByMove(std::make_unique<TestSurface>())));
          EXPECT_CALL(*android_surface_mock,
                      CreateGPUSurface(gr_context_2.get()))
              .WillOnce(Return(ByMove(nullptr)));
          return android_surface_mock;
        });
    EXPECT_CALL(*jni_mock, createOverlaySurface2())
        .WillOnce(Return(
            ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
                kHcppFailOverlayId, window))));
    ASSERT_NE(hcpp_pool->GetLayer(gr_context_1.get(), *android_context,
                                  jni_mock, recycle_factory),
              nullptr);
    hcpp_pool->ResetLayers();
    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(0);
    EXPECT_EQ(hcpp_pool->GetLayer(gr_context_2.get(), *android_context,
                                  jni_mock, recycle_factory),
              nullptr);
    EXPECT_FALSE(hcpp_pool->HasLayers());
    EXPECT_TRUE(hcpp_pool->HasPendingDestroy());

    EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(1);
    hcpp_pool->DestroyLayers(jni_mock);
    EXPECT_FALSE(hcpp_pool->HasPendingDestroy());
  }
}

}  // namespace testing
}  // namespace flutter
