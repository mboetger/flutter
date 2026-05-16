// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';

import 'package:path/path.dart' as path;

import '../../src/common.dart';

void main() {
  testWithoutContext(
    'Flutter SDK not found error message instructs user to run flutter build or flutter run',
    () {
      final String flutterRoot = getFlutterRoot();
      final flutterPluginFile = File(
        path.join(
          flutterRoot,
          'packages',
          'flutter_tools',
          'gradle',
          'src',
          'main',
          'kotlin',
          'FlutterPlugin.kt',
        ),
      );

      expect(flutterPluginFile.existsSync(), isTrue);
      final String content = flutterPluginFile.readAsStringSync();

      // Locate the specific GradleException block for missing Flutter SDK.
      final int startIndex = content.indexOf('Flutter SDK not found');
      expect(
        startIndex,
        greaterThanOrEqualTo(0),
        reason: 'Could not find "Flutter SDK not found" error message in FlutterPlugin.kt',
      );

      // Find the end of the exception message / statement.
      final int endIndex = content.indexOf(')', startIndex);
      expect(endIndex, greaterThan(startIndex));

      final String errorMessageBlock = content.substring(startIndex, endIndex);

      // Verify that the error message itself directs the user to run `flutter build` or `flutter run`.
      expect(
        errorMessageBlock,
        matches(RegExp(r'flutter build|flutter run')),
        reason:
            'The SDK not found error message should instruct the user to run "flutter build" or "flutter run".',
      );
    },
  );
}
