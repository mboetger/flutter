// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';

import 'editable_text_utils.dart' show findRenderEditable, globalize, textOffsetToPosition;

void main() {
  testWidgets('Selection handles should be invisible when magnifier is showing on Android', (
    WidgetTester tester,
  ) async {
    final controller = TextEditingController(text: 'abc def ghi');
    addTearDown(controller.dispose);

    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(platform: TargetPlatform.android),
        home: Scaffold(
          body: Center(child: TextField(controller: controller)),
        ),
      ),
    );

    // Focus the TextField.
    await tester.tap(find.byType(TextField));
    await tester.pumpAndSettle();

    const testValue = 'abc def ghi';
    final int indexOfE = testValue.indexOf('e');

    // Double tap to select 'def'.
    await tester.tapAt(textOffsetToPosition(tester, indexOfE));
    await tester.pump(const Duration(milliseconds: 30));
    await tester.tapAt(textOffsetToPosition(tester, indexOfE));
    await tester.pumpAndSettle();

    expect(controller.selection.textInside(testValue), 'def');

    final RenderEditable renderEditable = findRenderEditable(tester);
    final List<TextSelectionPoint> endpoints = globalize(
      renderEditable.getEndpointsForSelection(controller.selection),
      renderEditable,
    );
    expect(endpoints.length, 2);

    // Find the right selection handle position.
    final Offset rightHandlePosition = endpoints.last.point + const Offset(1.0, 1.0);

    // Start dragging the right handle.
    final TestGesture gesture = await tester.startGesture(rightHandlePosition);
    await tester.pump(); // Start of drag

    // Move a larger distance to exceed touch slop.
    await gesture.moveBy(const Offset(30.0, 0.0));
    await tester.pump();

    // The magnifier should be visible.
    expect(find.byType(RawMagnifier), findsOneWidget);

    // Find the selection handle overlays precisely.
    final Finder handleFinder = find.byWidgetPredicate(
      (Widget widget) => widget.runtimeType.toString() == '_SelectionHandleOverlay',
    );

    // Find the FadeTransitions that belong to the selection handles.
    final List<FadeTransition> fadeTransitions = tester
        .widgetList<FadeTransition>(
          find.descendant(of: handleFinder, matching: find.byType(FadeTransition)),
        )
        .toList();

    // The bug is that they are visible (opacity > 0, likely 1.0).
    // We expect the reproduction test to fail when we assert that they are invisible (opacity 0.0).
    expect(fadeTransitions, isNotEmpty);
    for (final t in fadeTransitions) {
      expect(
        t.opacity.value,
        0.0,
        reason:
            'Selection handle is visible (opacity ${t.opacity.value}) while magnifier is active!',
      );
    }

    await gesture.up();
    await tester.pumpAndSettle();
  }, variant: TargetPlatformVariant.only(TargetPlatform.android));
}
