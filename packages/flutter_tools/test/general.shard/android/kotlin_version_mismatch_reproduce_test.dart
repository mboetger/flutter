// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter_tools/src/android/gradle_errors.dart';

import '../../src/common.dart';

void main() {
  group('Kotlin version mismatch reproduction', () {
    testWithoutContext('pattern matches incompatibleKotlinVersionHandler', () {
      expect(
        incompatibleKotlinVersionHandler.test(
          "The Android Gradle plugin supports only Kotlin Gradle plugin version 1.2.51 and higher. Project 'plugin_with_old_kotlin' is using version 1.2.30.",
        ),
        isTrue,
      );
    });
  });
}
