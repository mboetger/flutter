// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/commands/create.dart';
import 'package:flutter_tools/src/globals.dart' as globals;

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/test_flutter_command_runner.dart';

void main() {
  late Directory tempDir;
  late Directory projectDir;

  setUpAll(() async {
    Cache.disableLocking();
  });

  setUp(() {
    tempDir = globals.fs.systemTempDirectory.createTempSync('flutter_tools_app_template_test.');
    projectDir = tempDir.childDirectory('flutter_project');

    Directory repoRoot = globals.fs.directory(globals.fs.currentDirectory.path);
    while (repoRoot.path != repoRoot.parent.path &&
        !repoRoot.childDirectory('packages').childDirectory('flutter_tools').existsSync()) {
      repoRoot = repoRoot.parent;
    }
    Cache.flutterRoot = repoRoot.path;
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  testUsingContext(
    'app template does not contain configureFlutterEngine or GeneratedPluginRegistrant in MainActivity (Java)',
    () async {
      final command = CreateCommand();
      await createTestCommandRunner(command).run(<String>[
        'create',
        '--template=app',
        '--no-pub',
        '--platforms=android',
        '-a',
        'java',
        projectDir.path,
      ]);

      final File mainActivityFile = projectDir
          .childDirectory('android')
          .childDirectory('app')
          .childDirectory('src')
          .childDirectory('main')
          .childDirectory('java')
          .childDirectory('com')
          .childDirectory('example')
          .childDirectory('flutter_project')
          .childFile('MainActivity.java');

      expect(mainActivityFile.existsSync(), isTrue);
      final String content = mainActivityFile.readAsStringSync();

      expect(content, isNot(contains('configureFlutterEngine')));
      expect(content, isNot(contains('GeneratedPluginRegistrant')));
    },
  );

  testUsingContext(
    'app template does not contain configureFlutterEngine or GeneratedPluginRegistrant in MainActivity (Kotlin)',
    () async {
      final command = CreateCommand();
      await createTestCommandRunner(command).run(<String>[
        'create',
        '--template=app',
        '--no-pub',
        '--platforms=android',
        '-a',
        'kotlin',
        projectDir.path,
      ]);

      final File mainActivityFile = projectDir
          .childDirectory('android')
          .childDirectory('app')
          .childDirectory('src')
          .childDirectory('main')
          .childDirectory('kotlin')
          .childDirectory('com')
          .childDirectory('example')
          .childDirectory('flutter_project')
          .childFile('MainActivity.kt');

      expect(mainActivityFile.existsSync(), isTrue);
      final String content = mainActivityFile.readAsStringSync();

      expect(content, isNot(contains('configureFlutterEngine')));
      expect(content, isNot(contains('GeneratedPluginRegistrant')));
    },
  );
}
