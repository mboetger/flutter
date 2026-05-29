// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:integration_ui/sys_ui_mode.dart' as app;

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  group('SystemUiMode Integration Tests', () {
    testWidgets('Switching from immersiveSticky to edgeToEdge restores padding', (
      WidgetTester tester,
    ) async {
      runApp(const app.SysUiModeApp());
      await tester.pumpAndSettle();

      // 1. Ensure we start in edgeToEdge mode and record initial padding.
      await SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
      await tester.pumpAndSettle();
      // Give it a moment to propagate to the platform and back.
      await Future<void>.delayed(const Duration(milliseconds: 500));
      await tester.pumpAndSettle();

      final EdgeInsets initialPadding = _getPadding(tester);
      print('Initial padding (edgeToEdge): $initialPadding');

      // If the device doesn't have system bars (e.g. some headless environments),
      // we cannot meaningfully test transitions via padding.
      if (initialPadding.top == 0 && initialPadding.bottom == 0) {
        print('Skipping test: Device has no status or navigation bar padding.');
        return;
      }

      // 2. Switch to immersiveSticky (should hide system bars and clear padding).
      print('Switching to immersiveSticky...');
      await SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);

      // Wait for padding to become 0 (with a timeout).
      var cleared = false;
      for (var i = 0; i < 10; i++) {
        await tester.pumpAndSettle();
        await Future<void>.delayed(const Duration(milliseconds: 200));
        await tester.pumpAndSettle();
        final EdgeInsets padding = _getPadding(tester);
        print('Current padding: $padding');
        if (padding.top == 0 && padding.bottom == 0) {
          cleared = true;
          break;
        }
      }
      expect(cleared, isTrue, reason: 'System padding should be cleared in immersiveSticky mode.');

      // 3. Switch back to edgeToEdge (should restore system bars and padding).
      print('Switching back to edgeToEdge...');
      await SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);

      // Wait for padding to restore to initial values (with a timeout).
      var restored = false;
      EdgeInsets finalPadding = EdgeInsets.zero;
      for (var i = 0; i < 10; i++) {
        await tester.pumpAndSettle();
        await Future<void>.delayed(const Duration(milliseconds: 200));
        await tester.pumpAndSettle();
        finalPadding = _getPadding(tester);
        print('Current padding: $finalPadding');
        if (finalPadding.top == initialPadding.top &&
            finalPadding.bottom == initialPadding.bottom) {
          restored = true;
          break;
        }
      }

      expect(
        restored,
        isTrue,
        reason:
            'System padding should be restored when switching back to edgeToEdge.\n'
            'Expected: $initialPadding, Got: $finalPadding\n'
            'This indicates a regression where system bars failed to show again (Issue #186723).',
      );
    });
  });
}

EdgeInsets _getPadding(WidgetTester tester) {
  final BuildContext context = tester.element(find.byType(app.SysUiModeApp));
  return MediaQuery.paddingOf(context);
}
