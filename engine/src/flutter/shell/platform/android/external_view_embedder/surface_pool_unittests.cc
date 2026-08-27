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

namespace flutter {
namespace testing {

using ::testing::_;
using ::testing::ByMove;
using ::testing::NiceMock;
using ::testing::Return;

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
  auto pool = std::make_unique<SurfacePool>(/*use_new_surface_methods=*/false);

  auto jni_mock = std::make_shared<JNIMock>();
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, FlutterViewCreateOverlaySurface())
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([window]() {
        auto android_surface_mock =
            std::make_unique<NiceMock<AndroidSurfaceMock>>();
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  auto layer = pool->GetLayer(jni_mock, surface_factory);

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_NE(nullptr, layer);
  ASSERT_EQ(0, layer->id);
}

TEST(SurfacePool, GetUnusedLayers) {
  auto pool = std::make_unique<SurfacePool>(/*use_new_surface_methods=*/false);

  auto jni_mock = std::make_shared<JNIMock>();
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, FlutterViewCreateOverlaySurface())
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([window]() {
        auto android_surface_mock =
            std::make_unique<NiceMock<AndroidSurfaceMock>>();
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  auto layer = pool->GetLayer(jni_mock, surface_factory);
  ASSERT_EQ(0UL, pool->GetUnusedLayers().size());

  pool->RecycleLayers();

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_EQ(1UL, pool->GetUnusedLayers().size());
  ASSERT_EQ(layer, pool->GetUnusedLayers()[0]);
}

TEST(SurfacePool, GetLayerRecycle) {
  auto pool = std::make_unique<SurfacePool>(/*use_new_surface_methods=*/false);

  auto jni_mock = std::make_shared<JNIMock>();
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, FlutterViewCreateOverlaySurface())
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([window]() {
        auto android_surface_mock =
            std::make_unique<NiceMock<AndroidSurfaceMock>>();
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  auto layer_1 = pool->GetLayer(jni_mock, surface_factory);

  pool->RecycleLayers();

  auto layer_2 = pool->GetLayer(jni_mock, surface_factory);

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_NE(nullptr, layer_1);
  ASSERT_EQ(layer_1, layer_2);
}

TEST(SurfacePool, GetLayerAllocateTwoLayers) {
  auto pool = std::make_unique<SurfacePool>(/*use_new_surface_methods=*/false);

  auto jni_mock = std::make_shared<JNIMock>();
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, FlutterViewCreateOverlaySurface())
      .Times(2)
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))))
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              1, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([window]() {
        auto android_surface_mock =
            std::make_unique<NiceMock<AndroidSurfaceMock>>();
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  auto layer_1 = pool->GetLayer(jni_mock, surface_factory);
  auto layer_2 = pool->GetLayer(jni_mock, surface_factory);

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_NE(nullptr, layer_1);
  ASSERT_NE(nullptr, layer_2);
  ASSERT_NE(layer_1, layer_2);
  ASSERT_EQ(0, layer_1->id);
  ASSERT_EQ(1, layer_2->id);
}

TEST(SurfacePool, DestroyLayersNew) {
  auto pool = std::make_unique<SurfacePool>(/*use_new_surface_methods=*/true);
  auto jni_mock = std::make_shared<JNIMock>();

  EXPECT_CALL(*jni_mock, destroyOverlaySurface2()).Times(0);
  pool->DestroyLayers(jni_mock);

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, createOverlaySurface2())
      .Times(1)
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([window]() {
        auto android_surface_mock =
            std::make_unique<NiceMock<AndroidSurfaceMock>>();
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  pool->GetLayer(jni_mock, surface_factory);

  EXPECT_CALL(*jni_mock, destroyOverlaySurface2());

  ASSERT_TRUE(pool->HasLayers());
  pool->DestroyLayers(jni_mock);

  ASSERT_FALSE(pool->HasLayers());
  ASSERT_TRUE(pool->GetUnusedLayers().empty());
}

TEST(SurfacePool, DestroyLayers) {
  auto pool = std::make_unique<SurfacePool>(/*use_new_surface_methods=*/false);
  auto jni_mock = std::make_shared<JNIMock>();

  EXPECT_CALL(*jni_mock, FlutterViewDestroyOverlaySurfaces()).Times(0);
  pool->DestroyLayers(jni_mock);

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);
  EXPECT_CALL(*jni_mock, FlutterViewCreateOverlaySurface())
      .Times(1)
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([window]() {
        auto android_surface_mock =
            std::make_unique<NiceMock<AndroidSurfaceMock>>();
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  pool->GetLayer(jni_mock, surface_factory);

  EXPECT_CALL(*jni_mock, FlutterViewDestroyOverlaySurfaces());

  ASSERT_TRUE(pool->HasLayers());
  pool->DestroyLayers(jni_mock);

  ASSERT_FALSE(pool->HasLayers());
  ASSERT_TRUE(pool->GetUnusedLayers().empty());
}

TEST(SurfacePool, DestroyLayersFrameSizeChanged) {
  auto pool = std::make_unique<SurfacePool>(/*use_new_surface_methods=*/false);
  auto jni_mock = std::make_shared<JNIMock>();

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([window]() {
        auto android_surface_mock =
            std::make_unique<NiceMock<AndroidSurfaceMock>>();
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _));
        EXPECT_CALL(*android_surface_mock, IsValid()).WillOnce(Return(true));
        return android_surface_mock;
      });
  pool->SetFrameSize(DlISize(10, 10));
  EXPECT_CALL(*jni_mock, FlutterViewDestroyOverlaySurfaces()).Times(0);
  EXPECT_CALL(*jni_mock, FlutterViewCreateOverlaySurface())
      .Times(1)
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              0, window))));

  ASSERT_FALSE(pool->HasLayers());

  pool->GetLayer(jni_mock, surface_factory);

  ASSERT_TRUE(pool->HasLayers());

  pool->SetFrameSize(DlISize(20, 20));
  EXPECT_CALL(*jni_mock, FlutterViewDestroyOverlaySurfaces()).Times(1);
  EXPECT_CALL(*jni_mock, FlutterViewCreateOverlaySurface())
      .Times(1)
      .WillOnce(Return(
          ByMove(std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
              1, window))));
  pool->GetLayer(jni_mock, surface_factory);

  ASSERT_TRUE(pool->GetUnusedLayers().empty());
  ASSERT_TRUE(pool->HasLayers());
}

TEST(SurfacePool, DoesNotDestroyLayersFrameSizeChangedNew) {
  auto pool = std::make_unique<SurfacePool>(/*use_new_surface_methods=*/true);
  auto jni_mock = std::make_shared<JNIMock>();

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(nullptr);

  auto surface_factory =
      std::make_shared<TestAndroidSurfaceFactory>([window]() {
        auto android_surface_mock =
            std::make_unique<NiceMock<AndroidSurfaceMock>>();
        EXPECT_CALL(*android_surface_mock, SetNativeWindow(window, _));
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

  auto layer_1 = pool->GetLayer(jni_mock, surface_factory);

  ASSERT_TRUE(pool->HasLayers());
  ASSERT_NE(nullptr, layer_1);

  pool->RecycleLayers();
  pool->SetFrameSize(DlISize(20, 20));
  auto layer_2 = pool->GetLayer(jni_mock, surface_factory);

  ASSERT_TRUE(pool->GetUnusedLayers().empty());
  ASSERT_TRUE(pool->HasLayers());
  ASSERT_EQ(layer_1, layer_2);
}

}  // namespace testing
}  // namespace flutter
