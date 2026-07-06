// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io' as io;

import 'package:flutter_tools/src/cache.dart';

import '../../src/common.dart';

void main() {
  testWithoutContext(
    'flutter_proguard_rules.pro keeps ContentProvider and FileProvider to prevent ClassNotFoundException on startup (flutter/flutter#74841)',
    () {
      Cache.flutterRoot = getFlutterRoot();
      final proguardRules = io.File(
        '${getFlutterRoot()}/packages/flutter_tools/gradle/flutter_proguard_rules.pro',
      );

      expect(proguardRules.existsSync(), isTrue);

      final String content = proguardRules.readAsStringSync();

      // Verify that ProGuard/R8 rules keep ContentProvider and FileProvider implementations
      // such as io.flutter.plugins.imagepicker.ImagePickerFileProvider and ShareFileProvider
      // that are referenced only in AndroidManifest.xml and would otherwise be stripped.
      expect(
        content,
        contains('-keep public class * extends android.content.ContentProvider'),
        reason: 'Must keep ContentProvider implementations referenced in AndroidManifest.xml',
      );
      expect(
        content,
        contains('-keep public class * extends androidx.core.content.FileProvider'),
        reason: 'Must keep FileProvider implementations referenced in AndroidManifest.xml',
      );
    },
  );
}
