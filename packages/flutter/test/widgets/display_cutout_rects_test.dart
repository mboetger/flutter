// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('DisplayCutout Rects (flutter/flutter#65088)', () {
    testWidgets(
      'current behavior: cutouts are exposed as raw Rect bounds without edge/direction association',
      (WidgetTester tester) async {
        // Simulate an Android R (API 30) device with two display cutouts:
        // A top camera notch and a left waterfall edge cutout.
        const topCutout = Rect.fromLTRB(100.0, 0.0, 300.0, 80.0);
        const leftCutout = Rect.fromLTRB(0.0, 100.0, 30.0, 200.0);

        const displayFeatures = <DisplayFeature>[
          DisplayFeature(
            bounds: topCutout,
            type: DisplayFeatureType.cutout,
            state: DisplayFeatureState.unknown,
          ),
          DisplayFeature(
            bounds: leftCutout,
            type: DisplayFeatureType.cutout,
            state: DisplayFeatureState.unknown,
          ),
        ];

        const mediaQuery = MediaQueryData(
          size: Size(1080.0, 2400.0),
          displayFeatures: displayFeatures,
        );

        // Verify current behavior:
        // 1. Cutouts are stored in an unordered list of DisplayFeatures.
        expect(mediaQuery.displayFeatures.length, 2);
        expect(mediaQuery.displayFeatures[0].type, DisplayFeatureType.cutout);
        expect(mediaQuery.displayFeatures[0].bounds, topCutout);
        expect(mediaQuery.displayFeatures[1].type, DisplayFeatureType.cutout);
        expect(mediaQuery.displayFeatures[1].bounds, leftCutout);

        // 2. DisplayFeature only contains bounds, type, and state.
        // For cutouts, state is always DisplayFeatureState.unknown and there is no
        // directional or edge association property (e.g., top, bottom, left, right).
        expect(mediaQuery.displayFeatures[0].state, DisplayFeatureState.unknown);
        expect(mediaQuery.displayFeatures[1].state, DisplayFeatureState.unknown);
      },
    );

    testWidgets('querying directional cutout bounding rects (Android R getBoundingRect<direction>)', (
      WidgetTester tester,
    ) async {
      const topCutout = Rect.fromLTRB(100.0, 0.0, 300.0, 80.0);
      const leftCutout = Rect.fromLTRB(0.0, 100.0, 30.0, 200.0);
      const topFeature = DisplayFeature(
        bounds: topCutout,
        type: DisplayFeatureType.cutout,
        state: DisplayFeatureState.cutoutTop,
      );
      const leftFeature = DisplayFeature(
        bounds: leftCutout,
        type: DisplayFeatureType.cutout,
        state: DisplayFeatureState.cutoutLeft,
      );

      // In Android R (API 30), DisplayCutout provides directional bounding rect APIs:
      // getBoundingRectTop(), getBoundingRectBottom(), getBoundingRectLeft(), getBoundingRectRight().
      // This allows apps to know specifically which screen edge is obscured by which cutout rect.
      //
      // In Flutter, DisplayFeature exposes directional cutout rects via DisplayFeatureState
      // (e.g., cutoutTop, cutoutBottom, cutoutLeft, cutoutRight).
      expect(
        topFeature.state,
        isNot(DisplayFeatureState.unknown),
        reason:
            'DisplayFeature should expose directional/edge metadata corresponding to Android R getBoundingRect<direction>() rather than unknown state.',
      );
      expect(topFeature.state, DisplayFeatureState.cutoutTop);
      expect(leftFeature.state, DisplayFeatureState.cutoutLeft);
    });
  });
}
