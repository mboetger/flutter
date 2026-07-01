// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

Finder _findTooltipContainer(String tooltipText) {
  return find.ancestor(of: find.text(tooltipText), matching: find.byType(Container));
}

void main() {
  testWidgets('Tooltip default verticalOffset on Android is 32.0 (reproduce #25478)', (
    WidgetTester tester,
  ) async {
    final tooltipKey = GlobalKey<TooltipState>();

    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(platform: TargetPlatform.android),
        home: Align(
          alignment: Alignment.topLeft,
          child: Tooltip(
            key: tooltipKey,
            message: 'Back',
            preferBelow: true,
            child: const SizedBox.shrink(),
          ),
        ),
      ),
    );

    tooltipKey.currentState!.ensureTooltipVisible();
    await tester.pump(const Duration(seconds: 2)); // Wait for fade-in animation to complete

    final RenderBox tip = tester.renderObject(_findTooltipContainer('Back'));
    final Offset tipInGlobal = tip.localToGlobal(tip.size.topLeft(Offset.zero));
    // Without the fix, the default verticalOffset is 24.0, so tipInGlobal.dy is 24.0.
    // The expected behavior on Android is a verticalOffset of 32.0.
    expect(tipInGlobal.dy, equals(32.0));
  });

  testWidgets('Tooltip default verticalOffset on macOS is 24.0', (WidgetTester tester) async {
    final tooltipKey = GlobalKey<TooltipState>();

    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(platform: TargetPlatform.macOS),
        home: Align(
          alignment: Alignment.topLeft,
          child: Tooltip(
            key: tooltipKey,
            message: 'Back',
            preferBelow: true,
            child: const SizedBox.shrink(),
          ),
        ),
      ),
    );

    tooltipKey.currentState!.ensureTooltipVisible();
    await tester.pump(const Duration(seconds: 2)); // Wait for fade-in animation to complete

    final RenderBox tip = tester.renderObject(_findTooltipContainer('Back'));
    final Offset tipInGlobal = tip.localToGlobal(tip.size.topLeft(Offset.zero));
    // The expected behavior on macOS is a verticalOffset of 24.0.
    expect(tipInGlobal.dy, equals(24.0));
  });
}
