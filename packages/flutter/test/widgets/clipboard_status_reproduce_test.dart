// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('ClipboardStatusNotifier reproduction test', () {
    late int hasStringsCount;
    late bool hasStringsResult;

    setUp(() {
      hasStringsCount = 0;
      hasStringsResult = true;

      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        (MethodCall methodCall) async {
          if (methodCall.method == 'Clipboard.hasStrings') {
            hasStringsCount++;
            return <String, bool>{'value': hasStringsResult};
          }
          if (methodCall.method == 'Clipboard.getData') {
            return <String, dynamic>{'text': 'pasteable text'};
          }
          return null;
        },
      );
    });

    tearDown(() {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        null,
      );
    });

    testWidgets(
      'TextField didUpdateWidget does not trigger excessive Clipboard.hasStrings calls',
      (WidgetTester tester) async {
        final Key textFieldKey = UniqueKey();

        // Pump a TextField.
        await tester.pumpWidget(
          MaterialApp(
            home: Scaffold(
              body: Center(
                child: TextField(
                  key: textFieldKey,
                  decoration: const InputDecoration(hintText: 'First Hint'),
                ),
              ),
            ),
          ),
        );

        // Initially, when the TextField is created and has selection controls (like Material),
        // it checks the clipboard status.
        // Wait for any async clipboard checks to complete.
        await tester.pumpAndSettle();

        // On non-web platforms, it should check once upon init.
        const initialCount = kIsWeb ? 0 : 1;
        expect(hasStringsCount, initialCount);

        // Rebuild the TextField with a different decoration to trigger didUpdateWidget.
        await tester.pumpWidget(
          MaterialApp(
            home: Scaffold(
              body: Center(
                child: TextField(
                  key: textFieldKey,
                  decoration: const InputDecoration(hintText: 'Second Hint'),
                ),
              ),
            ),
          ),
        );

        // Wait for any async clipboard checks.
        await tester.pumpAndSettle();

        // In the buggy code, didUpdateWidget will call clipboardStatus.update(),
        // which unconditionally calls Clipboard.hasStrings again, so hasStringsCount becomes 2 (on non-web).
        // In the fixed code, since the status is already known, it should not call Clipboard.hasStrings again,
        // and hasStringsCount should remain 1 (or 0 on web).
        expect(hasStringsCount, initialCount);
      },
      skip: kIsWeb,
    ); // [intended] Since web bypasses clipboard checks to avoid browser permission prompts.

    testWidgets(
      'TextField still checks clipboard status when showing the toolbar',
      (WidgetTester tester) async {
        final Key textFieldKey = UniqueKey();

        await tester.pumpWidget(
          MaterialApp(
            home: Scaffold(
              body: Center(child: TextField(key: textFieldKey)),
            ),
          ),
        );

        await tester.pumpAndSettle();

        // Initially, 1 call on init (non-web).
        const initialCount = kIsWeb ? 0 : 1;
        expect(hasStringsCount, initialCount);

        // Long press the TextField to show the selection toolbar/handles.
        // This must trigger a clipboard check to ensure the toolbar shows the latest status.
        await tester.longPress(find.byKey(textFieldKey));
        await tester.pumpAndSettle();

        // The count should have incremented, indicating a fresh clipboard query was performed.
        expect(hasStringsCount, greaterThan(initialCount));
      },
      skip: kIsWeb,
    ); // [intended] Since web bypasses clipboard checks to avoid browser permission prompts.
  });
}
