// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter_tools/src/android/gradle_errors.dart';

import '../../src/common.dart';

void main() {
  group('gradleErrors reproduction', () {
    testWithoutContext('app_plugin_loader.gradle missing error is handled', () {
      const errorMessage = r'''
FAILURE: Build failed with an exception.

* Where:
Settings file '/Users/ralphkleinguevarra/Projects/patient-portal/android/settings.gradle' line: 15

* What went wrong:
A problem occurred evaluating settings 'android'.
> Could not read script '/Users/ralphkleinguevarra/development/flutter/packages/flutter_tools/gradle/app_plugin_loader.gradle' as it does not exist.

* Try:
Run with --stacktrace option to get the stack trace. Run with --info or --debug option to get more log output. Run with --scan to get full insights.

* Get more help at https://help.gradle.org

BUILD FAILED in 1s
''';

      final bool isHandled = gradleErrors.any((GradleHandledError error) {
        return errorMessage.split('\n').any((String line) => error.test(line));
      });

      expect(
        isHandled,
        isTrue,
        reason:
            'The error "Could not read script .../app_plugin_loader.gradle as it does not exist" should be recognized and handled by the Gradle error handlers.',
      );
    });
  });
}
