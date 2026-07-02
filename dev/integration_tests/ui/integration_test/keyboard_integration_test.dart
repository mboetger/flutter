// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:integration_ui/keyboard_textfield.dart' as app;
import 'package:integration_ui/keys.dart' as keys;

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  Future<void> setupApp(WidgetTester tester) async {
    await tester.pumpWidget(const app.MyApp());
    await tester.pumpAndSettle();

    final Finder textFieldFinder = find.byKey(
      const ValueKey<String>(keys.kDefaultTextField),
      skipOffstage: false,
    );

    // Scroll until the TextField is visible.
    await tester.scrollUntilVisible(textFieldFinder, 100.0);
    await tester.pumpAndSettle();

    // Wait for the native window to gain focus.
    await Future<void>.delayed(const Duration(seconds: 2));
    await tester.pumpAndSettle();
  }

  Future<void> waitForKeyboardToShow(WidgetTester tester) async {
    final DateTime endTime = DateTime.now().add(const Duration(seconds: 15));
    while (tester.view.viewInsets.bottom == 0.0) {
      if (DateTime.now().isAfter(endTime)) {
        throw StateError(
          'Keyboard did not appear. '
          'ViewInsets: ${tester.view.viewInsets}',
        );
      }
      await Future<void>.delayed(const Duration(milliseconds: 100));
      tester.binding.handleMetricsChanged();
      await tester.pump();
    }
  }

  Future<void> waitForKeyboardToHide(WidgetTester tester) async {
    final DateTime endTime = DateTime.now().add(const Duration(seconds: 15));
    while (tester.view.viewInsets.bottom > 0.0) {
      if (DateTime.now().isAfter(endTime)) {
        throw StateError(
          'Keyboard did not disappear. '
          'ViewInsets: ${tester.view.viewInsets}',
        );
      }
      await Future<void>.delayed(const Duration(milliseconds: 100));
      tester.binding.handleMetricsChanged();
      await tester.pump();
    }
  }

  Future<void> simulateTextInputFromPlatform({
    required WidgetTester tester,
    required String text,
    required TextSelection selection,
    TextRange composing = TextRange.empty,
  }) async {
    final arguments = <dynamic>[
      -1, // client ID (magical -1 works in debug mode)
      <String, dynamic>{
        'text': text,
        'selectionBase': selection.baseOffset,
        'selectionExtent': selection.extentOffset,
        'selectionAffinity': selection.affinity.toString(),
        'selectionIsDirectional': selection.isDirectional,
        'composingBase': composing.start,
        'composingExtent': composing.end,
      },
    ];

    await tester.binding.defaultBinaryMessenger.handlePlatformMessage(
      SystemChannels.textInput.name,
      SystemChannels.textInput.codec.encodeMethodCall(
        MethodCall('TextInputClient.updateEditingState', arguments),
      ),
      (ByteData? data) {},
    );
  }

  group('Keyboard Integration Tests', () {
    testWidgets('Focusing and unfocusing TextField updates keyboard visibility', (
      WidgetTester tester,
    ) async {
      await setupApp(tester);

      final Finder textFieldFinder = find.byKey(const ValueKey<String>(keys.kDefaultTextField));
      expect(textFieldFinder, findsOneWidget);

      // Verify initially no keyboard is visible
      expect(tester.view.viewInsets.bottom, 0.0);
      expect(find.byKey(const ValueKey<String>(keys.kKeyboardVisibleView)), findsNothing);

      // 1. Focus the TextField
      await tester.tap(textFieldFinder);
      await tester.pumpAndSettle();

      // Verify TextField has focus (via its descendant EditableText)
      final EditableTextState editableTextState = tester.state<EditableTextState>(
        find.byType(EditableText),
      );
      expect(editableTextState.widget.focusNode.hasFocus, isTrue);

      // Verify keyboard is shown
      await waitForKeyboardToShow(tester);
      expect(tester.view.viewInsets.bottom, greaterThan(0.0));
      expect(find.byKey(const ValueKey<String>(keys.kKeyboardVisibleView)), findsOneWidget);

      // 2. Unfocus the TextField
      editableTextState.widget.focusNode.unfocus();
      await tester.pumpAndSettle();

      // Verify TextField lost focus
      expect(editableTextState.widget.focusNode.hasFocus, isFalse);

      // Verify keyboard is hidden
      await waitForKeyboardToHide(tester);
      expect(tester.view.viewInsets.bottom, 0.0);
      expect(find.byKey(const ValueKey<String>(keys.kKeyboardVisibleView)), findsNothing);
    });

    testWidgets('Typing and selection/composing region tracking', (WidgetTester tester) async {
      await setupApp(tester);

      final Finder textFieldFinder = find.byKey(const ValueKey<String>(keys.kDefaultTextField));
      await tester.tap(textFieldFinder);
      await tester.pumpAndSettle();

      final EditableTextState editableTextState = tester.state<EditableTextState>(
        find.byType(EditableText),
      );

      // Mock the channel to intercept outgoing messages and prevent the real IME from overriding.
      tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(SystemChannels.textInput, (
        MethodCall message,
      ) async {
        return null;
      });

      // Simulate typing 'Hello' with 'Hello' as composing region.
      // The selection must be within the composing region, otherwise the
      // TextEditingController will clear the composing region.
      const testText = 'Hello';
      const selection = TextSelection.collapsed(offset: 5);
      const composing = TextRange(start: 0, end: 5);

      await simulateTextInputFromPlatform(
        tester: tester,
        text: testText,
        selection: selection,
        composing: composing,
      );
      await tester.pumpAndSettle();

      // Verify the framework updated its state
      expect(editableTextState.textEditingValue.text, testText);
      expect(editableTextState.textEditingValue.selection, selection);
      expect(editableTextState.textEditingValue.composing, composing);

      // Simulate updating typing to 'Hello World' with no composing region
      const testText2 = 'Hello World';
      const selection2 = TextSelection.collapsed(offset: 11);

      await simulateTextInputFromPlatform(tester: tester, text: testText2, selection: selection2);
      await tester.pumpAndSettle();

      // Verify the framework updated its state again
      expect(editableTextState.textEditingValue.text, testText2);
      expect(editableTextState.textEditingValue.selection, selection2);
      expect(editableTextState.textEditingValue.composing, TextRange.empty);

      // Restore the channel handler
      tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.textInput,
        null,
      );
    });
  });
}
