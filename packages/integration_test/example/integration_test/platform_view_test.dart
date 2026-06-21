// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:integration_test_example/main.dart' as app;

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('verify platform view is present', (WidgetTester tester) async {
    // Mock the platform views channel ONLY for the headless flutter_tester environment.
    // On a real device (where Platform.isAndroid/isIOS is true), we want to test the
    // actual native platform view implementation.
    final bool isHeadless = !kIsWeb && !(Platform.isAndroid || Platform.isIOS);
    if (isHeadless) {
      tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform_views,
        (MethodCall methodCall) async {
          if (methodCall.method == 'create') {
            return 0; // Return a fake texture ID.
          }
          return null;
        },
      );
    }

    app.main();
    await tester.pumpAndSettle();

    if (!kIsWeb) {
      if (defaultTargetPlatform == TargetPlatform.android) {
        expect(find.byType(AndroidView), findsOneWidget);
      } else if (defaultTargetPlatform == TargetPlatform.iOS) {
        expect(find.byType(UiKitView), findsOneWidget);
      }
    }
  });
}
