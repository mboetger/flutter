// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "flutter/fml/message_loop.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/platform/android/android_engine.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

// PARITY CONTRACT:
// The following 30 parameters are passed from Java
// FlutterJNI.nativeSetViewportMetrics to the native Android engine and mapped
// to flutter::ViewportMetrics. In the Embedder API migration (Stage 1: T-1.8),
// FlutterWindowMetricsEvent must be extended to carry all of these fields so
// that no Android display insets, gestures, or cutout features are dropped.
//
//  1. nativeShellHolderId
//  2. devicePixelRatio -> device_pixel_ratio
//  3. physicalWidth -> physical_width
//  4. physicalHeight -> physical_height
//  5. physicalPaddingTop -> physical_padding_top
//  6. physicalPaddingRight -> physical_padding_right
//  7. physicalPaddingBottom -> physical_padding_bottom
//  8. physicalPaddingLeft -> physical_padding_left
//  9. physicalViewInsetTop -> physical_view_inset_top
// 10. physicalViewInsetRight -> physical_view_inset_right
// 11. physicalViewInsetBottom -> physical_view_inset_bottom
// 12. physicalViewInsetLeft -> physical_view_inset_left
// 13. systemGestureInsetTop -> physical_system_gesture_inset_top
// 14. systemGestureInsetRight -> physical_system_gesture_inset_right
// 15. systemGestureInsetBottom -> physical_system_gesture_inset_bottom
// 16. systemGestureInsetLeft -> physical_system_gesture_inset_left
// 17. physicalTouchSlop -> physical_touch_slop
// 18. displayFeaturesBounds -> physical_display_features_bounds
// 19. displayFeaturesType -> physical_display_features_type
// 20. displayFeaturesState -> physical_display_features_state
// 21. physicalMinWidth -> physical_min_width_constraint
// 22. physicalMaxWidth -> physical_max_width_constraint
// 23. physicalMinHeight -> physical_min_height_constraint
// 24. physicalMaxHeight -> physical_max_height_constraint
// 25. physicalDisplayCornerRadiusTopLeft ->
// physical_display_corner_radius_top_left
// 26. physicalDisplayCornerRadiusTopRight ->
// physical_display_corner_radius_top_right
// 27. physicalDisplayCornerRadiusBottomRight ->
// physical_display_corner_radius_bottom_right
// 28. physicalDisplayCornerRadiusBottomLeft ->
// physical_display_corner_radius_bottom_left
// 29. JNIEnv* env
// 30. jobject jcaller

TEST(AndroidViewportMetricsTest, All30ParametersIndividualFidelity) {
  TRACE_EVENT0("flutter", "All30ParametersIndividualFidelity");
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<JNIMock>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  // Distinct non-default test values for each parameter
  const double kDevicePixelRatio = 2.75;
  const double kPhysicalWidth = 1080.0;
  const double kPhysicalHeight = 2400.0;
  const double kPhysicalMinWidth = 320.0;
  const double kPhysicalMaxWidth = 1440.0;
  const double kPhysicalMinHeight = 480.0;
  const double kPhysicalMaxHeight = 3200.0;
  const double kPaddingTop = 84.0;
  const double kPaddingRight = 12.0;
  const double kPaddingBottom = 48.0;
  const double kPaddingLeft = 16.0;
  const double kViewInsetTop = 10.0;
  const double kViewInsetRight = 20.0;
  const double kViewInsetBottom = 30.0;
  const double kViewInsetLeft = 40.0;
  const double kSystemGestureInsetTop = 50.0;
  const double kSystemGestureInsetRight = 60.0;
  const double kSystemGestureInsetBottom = 70.0;
  const double kSystemGestureInsetLeft = 80.0;
  const double kTouchSlop = 24.0;
  const std::vector<double> kDisplayFeaturesBounds = {0, 1000, 1080, 1050};
  const std::vector<int> kDisplayFeaturesType = {1};   // Fold
  const std::vector<int> kDisplayFeaturesState = {2};  // Half-opened
  const size_t kDisplayId = 0;
  const double kCornerRadiusTopLeft = 15.0;
  const double kCornerRadiusTopRight = 16.0;
  const double kCornerRadiusBottomRight = 17.0;
  const double kCornerRadiusBottomLeft = 18.0;

  const flutter::ViewportMetrics metrics{
      kDevicePixelRatio,          // 2: devicePixelRatio
      kPhysicalWidth,             // 3: physicalWidth
      kPhysicalHeight,            // 4: physicalHeight
      kPhysicalMinWidth,          // 21: physicalMinWidth
      kPhysicalMaxWidth,          // 22: physicalMaxWidth
      kPhysicalMinHeight,         // 23: physicalMinHeight
      kPhysicalMaxHeight,         // 24: physicalMaxHeight
      kPaddingTop,                // 5: physicalPaddingTop
      kPaddingRight,              // 6: physicalPaddingRight
      kPaddingBottom,             // 7: physicalPaddingBottom
      kPaddingLeft,               // 8: physicalPaddingLeft
      kViewInsetTop,              // 9: physicalViewInsetTop
      kViewInsetRight,            // 10: physicalViewInsetRight
      kViewInsetBottom,           // 11: physicalViewInsetBottom
      kViewInsetLeft,             // 12: physicalViewInsetLeft
      kSystemGestureInsetTop,     // 13: systemGestureInsetTop
      kSystemGestureInsetRight,   // 14: systemGestureInsetRight
      kSystemGestureInsetBottom,  // 15: systemGestureInsetBottom
      kSystemGestureInsetLeft,    // 16: systemGestureInsetLeft
      kTouchSlop,                 // 17: physicalTouchSlop
      kDisplayFeaturesBounds,     // 18: displayFeaturesBounds
      kDisplayFeaturesType,       // 19: displayFeaturesType
      kDisplayFeaturesState,      // 20: displayFeaturesState
      kDisplayId,                 // display_id
      kCornerRadiusTopLeft,       // 25: physicalDisplayCornerRadiusTopLeft
      kCornerRadiusTopRight,      // 26: physicalDisplayCornerRadiusTopRight
      kCornerRadiusBottomRight,   // 27: physicalDisplayCornerRadiusBottomRight
      kCornerRadiusBottomLeft,    // 28: physicalDisplayCornerRadiusBottomLeft
  };

  // Assert individual parameters arrive intact:
  // Parameter 2
  EXPECT_DOUBLE_EQ(metrics.device_pixel_ratio, kDevicePixelRatio);
  // Parameter 3
  EXPECT_DOUBLE_EQ(metrics.physical_width, kPhysicalWidth);
  // Parameter 4
  EXPECT_DOUBLE_EQ(metrics.physical_height, kPhysicalHeight);
  // Parameter 5
  EXPECT_DOUBLE_EQ(metrics.physical_padding_top, kPaddingTop);
  // Parameter 6
  EXPECT_DOUBLE_EQ(metrics.physical_padding_right, kPaddingRight);
  // Parameter 7
  EXPECT_DOUBLE_EQ(metrics.physical_padding_bottom, kPaddingBottom);
  // Parameter 8
  EXPECT_DOUBLE_EQ(metrics.physical_padding_left, kPaddingLeft);
  // Parameter 9
  EXPECT_DOUBLE_EQ(metrics.physical_view_inset_top, kViewInsetTop);
  // Parameter 10
  EXPECT_DOUBLE_EQ(metrics.physical_view_inset_right, kViewInsetRight);
  // Parameter 11
  EXPECT_DOUBLE_EQ(metrics.physical_view_inset_bottom, kViewInsetBottom);
  // Parameter 12
  EXPECT_DOUBLE_EQ(metrics.physical_view_inset_left, kViewInsetLeft);
  // Parameter 13
  EXPECT_DOUBLE_EQ(metrics.physical_system_gesture_inset_top,
                   kSystemGestureInsetTop);
  // Parameter 14
  EXPECT_DOUBLE_EQ(metrics.physical_system_gesture_inset_right,
                   kSystemGestureInsetRight);
  // Parameter 15
  EXPECT_DOUBLE_EQ(metrics.physical_system_gesture_inset_bottom,
                   kSystemGestureInsetBottom);
  // Parameter 16
  EXPECT_DOUBLE_EQ(metrics.physical_system_gesture_inset_left,
                   kSystemGestureInsetLeft);
  // Parameter 17
  EXPECT_DOUBLE_EQ(metrics.physical_touch_slop, kTouchSlop);
  // Parameter 18
  EXPECT_EQ(metrics.physical_display_features_bounds, kDisplayFeaturesBounds);
  // Parameter 19
  EXPECT_EQ(metrics.physical_display_features_type, kDisplayFeaturesType);
  // Parameter 20
  EXPECT_EQ(metrics.physical_display_features_state, kDisplayFeaturesState);
  // Parameter 21
  EXPECT_DOUBLE_EQ(metrics.physical_min_width_constraint, kPhysicalMinWidth);
  // Parameter 22
  EXPECT_DOUBLE_EQ(metrics.physical_max_width_constraint, kPhysicalMaxWidth);
  // Parameter 23
  EXPECT_DOUBLE_EQ(metrics.physical_min_height_constraint, kPhysicalMinHeight);
  // Parameter 24
  EXPECT_DOUBLE_EQ(metrics.physical_max_height_constraint, kPhysicalMaxHeight);
  // Parameter 25
  EXPECT_DOUBLE_EQ(metrics.physical_display_corner_radius_top_left,
                   kCornerRadiusTopLeft);
  // Parameter 26
  EXPECT_DOUBLE_EQ(metrics.physical_display_corner_radius_top_right,
                   kCornerRadiusTopRight);
  // Parameter 27
  EXPECT_DOUBLE_EQ(metrics.physical_display_corner_radius_bottom_right,
                   kCornerRadiusBottomRight);
  // Parameter 28
  EXPECT_DOUBLE_EQ(metrics.physical_display_corner_radius_bottom_left,
                   kCornerRadiusBottomLeft);

  // Deliver to platform view
  platform_view->SetViewportMetrics(0, metrics);
}

}  // namespace testing
}  // namespace flutter
