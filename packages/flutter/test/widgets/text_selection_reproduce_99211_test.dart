// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'editable_text_utils.dart';

void main() {
  testWidgets(
    'android: dragging after long press on a focused text field should move the cursor/selection (collapsed) instead of selecting a word range',
    (WidgetTester tester) async {
      final controller = TextEditingController(text: 'first second third');
      final focusNode = FocusNode();
      addTearDown(controller.dispose);
      addTearDown(focusNode.dispose);

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Center(
              child: TextField(controller: controller, focusNode: focusNode),
            ),
          ),
        ),
      );

      // 1. Focus the TextField.
      await tester.tap(find.byType(TextField));
      await tester.pumpAndSettle();

      expect(focusNode.hasFocus, isTrue);

      // Place cursor at the end.
      controller.selection = const TextSelection.collapsed(offset: 18);
      await tester.pumpAndSettle();

      // 2. Get the positions of the words.
      final Offset secondWordPosition = textOffsetToPosition(tester, 8); // 'second'
      final Offset thirdWordPosition = textOffsetToPosition(tester, 15); // 'third'

      // 3. Perform a long-press gesture on 'second' and drag to 'third'.
      final TestGesture gesture = await tester.startGesture(secondWordPosition);
      await tester.pump(const Duration(milliseconds: 600)); // Trigger long press.

      // Drag to 'third'.
      await gesture.moveTo(thirdWordPosition);
      await tester.pump();
      await gesture.up();
      await tester.pumpAndSettle();

      // Expected behavior (Samsung/One UI & general movable cursor preference):
      // The cursor should move to the dragged position ('third' / offset 15),
      // and the selection should be collapsed.
      expect(controller.selection.isCollapsed, isTrue);
      expect(controller.selection.baseOffset, 15);
    },
    variant: TargetPlatformVariant.only(TargetPlatform.android),
  );

  testWidgets(
    'android: dragging after long press on an unfocused text field should select a word range (not collapsed)',
    (WidgetTester tester) async {
      final controller = TextEditingController(text: 'first second third');
      final focusNode = FocusNode();
      addTearDown(controller.dispose);
      addTearDown(focusNode.dispose);

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Center(
              child: TextField(controller: controller, focusNode: focusNode),
            ),
          ),
        ),
      );

      // Verify text field starts unfocused.
      expect(focusNode.hasFocus, isFalse);

      // Get the positions of the words.
      final Offset secondWordPosition = textOffsetToPosition(tester, 8); // 'second'
      final Offset thirdWordPosition = textOffsetToPosition(tester, 15); // 'third'

      // Perform a long-press gesture on 'second' (when unfocused) and drag to 'third'.
      final TestGesture gesture = await tester.startGesture(secondWordPosition);
      await tester.pump(const Duration(milliseconds: 600)); // Trigger long press.

      // Drag to 'third'.
      await gesture.moveTo(thirdWordPosition);
      await tester.pumpAndSettle();
      await gesture.up();
      await tester.pumpAndSettle();

      // Expected behavior:
      // Since it started without focus, it should select the word range ('second third').
      expect(controller.selection.isCollapsed, isFalse);
      expect(controller.selection.baseOffset, 6); // Start of 'second'
      expect(controller.selection.extentOffset, 18); // End of 'third'
    },
    variant: TargetPlatformVariant.only(TargetPlatform.android),
  );
}
