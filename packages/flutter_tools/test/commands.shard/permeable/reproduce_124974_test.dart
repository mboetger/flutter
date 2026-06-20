// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:args/command_runner.dart';
import 'package:file/file.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/commands/create.dart';
import 'package:flutter_tools/src/globals.dart' as globals;
import 'package:flutter_tools/src/project.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/test_flutter_command_runner.dart';
import 'utils/project_testing_utils.dart';

void main() {
  late Directory tempDir;
  late Directory projectDir;

  setUpAll(() async {
    Cache.disableLocking();
    await ensureFlutterToolsSnapshot();
  });

  setUp(() {
    tempDir = globals.fs.systemTempDirectory.createTempSync('flutter_tools_reproduce_124974_test.');
    projectDir = tempDir.childDirectory('flutter_project');
    Cache.flutterRoot = '../..';
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  tearDownAll(() async {
    await restoreFlutterToolsSnapshot();
  });

  void assertUsesToolchain(File gradleFile) {
    expect(gradleFile.existsSync(), isTrue);
    final String content = gradleFile.readAsStringSync();

    // Expect java toolchain block
    expect(content, contains('java {'));
    expect(content, contains('toolchain {'));
    expect(content, contains('languageVersion'));
    expect(content, contains('JavaLanguageVersion.of('));

    // Ensure old compatibility configurations are completely gone
    expect(content, isNot(contains('sourceCompatibility')));
    expect(content, isNot(contains('targetCompatibility')));
  }

  testUsingContext(
    'Android Kotlin app template uses Gradle toolchain for Java compatibility',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>[
        'create',
        '--no-pub',
        '--org',
        'com.reproduce.issue124974',
        '-a',
        'kotlin',
        '--platforms=android',
        projectDir.path,
      ]);

      final File buildGradleFile = projectDir
          .childDirectory('android')
          .childDirectory('app')
          .childFile('build.gradle.kts');
      assertUsesToolchain(buildGradleFile);
    },
  );

  testUsingContext(
    'Android Java app template uses Gradle toolchain for Java compatibility',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>[
        'create',
        '--no-pub',
        '--org',
        'com.reproduce.issue124974',
        '-a',
        'java',
        '--platforms=android',
        projectDir.path,
      ]);

      final File buildGradleFile = projectDir
          .childDirectory('android')
          .childDirectory('app')
          .childFile('build.gradle.kts');
      assertUsesToolchain(buildGradleFile);
    },
  );

  testUsingContext(
    'Android Kotlin plugin template uses Gradle toolchain for Java compatibility',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>[
        'create',
        '--no-pub',
        '--template=plugin',
        '--org',
        'com.reproduce.issue124974',
        '-a',
        'kotlin',
        '--platforms=android',
        projectDir.path,
      ]);

      final File buildGradleFile = projectDir
          .childDirectory('android')
          .childFile('build.gradle.kts');
      assertUsesToolchain(buildGradleFile);
    },
  );

  testUsingContext(
    'Android Java plugin template uses Gradle toolchain for Java compatibility',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>[
        'create',
        '--no-pub',
        '--template=plugin',
        '--org',
        'com.reproduce.issue124974',
        '-a',
        'java',
        '--platforms=android',
        projectDir.path,
      ]);

      final File buildGradleFile = projectDir
          .childDirectory('android')
          .childFile('build.gradle.kts');
      assertUsesToolchain(buildGradleFile);
    },
  );

  testUsingContext(
    'Android FFI plugin template uses Gradle toolchain for Java compatibility',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>[
        'create',
        '--no-pub',
        '--template=plugin_ffi',
        '--org',
        'com.reproduce.issue124974',
        '--platforms=android',
        projectDir.path,
      ]);

      final File buildGradleFile = projectDir.childDirectory('android').childFile('build.gradle');
      assertUsesToolchain(buildGradleFile);
    },
  );

  testUsingContext(
    'Android module template uses Gradle toolchain for Java compatibility',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>[
        'create',
        '--no-pub',
        '--template=module',
        '--org',
        'com.reproduce.issue124974',
        projectDir.path,
      ]);

      final FlutterProject project = FlutterProject.fromDirectory(projectDir);
      await project.android.ensureReadyForPlatformSpecificTooling();

      final File appBuildGradle = projectDir
          .childDirectory('.android')
          .childDirectory('app')
          .childFile('build.gradle');
      final File flutterBuildGradle = projectDir
          .childDirectory('.android')
          .childDirectory('Flutter')
          .childFile('build.gradle');

      assertUsesToolchain(appBuildGradle);
      assertUsesToolchain(flutterBuildGradle);
    },
  );
}
