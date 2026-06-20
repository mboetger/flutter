// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets(
    'Delete button of Samsung keyboard is not deleting text when built without WidgetsApp',
    (WidgetTester tester) async {
      final controller = TextEditingController(text: 'abc');
      addTearDown(controller.dispose);

      await tester.pumpWidget(
        Directionality(
          textDirection: TextDirection.ltr,
          child: Localizations(
            locale: const Locale('en', 'US'),
            delegates: const <LocalizationsDelegate<dynamic>>[
              DefaultWidgetsLocalizations.delegate,
              DefaultMaterialLocalizations.delegate,
            ],
            child: Material(
              child: Overlay(
                initialEntries: <OverlayEntry>[
                  OverlayEntry(
                    builder: (BuildContext context) {
                      return Center(child: TextField(controller: controller, autofocus: true));
                    },
                  ),
                ],
              ),
            ),
          ),
        ),
      );

      // Ensure the text field is focused and cursor is positioned at the end of 'abc'
      await tester.pump();
      final Finder textField = find.byType(TextField);
      await tester.tap(textField);
      await tester.pump();

      expect(controller.text, 'abc');
      expect(controller.selection.baseOffset, 3);
      expect(controller.selection.extentOffset, 3);

      // Send a backspace key event, simulating Samsung keyboard sending a physical key event
      await tester.sendKeyEvent(LogicalKeyboardKey.backspace);
      await tester.pump();

      // Verify that the backspace key deletes the character, updating the text to 'ab'
      expect(controller.text, 'ab');
    },
  );

  testWidgets('Forward delete key is not deleting text when built without WidgetsApp', (
    WidgetTester tester,
  ) async {
    final controller = TextEditingController(text: 'abc');
    addTearDown(controller.dispose);

    await tester.pumpWidget(
      Directionality(
        textDirection: TextDirection.ltr,
        child: Localizations(
          locale: const Locale('en', 'US'),
          delegates: const <LocalizationsDelegate<dynamic>>[
            DefaultWidgetsLocalizations.delegate,
            DefaultMaterialLocalizations.delegate,
          ],
          child: Material(
            child: Overlay(
              initialEntries: <OverlayEntry>[
                OverlayEntry(
                  builder: (BuildContext context) {
                    return Center(child: TextField(controller: controller, autofocus: true));
                  },
                ),
              ],
            ),
          ),
        ),
      ),
    );

    // Ensure the text field is focused
    await tester.pump();
    final Finder textField = find.byType(TextField);
    await tester.tap(textField);
    await tester.pump();

    // Place the cursor at the beginning of 'abc'
    controller.selection = const TextSelection.collapsed(offset: 0);
    await tester.pump();

    expect(controller.text, 'abc');
    expect(controller.selection.baseOffset, 0);
    expect(controller.selection.extentOffset, 0);

    // Send a delete key event (forward delete)
    await tester.sendKeyEvent(LogicalKeyboardKey.delete);
    await tester.pump();

    // Verify that the delete key deletes the character in front of the cursor, updating the text to 'bc'
    expect(controller.text, 'bc');
  });
}
