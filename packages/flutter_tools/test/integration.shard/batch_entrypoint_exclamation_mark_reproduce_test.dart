// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:flutter_tools/src/base/io.dart';
import '../src/common.dart';
import 'test_utils.dart';

void main() {
  testWithoutContext(
    'flutter.bat parses arguments with exclamation marks correctly when delayed expansion is enabled',
    () async {
      // Run flutter.bat via cmd.exe with delayed expansion (/V:ON) enabled.
      // We escape the exclamation marks using ^! so the outer cmd.exe doesn't expand them,
      // passing them literally to flutter.bat.
      final ProcessResult result = await processManager.run(<String>[
        'cmd.exe',
        '/V:ON',
        '/C',
        flutterBin,
        '^!foo^!bar',
      ]);

      expect(result.exitCode, 64);
      // If the bug is present, delayed expansion is not disabled inside flutter.bat,
      // so !foo! is expanded to empty string and the command received is 'bar'.
      // If fixed, it should be '!foo!bar'.
      expect(result.stdout, contains('Could not find a command named "!foo!bar".'));
    },
    testOn: 'windows',
  );

  // A platform-agnostic check that verifies all Windows batch entrypoints disable delayed expansion.
  testWithoutContext('all Windows batch entrypoints disable delayed expansion', () {
    final batchFiles = <String>[
      fileSystem.path.join(getFlutterRoot(), 'bin', 'flutter.bat'),
      fileSystem.path.join(getFlutterRoot(), 'bin', 'internal', 'shared.bat'),
      fileSystem.path.join(getFlutterRoot(), 'bin', 'dart.bat'),
      fileSystem.path.join(getFlutterRoot(), 'bin', 'flutter-dev.bat'),
    ];

    for (final path in batchFiles) {
      final File file = fileSystem.file(path);
      expect(
        file.readAsStringSync(),
        contains('SETLOCAL DISABLEDELAYEDEXPANSION'),
        reason:
            'File $path must disable delayed expansion explicitly to support exclamation marks in paths.',
      );
    }
  });
}
