// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

import 'widget_inspector_test_utils.dart';

void main() {
  StructuredErrorsReproduceTestWidgetInspectorService.runTests();
}

class StructuredErrorsReproduceTestWidgetInspectorService extends TestWidgetInspectorService {
  static void runTests() {
    final log = <String>[];
    late final DebugPrintCallback oldDebugPrint;
    late final FlutterExceptionHandler oldPresentError;
    late final WidgetInspectorService oldInspectorService;
    late final TestWidgetInspectorService service;

    setUpAll(() {
      oldPresentError = FlutterError.presentError;
      oldInspectorService = WidgetInspectorService.instance;
      oldDebugPrint = debugPrint;
      service = StructuredErrorsReproduceTestWidgetInspectorService();
      WidgetInspectorService.instance = service;

      TestWidgetsFlutterBinding.ensureInitialized();

      debugPrint = (String? message, {int? wrapWidth}) {
        if (message != null) {
          log.add(message);
        }
      };
    });

    tearDownAll(() {
      debugPrint = oldDebugPrint;
      FlutterError.presentError = oldPresentError;
      WidgetInspectorService.instance = oldInspectorService;
    });

    setUp(() {
      log.clear();
    });

    test(
      'FlutterError.presentError prints to console when structured errors are enabled BUT no listener is attached',
      () {
        service.mockExtensionStreamHasListener = false;

        // Verify structured errors are enabled.
        expect(service.isStructuredErrorsEnabled(), isTrue);

        // Verify that presentError has indeed been overridden by _reportStructuredError.
        expect(FlutterError.presentError, isNot(equals(oldPresentError)));

        final details = FlutterErrorDetails(
          exception: AssertionError('Test exception'),
          stack: StackTrace.current,
          library: 'Test library',
          context: ErrorDescription('Test context'),
        );

        // Call presentError.
        FlutterError.presentError(details);

        // We expect the error to be printed to the console (log should not be empty).
        expect(
          log,
          isNotEmpty,
          reason:
              'The error should be printed to the console/logcat when no VM service client is listening.',
        );
        expect(log.join('\n'), contains('Test exception'));
      },
      skip: kIsWeb,
    );

    test(
      'FlutterError.presentError does NOT print to console when structured errors are enabled AND a listener is attached',
      () {
        service.mockExtensionStreamHasListener = true;

        // Verify structured errors are enabled.
        expect(service.isStructuredErrorsEnabled(), isTrue);

        // Verify that presentError has indeed been overridden by _reportStructuredError.
        expect(FlutterError.presentError, isNot(equals(oldPresentError)));

        final details = FlutterErrorDetails(
          exception: AssertionError('Test exception'),
          stack: StackTrace.current,
          library: 'Test library',
          context: ErrorDescription('Test context'),
        );

        // Call presentError.
        FlutterError.presentError(details);

        // We expect the error to NOT be printed to the console because the listener handles it.
        expect(
          log,
          isEmpty,
          reason:
              'The error should not be printed to the console when a VM service client is listening.',
        );
      },
      skip: kIsWeb,
    );
  }
}
