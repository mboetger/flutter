// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';
import 'package:path/path.dart' as path;
import 'common.dart';

void main() {
  test('gradle tests must also run the AAR build mode', () {
    Directory root = Directory.current;
    while (root.path != root.parent.path &&
        !Directory(path.join(root.path, 'dev', 'devicelab', 'bin', 'tasks')).existsSync()) {
      root = root.parent;
    }
    if (root.path == root.parent.path) {
      fail(
        'Could not find the Flutter repository root. Please run this test from within the Flutter repository.',
      );
    }
    final tasksDir = Directory(path.join(root.path, 'dev', 'devicelab', 'bin', 'tasks'));
    expect(
      tasksDir.existsSync(),
      true,
      reason: 'bin/tasks directory must exist at ${tasksDir.path}',
    );

    final List<File> gradleTestFiles = tasksDir
        .listSync()
        .whereType<File>()
        .where(
          (File file) =>
              path.basename(file.path).startsWith('gradle_') && file.path.endsWith('_test.dart'),
        )
        .toList();

    expect(gradleTestFiles.isNotEmpty, true, reason: 'Should find gradle_* test files');

    final failingFiles = <String>[];

    for (final file in gradleTestFiles) {
      final String content = file.readAsStringSync();
      // Check if this test builds apk or appbundle.
      final bool buildsApkOrBundle =
          content.contains("'apk'") ||
          content.contains('"apk"') ||
          content.contains("'appbundle'") ||
          content.contains('"appbundle"');

      if (buildsApkOrBundle) {
        // If it builds apk/bundle, it must also build aar.
        final bool buildsAar =
            content.contains("'aar'") ||
            content.contains('"aar"') ||
            content.contains('testAarBuilding') ||
            content.contains('testPluginAarBuilding');
        if (!buildsAar) {
          failingFiles.add(path.basename(file.path));
        }
      }
    }

    if (failingFiles.isNotEmpty) {
      fail(
        'The following Gradle tests build an APK or App Bundle but do not run the AAR build mode:\n'
        '${failingFiles.map((String filename) => ' - $filename').join('\n')}\n'
        'To fix this, update these tests to also exercise the AAR build workflow.',
      );
    }
  });
}
