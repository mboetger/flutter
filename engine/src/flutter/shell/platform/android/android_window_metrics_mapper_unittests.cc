// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_window_metrics_mapper.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace flutter::android::testing {

TEST(AndroidWindowMetricsMapperTest,
     ToFlutterWindowMetricsEventMapsPaddingGestureInsetsTouchSlopAndRadii) {
  AndroidViewportMetrics metrics;
  metrics.device_pixel_ratio = 2.5f;
  metrics.physical_width = 1080;
  metrics.physical_height = 2400;
  metrics.physical_padding_top = 80;
  metrics.physical_padding_right = 10;
  metrics.physical_padding_bottom = 40;
  metrics.physical_padding_left = 15;
  metrics.physical_view_inset_top = 0;
  metrics.physical_view_inset_right = 0;
  metrics.physical_view_inset_bottom = 600;
  metrics.physical_view_inset_left = 0;
  metrics.system_gesture_inset_top = 80;
  metrics.system_gesture_inset_right = 48;
  metrics.system_gesture_inset_bottom = 96;
  metrics.system_gesture_inset_left = 48;
  metrics.physical_touch_slop = 24;
  metrics.physical_min_width = 500;
  metrics.physical_max_width = 1080;
  metrics.physical_min_height = 800;
  metrics.physical_max_height = 2400;
  metrics.physical_display_corner_radius_top_left = 32.0;
  metrics.physical_display_corner_radius_top_right = 34.0;
  metrics.physical_display_corner_radius_bottom_right = 36.0;
  metrics.physical_display_corner_radius_bottom_left = 38.0;
  metrics.display_id = 3;
  metrics.view_id = 7;

  FlutterWindowMetricsEvent event =
      AndroidWindowMetricsMapper::ToFlutterWindowMetricsEvent(metrics);

  EXPECT_EQ(event.struct_size, sizeof(FlutterWindowMetricsEvent));
  EXPECT_EQ(event.width, 1080u);
  EXPECT_EQ(event.height, 2400u);
  EXPECT_DOUBLE_EQ(event.pixel_ratio, 2.5);
  EXPECT_EQ(event.top, 80u);
  EXPECT_EQ(event.left, 15u);
  EXPECT_EQ(event.physical_view_inset_bottom, 600u);
  EXPECT_EQ(event.display_id, 3u);
  EXPECT_EQ(event.view_id, 7);
  EXPECT_TRUE(event.has_constraints);
  EXPECT_EQ(event.min_width_constraint, 500u);
  EXPECT_EQ(event.max_width_constraint, 1080u);
  EXPECT_EQ(event.min_height_constraint, 800u);
  EXPECT_EQ(event.max_height_constraint, 2400u);

  // RFC 410.0000 FlutterEdgeInsets physical_padding
  EXPECT_DOUBLE_EQ(event.physical_padding.left, 15.0);
  EXPECT_DOUBLE_EQ(event.physical_padding.top, 80.0);
  EXPECT_DOUBLE_EQ(event.physical_padding.right, 10.0);
  EXPECT_DOUBLE_EQ(event.physical_padding.bottom, 40.0);

  // RFC 410.0000 FlutterEdgeInsets physical_system_gesture_insets
  EXPECT_DOUBLE_EQ(event.physical_system_gesture_insets.left, 48.0);
  EXPECT_DOUBLE_EQ(event.physical_system_gesture_insets.top, 80.0);
  EXPECT_DOUBLE_EQ(event.physical_system_gesture_insets.right, 48.0);
  EXPECT_DOUBLE_EQ(event.physical_system_gesture_insets.bottom, 96.0);

  // RFC 410.0000 physical_touch_slop
  EXPECT_DOUBLE_EQ(event.physical_touch_slop, 24.0);

  // RFC 410.0000 display_corner_radii pointer
  ASSERT_NE(event.display_corner_radii, nullptr);
  EXPECT_EQ(event.display_corner_radii->struct_size,
            sizeof(FlutterDisplayCornerRadii));
  EXPECT_DOUBLE_EQ(event.display_corner_radii->top_left, 32.0);
  EXPECT_DOUBLE_EQ(event.display_corner_radii->top_right, 34.0);
  EXPECT_DOUBLE_EQ(event.display_corner_radii->bottom_right, 36.0);
  EXPECT_DOUBLE_EQ(event.display_corner_radii->bottom_left, 38.0);
}

TEST(AndroidWindowMetricsMapperTest,
     DefaultCornerRadiiAreNullptrAndDisplayFeaturesArePointerArray) {
  AndroidViewportMetrics metrics;
  metrics.physical_width = 800;
  metrics.physical_height = 600;
  metrics.display_features_bounds = {0, 290, 800, 310, 390, 0, 410, 600};
  metrics.display_features_type = {1, 2};   // kFold, kHinge
  metrics.display_features_state = {2, 1};  // kPostureHalfOpened, kPostureFlat

  MappedWindowMetricsEvent mapped =
      AndroidWindowMetricsMapper::MapViewportMetrics(metrics);
  const FlutterWindowMetricsEvent& event = mapped.event;

  // Default corner radii (-1.0) map to nullptr.
  EXPECT_EQ(event.display_corner_radii, nullptr);
  EXPECT_DOUBLE_EQ(event.physical_touch_slop, -1.0);

  // Display features map to const FlutterDisplayFeature** array of length 2.
  ASSERT_EQ(event.display_features_count, 2u);
  ASSERT_NE(event.display_features, nullptr);

  ASSERT_NE(event.display_features[0], nullptr);
  EXPECT_EQ(event.display_features[0]->struct_size,
            sizeof(FlutterDisplayFeature));
  EXPECT_DOUBLE_EQ(event.display_features[0]->bounds.left, 0.0);
  EXPECT_DOUBLE_EQ(event.display_features[0]->bounds.top, 290.0);
  EXPECT_DOUBLE_EQ(event.display_features[0]->bounds.right, 800.0);
  EXPECT_DOUBLE_EQ(event.display_features[0]->bounds.bottom, 310.0);
  EXPECT_EQ(event.display_features[0]->type, kFlutterDisplayFeatureTypeFold);
  EXPECT_EQ(event.display_features[0]->state,
            kFlutterDisplayFeatureStatePostureHalfOpened);

  ASSERT_NE(event.display_features[1], nullptr);
  EXPECT_EQ(event.display_features[1]->struct_size,
            sizeof(FlutterDisplayFeature));
  EXPECT_DOUBLE_EQ(event.display_features[1]->bounds.left, 390.0);
  EXPECT_DOUBLE_EQ(event.display_features[1]->bounds.top, 0.0);
  EXPECT_DOUBLE_EQ(event.display_features[1]->bounds.right, 410.0);
  EXPECT_DOUBLE_EQ(event.display_features[1]->bounds.bottom, 600.0);
  EXPECT_EQ(event.display_features[1]->type, kFlutterDisplayFeatureTypeHinge);
  EXPECT_EQ(event.display_features[1]->state,
            kFlutterDisplayFeatureStatePostureFlat);

  // Verify copy constructor re-binds pointers to the new instance's storage.
  MappedWindowMetricsEvent copy =
      mapped;  // NOLINT(performance-unnecessary-copy-initialization)
  ASSERT_EQ(copy.event.display_features_count, 2u);
  ASSERT_NE(copy.event.display_features, nullptr);
  EXPECT_NE(copy.event.display_features, mapped.event.display_features);
  EXPECT_NE(copy.event.display_features[0], mapped.event.display_features[0]);
  EXPECT_EQ(copy.event.display_features[0]->type,
            kFlutterDisplayFeatureTypeFold);
  copy.features[0].type = kFlutterDisplayFeatureTypeCutout;
  EXPECT_EQ(mapped.event.display_features[0]->type,
            kFlutterDisplayFeatureTypeFold);
}

}  // namespace flutter::android::testing
