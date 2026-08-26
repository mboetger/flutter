// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <memory>
#include <vector>

#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/flow/embedded_views.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {
namespace android {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::SaveArg;

class MockAndroidContext : public AndroidContext {
 public:
  explicit MockAndroidContext(AndroidRenderingAPI rendering_api)
      : AndroidContext(rendering_api) {}

  ~MockAndroidContext() override = default;

  bool IsValid() const override { return true; }
};

TEST(AndroidCompositorTest, GetCompositorInitializesCorrectly) {
  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(context);
  AndroidCompositor compositor(surface_manager, nullptr);

  FlutterCompositor c_compositor = compositor.GetCompositor();
  EXPECT_EQ(c_compositor.struct_size, sizeof(FlutterCompositor));
  EXPECT_EQ(c_compositor.user_data, &compositor);
  EXPECT_NE(c_compositor.create_backing_store_callback, nullptr);
  EXPECT_NE(c_compositor.collect_backing_store_callback, nullptr);
  EXPECT_NE(c_compositor.present_view_callback, nullptr);
}

TEST(AndroidCompositorTest, CreateAndCollectBackingStoreViaCallbacks) {
  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(context);
  AndroidCompositor compositor(surface_manager, nullptr);

  FlutterCompositor c_compositor = compositor.GetCompositor();

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  // Dimensions 640x480 for compositor test frame
  config.size.width = 640;
  config.size.height = 480;

  FlutterBackingStore store = {};
  ASSERT_TRUE(c_compositor.create_backing_store_callback(
      &config, &store, c_compositor.user_data));

  EXPECT_EQ(store.type, kFlutterBackingStoreTypeOpenGL);
  EXPECT_EQ(surface_manager->GetPoolSize(), 1u);
  EXPECT_EQ(surface_manager->GetInUseCount(), 1u);

  ASSERT_TRUE(c_compositor.collect_backing_store_callback(
      &store, c_compositor.user_data));
  EXPECT_EQ(surface_manager->GetInUseCount(), 0u);
}

TEST(AndroidCompositorTest, ConvertMutatorsNullAndEmpty) {
  EXPECT_TRUE(AndroidCompositor::ConvertMutators(nullptr).is_empty());

  FlutterPlatformView view_no_mutations = {};
  view_no_mutations.struct_size = sizeof(view_no_mutations);
  // View identifier 10
  view_no_mutations.identifier = 10;
  view_no_mutations.mutations_count = 0;
  view_no_mutations.mutations = nullptr;
  EXPECT_TRUE(
      AndroidCompositor::ConvertMutators(&view_no_mutations).is_empty());

  const FlutterPlatformViewMutation* null_mutations[] = {nullptr, nullptr};
  FlutterPlatformView view_null_entries = {};
  view_null_entries.struct_size = sizeof(view_null_entries);
  // View identifier 11
  view_null_entries.identifier = 11;
  view_null_entries.mutations_count = 2;
  view_null_entries.mutations = null_mutations;
  EXPECT_TRUE(
      AndroidCompositor::ConvertMutators(&view_null_entries).is_empty());
}

TEST(AndroidCompositorTest, ConvertMutatorsTransformation) {
  FlutterPlatformViewMutation transform_mutation = {};
  transform_mutation.type = kFlutterPlatformViewMutationTypeTransformation;
  // Scale by 2.0x, 3.0y, translate by 100.0x, 200.0y, skew 0.5x, 0.25y
  transform_mutation.transformation.scaleX = 2.0;
  transform_mutation.transformation.skewX = 0.5;
  transform_mutation.transformation.transX = 100.0;
  transform_mutation.transformation.skewY = 0.25;
  transform_mutation.transformation.scaleY = 3.0;
  transform_mutation.transformation.transY = 200.0;
  transform_mutation.transformation.pers0 = 0.0;
  transform_mutation.transformation.pers1 = 0.0;
  transform_mutation.transformation.pers2 = 1.0;

  const FlutterPlatformViewMutation* mutations[] = {&transform_mutation};
  FlutterPlatformView view = {};
  view.struct_size = sizeof(view);
  // View identifier 100
  view.identifier = 100;
  view.mutations_count = 1;
  view.mutations = mutations;

  MutatorsStack stack = AndroidCompositor::ConvertMutators(&view);
  EXPECT_FALSE(stack.is_empty());
  EXPECT_EQ(stack.stack_count(), 1u);

  auto iter = stack.Begin();
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kTransform);

  const DlMatrix& matrix = (*iter)->GetMatrix();
  // Column 0: scaleX (2.0), skewY (0.25), 0, pers0 (0)
  EXPECT_FLOAT_EQ(matrix.m[0], 2.0f);
  EXPECT_FLOAT_EQ(matrix.m[1], 0.25f);
  EXPECT_FLOAT_EQ(matrix.m[3], 0.0f);
  // Column 1: skewX (0.5), scaleY (3.0), 0, pers1 (0)
  EXPECT_FLOAT_EQ(matrix.m[4], 0.5f);
  EXPECT_FLOAT_EQ(matrix.m[5], 3.0f);
  EXPECT_FLOAT_EQ(matrix.m[7], 0.0f);
  // Column 3: transX (100.0), transY (200.0), 0, pers2 (1.0)
  EXPECT_FLOAT_EQ(matrix.m[12], 100.0f);
  EXPECT_FLOAT_EQ(matrix.m[13], 200.0f);
  EXPECT_FLOAT_EQ(matrix.m[15], 1.0f);
}

TEST(AndroidCompositorTest, ConvertMutatorsClipRect) {
  FlutterPlatformViewMutation clip_mutation = {};
  clip_mutation.type = kFlutterPlatformViewMutationTypeClipRect;
  // Bounds (10.0, 20.0, 110.0, 220.0)
  clip_mutation.clip_rect.left = 10.0;
  clip_mutation.clip_rect.top = 20.0;
  clip_mutation.clip_rect.right = 110.0;
  clip_mutation.clip_rect.bottom = 220.0;

  const FlutterPlatformViewMutation* mutations[] = {&clip_mutation};
  FlutterPlatformView view = {};
  view.struct_size = sizeof(view);
  // View identifier 101
  view.identifier = 101;
  view.mutations_count = 1;
  view.mutations = mutations;

  MutatorsStack stack = AndroidCompositor::ConvertMutators(&view);
  EXPECT_EQ(stack.stack_count(), 1u);

  auto iter = stack.Begin();
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kClipRect);

  const DlRect& rect = (*iter)->GetRect();
  EXPECT_FLOAT_EQ(rect.GetLeft(), 10.0f);
  EXPECT_FLOAT_EQ(rect.GetTop(), 20.0f);
  EXPECT_FLOAT_EQ(rect.GetRight(), 110.0f);
  EXPECT_FLOAT_EQ(rect.GetBottom(), 220.0f);
}

TEST(AndroidCompositorTest, ConvertMutatorsClipRoundedRect) {
  FlutterPlatformViewMutation rrect_mutation = {};
  rrect_mutation.type = kFlutterPlatformViewMutationTypeClipRoundedRect;
  // Bounds (5.0, 10.0, 205.0, 310.0)
  rrect_mutation.clip_rounded_rect.rect.left = 5.0;
  rrect_mutation.clip_rounded_rect.rect.top = 10.0;
  rrect_mutation.clip_rounded_rect.rect.right = 205.0;
  rrect_mutation.clip_rounded_rect.rect.bottom = 310.0;
  // Corner radii: top-left (8, 8), top-right (12, 12), bottom-right (16, 16),
  // bottom-left (4, 4)
  rrect_mutation.clip_rounded_rect.upper_left_corner_radius = {8.0, 8.0};
  rrect_mutation.clip_rounded_rect.upper_right_corner_radius = {12.0, 12.0};
  rrect_mutation.clip_rounded_rect.lower_right_corner_radius = {16.0, 16.0};
  rrect_mutation.clip_rounded_rect.lower_left_corner_radius = {4.0, 4.0};

  const FlutterPlatformViewMutation* mutations[] = {&rrect_mutation};
  FlutterPlatformView view = {};
  view.struct_size = sizeof(view);
  // View identifier 102
  view.identifier = 102;
  view.mutations_count = 1;
  view.mutations = mutations;

  MutatorsStack stack = AndroidCompositor::ConvertMutators(&view);
  EXPECT_EQ(stack.stack_count(), 1u);

  auto iter = stack.Begin();
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kClipRRect);

  const DlRoundRect& rrect = (*iter)->GetRRect();
  EXPECT_FLOAT_EQ(rrect.GetBounds().GetLeft(), 5.0f);
  EXPECT_FLOAT_EQ(rrect.GetBounds().GetTop(), 10.0f);
  EXPECT_FLOAT_EQ(rrect.GetBounds().GetRight(), 205.0f);
  EXPECT_FLOAT_EQ(rrect.GetBounds().GetBottom(), 310.0f);

  const DlRoundingRadii& radii = rrect.GetRadii();
  EXPECT_FLOAT_EQ(radii.top_left.width, 8.0f);
  EXPECT_FLOAT_EQ(radii.top_left.height, 8.0f);
  EXPECT_FLOAT_EQ(radii.top_right.width, 12.0f);
  EXPECT_FLOAT_EQ(radii.top_right.height, 12.0f);
  EXPECT_FLOAT_EQ(radii.bottom_right.width, 16.0f);
  EXPECT_FLOAT_EQ(radii.bottom_right.height, 16.0f);
  EXPECT_FLOAT_EQ(radii.bottom_left.width, 4.0f);
  EXPECT_FLOAT_EQ(radii.bottom_left.height, 4.0f);
}

TEST(AndroidCompositorTest, ConvertMutatorsOpacity) {
  FlutterPlatformViewMutation opacity_mutation = {};
  opacity_mutation.type = kFlutterPlatformViewMutationTypeOpacity;
  // Half opacity 0.5 -> 128 alpha (round(0.5 * 255) = 128)
  opacity_mutation.opacity = 0.5;

  const FlutterPlatformViewMutation* mutations[] = {&opacity_mutation};
  FlutterPlatformView view = {};
  view.struct_size = sizeof(view);
  // View identifier 103
  view.identifier = 103;
  view.mutations_count = 1;
  view.mutations = mutations;

  MutatorsStack stack = AndroidCompositor::ConvertMutators(&view);
  EXPECT_EQ(stack.stack_count(), 1u);

  auto iter = stack.Begin();
  ASSERT_NE(iter, stack.End());
  EXPECT_EQ((*iter)->GetType(), MutatorType::kOpacity);
  // 128 / 255.0f ~ 0.50196f
  EXPECT_NEAR((*iter)->GetAlphaFloat(), 0.5f, 0.01f);
  EXPECT_EQ((*iter)->GetAlpha(), 128);
}

TEST(AndroidCompositorTest, ConvertMutatorsCompositeStack) {
  FlutterPlatformViewMutation trans_mut = {};
  trans_mut.type = kFlutterPlatformViewMutationTypeTransformation;
  trans_mut.transformation.scaleX = 1.0;
  trans_mut.transformation.scaleY = 1.0;
  trans_mut.transformation.transX = 50.0;
  trans_mut.transformation.transY = 50.0;
  trans_mut.transformation.pers2 = 1.0;

  FlutterPlatformViewMutation clip_mut = {};
  clip_mut.type = kFlutterPlatformViewMutationTypeClipRect;
  clip_mut.clip_rect.left = 0.0;
  clip_mut.clip_rect.top = 0.0;
  clip_mut.clip_rect.right = 200.0;
  clip_mut.clip_rect.bottom = 200.0;

  FlutterPlatformViewMutation opacity_mut = {};
  opacity_mut.type = kFlutterPlatformViewMutationTypeOpacity;
  opacity_mut.opacity = 0.8;

  const FlutterPlatformViewMutation* mutations[] = {&trans_mut, &clip_mut,
                                                    &opacity_mut};
  FlutterPlatformView view = {};
  view.struct_size = sizeof(view);
  // View identifier 104
  view.identifier = 104;
  view.mutations_count = 3;
  view.mutations = mutations;

  MutatorsStack stack = AndroidCompositor::ConvertMutators(&view);
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
  ++iter;
  EXPECT_EQ(iter, stack.End());
}

TEST(AndroidCompositorTest, PresentLayersWithBackingStoresAndPlatformViews) {
  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(context);
  AndroidCompositor compositor(surface_manager, nullptr);

  FlutterCompositor c_compositor = compositor.GetCompositor();

  FlutterBackingStore backing_store = {};
  backing_store.struct_size = sizeof(backing_store);
  backing_store.type = kFlutterBackingStoreTypeOpenGL;

  FlutterPlatformViewMutation trans_mut = {};
  trans_mut.type = kFlutterPlatformViewMutationTypeTransformation;
  trans_mut.transformation.scaleX = 1.0;
  trans_mut.transformation.scaleY = 1.0;
  trans_mut.transformation.pers2 = 1.0;

  const FlutterPlatformViewMutation* mutations[] = {&trans_mut};
  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(platform_view);
  // Platform view identifier 42
  platform_view.identifier = 42;
  platform_view.mutations_count = 1;
  platform_view.mutations = mutations;

  FlutterLayer layer1 = {};
  layer1.struct_size = sizeof(layer1);
  layer1.type = kFlutterLayerContentTypeBackingStore;
  layer1.backing_store = &backing_store;
  // Presentation timestamp 1000000 nanoseconds
  layer1.presentation_time = 1000000;

  FlutterLayer layer2 = {};
  layer2.struct_size = sizeof(layer2);
  layer2.type = kFlutterLayerContentTypePlatformView;
  layer2.platform_view = &platform_view;
  // Platform view positioned at (50, 100), size (300, 400)
  layer2.offset = {50.0, 100.0};
  layer2.size = {300.0, 400.0};

  const FlutterLayer* layers[] = {&layer1, &layer2};

  FlutterPresentViewInfo info = {};
  info.struct_size = sizeof(info);
  // View ID 1
  info.view_id = 1;
  info.layers = layers;
  // Count of 2 layers
  info.layers_count = 2;
  info.user_data = &compositor;

  ASSERT_TRUE(c_compositor.present_view_callback(&info));

  AndroidCompositor::PresentedFrame frame = compositor.GetLastPresentedFrame();
  EXPECT_EQ(frame.view_id, 1);
  EXPECT_EQ(frame.backing_store_count, 1u);
  EXPECT_EQ(frame.platform_view_count, 1u);
  ASSERT_EQ(frame.platform_view_ids.size(), 1u);
  EXPECT_EQ(frame.platform_view_ids[0], 42);
  ASSERT_EQ(frame.platform_view_mutators.size(), 1u);
  EXPECT_EQ(frame.platform_view_mutators[0].stack_count(), 1u);
  EXPECT_EQ(frame.presentation_time, 1000000u);
}

TEST(AndroidCompositorTest, PresentPlatformViewDispatchesToJNI) {
  auto jni_mock = std::make_shared<JNIMock>();

  int captured_view_id = -1;
  int captured_x = -1;
  int captured_y = -1;
  int captured_width = -1;
  int captured_height = -1;
  MutatorsStack captured_stack;

  EXPECT_CALL(*jni_mock, FlutterViewOnDisplayPlatformView(42, 50, 100, 300, 400,
                                                          300, 400, _))
      .WillOnce(DoAll(SaveArg<0>(&captured_view_id), SaveArg<1>(&captured_x),
                      SaveArg<2>(&captured_y), SaveArg<3>(&captured_width),
                      SaveArg<4>(&captured_height),
                      SaveArg<7>(&captured_stack)));

  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(context);
  AndroidCompositor compositor(surface_manager, jni_mock);

  FlutterPlatformViewMutation clip_mut = {};
  clip_mut.type = kFlutterPlatformViewMutationTypeClipRect;
  clip_mut.clip_rect.left = 0.0;
  clip_mut.clip_rect.top = 0.0;
  clip_mut.clip_rect.right = 300.0;
  clip_mut.clip_rect.bottom = 400.0;

  const FlutterPlatformViewMutation* mutations[] = {&clip_mut};
  FlutterPlatformView platform_view = {};
  platform_view.struct_size = sizeof(platform_view);
  // View identifier 42
  platform_view.identifier = 42;
  platform_view.mutations_count = 1;
  platform_view.mutations = mutations;

  FlutterLayer layer = {};
  layer.struct_size = sizeof(layer);
  layer.type = kFlutterLayerContentTypePlatformView;
  layer.platform_view = &platform_view;
  layer.offset = {50.0, 100.0};
  layer.size = {300.0, 400.0};

  const FlutterLayer* layers[] = {&layer};

  FlutterPresentViewInfo info = {};
  info.struct_size = sizeof(info);
  info.view_id = 1;
  info.layers = layers;
  info.layers_count = 1;
  info.user_data = &compositor;

  ASSERT_TRUE(compositor.GetCompositor().present_view_callback(&info));

  EXPECT_EQ(captured_view_id, 42);
  EXPECT_EQ(captured_x, 50);
  EXPECT_EQ(captured_y, 100);
  EXPECT_EQ(captured_width, 300);
  EXPECT_EQ(captured_height, 400);
  EXPECT_EQ(captured_stack.stack_count(), 1u);
}

TEST(AndroidCompositorTest, ViewRegistration) {
  auto context = std::make_shared<MockAndroidContext>(
      AndroidRenderingAPI::kImpellerOpenGLES);
  auto surface_manager = std::make_shared<AndroidSurfaceManager>(context);
  AndroidCompositor compositor(surface_manager, nullptr);

  EXPECT_EQ(compositor.GetViewCount(), 0u);

  compositor.AddView(1);
  compositor.AddView(2);
  EXPECT_EQ(compositor.GetViewCount(), 2u);

  compositor.RemoveView(1);
  EXPECT_EQ(compositor.GetViewCount(), 1u);

  compositor.RemoveView(2);
  EXPECT_EQ(compositor.GetViewCount(), 0u);
}

TEST(AndroidCompositorTest, NullSafety) {
  AndroidCompositor compositor(nullptr, nullptr);

  FlutterBackingStoreConfig config = {};
  config.struct_size = sizeof(config);
  config.size.width = 100;
  config.size.height = 100;

  FlutterBackingStore store = {};
  EXPECT_FALSE(compositor.CreateBackingStore(&config, &store));
  EXPECT_FALSE(compositor.CollectBackingStore(&store));

  FlutterCompositor c_compositor = compositor.GetCompositor();
  EXPECT_FALSE(
      c_compositor.create_backing_store_callback(&config, &store, nullptr));
  EXPECT_FALSE(c_compositor.collect_backing_store_callback(&store, nullptr));
  EXPECT_FALSE(c_compositor.present_view_callback(nullptr));
}

}  // namespace android
}  // namespace testing
}  // namespace flutter
