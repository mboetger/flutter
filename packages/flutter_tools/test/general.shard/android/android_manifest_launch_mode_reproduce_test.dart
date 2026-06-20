// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/local.dart';

import '../../src/common.dart';

void main() {
  const FileSystem fs = LocalFileSystem();

  group('AndroidManifest launchMode reproduction test', () {
    testWithoutContext('app template AndroidManifest uses singleTask', () {
      final File manifestTemplate = fs.file(
        fs.path.join(
          getFlutterRoot(),
          'packages',
          'flutter_tools',
          'templates',
          'app',
          'android.tmpl',
          'app',
          'src',
          'main',
          'AndroidManifest.xml.tmpl',
        ),
      );

      expect(manifestTemplate.existsSync(), isTrue);
      final String content = manifestTemplate.readAsStringSync();

      // Assert that launchMode is singleTask.
      expect(
        content,
        contains('android:launchMode="singleTask"'),
        reason:
            'Android app template should use launchMode="singleTask" to prevent creating multiple activity instances on deep links.',
      );
    });

    testWithoutContext('module template AndroidManifest uses singleTask', () {
      final File manifestTemplate = fs.file(
        fs.path.join(
          getFlutterRoot(),
          'packages',
          'flutter_tools',
          'templates',
          'module',
          'android',
          'host_app_common',
          'app.tmpl',
          'src',
          'main',
          'AndroidManifest.xml.tmpl',
        ),
      );

      expect(manifestTemplate.existsSync(), isTrue);
      final String content = manifestTemplate.readAsStringSync();

      // Assert that launchMode is singleTask.
      expect(
        content,
        contains('android:launchMode="singleTask"'),
        reason:
            'Android module template should use launchMode="singleTask" to prevent creating multiple activity instances on deep links.',
      );
    });
  });
}
