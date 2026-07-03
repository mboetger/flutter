// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Verify how system bottom navigation bar height is exposed', (
    WidgetTester tester,
  ) async {
    final double devicePixelRatio = tester.view.devicePixelRatio;

    // Simulate a device with:
    // - Status bar height: 24.0 (top padding)
    // - Bottom navigation bar height: 48.0 (bottom padding)
    tester.view.viewPadding = FakeViewPadding(
      top: 24.0 * devicePixelRatio,
      bottom: 48.0 * devicePixelRatio,
    );
    tester.view.padding = FakeViewPadding(
      top: 24.0 * devicePixelRatio,
      bottom: 48.0 * devicePixelRatio,
    );

    addTearDown(() {
      tester.view.resetViewPadding();
      tester.view.resetPadding();
    });

    late MediaQueryData mediaQueryData;

    await tester.pumpWidget(
      Builder(
        builder: (BuildContext context) {
          mediaQueryData = MediaQuery.of(context);
          return const SizedBox();
        },
      ),
    );

    // 1. Verify that we can get the bottom navigation bar height via `viewPadding.bottom`
    // and `padding.bottom` when no keyboard is shown.
    expect(mediaQueryData.padding.bottom, 48.0);
    expect(mediaQueryData.viewPadding.bottom, 48.0);

    // 2. Simulate keyboard showing (viewInsets.bottom = 300.0)
    tester.view.viewInsets = FakeViewPadding(bottom: 300.0 * devicePixelRatio);
    // When keyboard shows, the framework calculates padding as max(0.0, viewPadding - viewInsets).
    // So padding.bottom becomes 0.0.
    // But viewPadding.bottom should remain 48.0 (on API 30+).
    tester.view.padding = FakeViewPadding(top: 24.0 * devicePixelRatio);

    addTearDown(() {
      tester.view.resetViewInsets();
    });

    await tester.pump();

    mediaQueryData = MediaQuery.of(tester.element(find.byType(SizedBox)));

    // Verify that `padding.bottom` is now 0.0 (consumed by keyboard).
    expect(mediaQueryData.padding.bottom, 0.0);

    // Verify that `viewPadding.bottom` still holds the bottom navigation bar height (48.0).
    expect(mediaQueryData.viewPadding.bottom, 48.0);
  });
}
