// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:args/command_runner.dart';
import 'package:file/file.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/commands/create.dart';
import 'package:flutter_tools/src/dart/pub.dart';
import 'package:flutter_tools/src/globals.dart' as globals;

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fakes.dart';
import '../../src/test_flutter_command_runner.dart';

void main() {
  late Directory tempDir;

  setUpAll(() async {
    Cache.disableLocking();
  });

  setUp(() {
    tempDir = globals.fs.systemTempDirectory.createTempSync('flutter_tools_reproduce_29700_test.');
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  testUsingContext(
    'generated app build.gradle.kts does not contain allprojects repository configuration',
    () async {
      final Directory projectDir = tempDir.childDirectory('flutter_app');
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);
      await runner.run(<String>[
        'create',
        '--no-pub',
        '--template=app',
        '--platforms=android',
        projectDir.path,
      ]);

      final File buildGradleKts = projectDir
          .childDirectory('android')
          .childFile('build.gradle.kts');
      expect(buildGradleKts.existsSync(), isTrue);

      final String content = buildGradleKts.readAsStringSync();
      expect(content, isNot(contains('allprojects')));

      final File appBuildGradleKts = projectDir
          .childDirectory('android')
          .childDirectory('app')
          .childFile('build.gradle.kts');
      expect(appBuildGradleKts.existsSync(), isTrue);
      final String appContent = appBuildGradleKts.readAsStringSync();
      expect(appContent, contains('repositories {'));
      expect(appContent, contains('google()'));
      expect(appContent, contains('mavenCentral()'));

      final File settingsGradleKts = projectDir
          .childDirectory('android')
          .childFile('settings.gradle.kts');
      expect(settingsGradleKts.existsSync(), isTrue);
      final String settingsContent = settingsGradleKts.readAsStringSync();
      expect(settingsContent, contains('dependencyResolutionManagement {'));
      expect(settingsContent, contains('repositoriesMode.set(RepositoriesMode.PREFER_PROJECT)'));
    },
    overrides: <Type, Generator>{},
  );

  testUsingContext(
    'generated plugin build.gradle.kts does not contain allprojects repository configuration',
    () async {
      final Directory projectDir = tempDir.childDirectory('flutter_plugin');
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);
      await runner.run(<String>[
        'create',
        '--no-pub',
        '--template=plugin',
        '--platforms=android',
        projectDir.path,
      ]);

      final File buildGradleKts = projectDir
          .childDirectory('android')
          .childFile('build.gradle.kts');
      expect(buildGradleKts.existsSync(), isTrue);

      final String content = buildGradleKts.readAsStringSync();
      expect(content, isNot(contains('allprojects')));
      expect(content, contains('repositories {'));
      expect(content, contains('google()'));
      expect(content, contains('mavenCentral()'));
    },
    overrides: <Type, Generator>{},
  );

  testUsingContext(
    'generated plugin_ffi build.gradle does not contain rootProject.allprojects repository configuration',
    () async {
      final Directory projectDir = tempDir.childDirectory('flutter_plugin_ffi');
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);
      await runner.run(<String>[
        'create',
        '--no-pub',
        '--template=plugin_ffi',
        '--platforms=android',
        projectDir.path,
      ]);

      final File buildGradle = projectDir.childDirectory('android').childFile('build.gradle');
      expect(buildGradle.existsSync(), isTrue);

      final String content = buildGradle.readAsStringSync();
      expect(content, isNot(contains('rootProject.allprojects')));
      expect(content, contains('repositories {'));
      expect(content, contains('google()'));
      expect(content, contains('mavenCentral()'));
    },
    overrides: <Type, Generator>{},
  );

  testUsingContext(
    'generated module Android project does not contain allprojects and has correct repositories/DRM',
    () async {
      final Directory projectDir = tempDir.childDirectory('flutter_module');
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);
      await runner.run(<String>['create', '--template=module', projectDir.path]);

      // Verify root .android/build.gradle
      final File rootBuildGradle = projectDir.childDirectory('.android').childFile('build.gradle');
      expect(rootBuildGradle.existsSync(), isTrue);
      final String rootContent = rootBuildGradle.readAsStringSync();
      expect(rootContent, isNot(contains('allprojects')));
      expect(rootContent, contains('repositories {'));
      expect(rootContent, contains('google()'));
      expect(rootContent, contains('mavenCentral()'));

      // Verify root .android/settings.gradle
      final File rootSettingsGradle = projectDir
          .childDirectory('.android')
          .childFile('settings.gradle');
      expect(rootSettingsGradle.existsSync(), isTrue);
      final String rootSettingsContent = rootSettingsGradle.readAsStringSync();
      expect(rootSettingsContent, contains('dependencyResolutionManagement {'));
      expect(
        rootSettingsContent,
        contains('repositoriesMode.set(RepositoriesMode.PREFER_PROJECT)'),
      );

      // Verify host app .android/app/build.gradle
      final File appBuildGradle = projectDir
          .childDirectory('.android')
          .childDirectory('app')
          .childFile('build.gradle');
      expect(appBuildGradle.existsSync(), isTrue);
      final String appContent = appBuildGradle.readAsStringSync();
      expect(appContent, contains('repositories {'));
      expect(appContent, contains('google()'));
      expect(appContent, contains('mavenCentral()'));

      // Verify library .android/Flutter/build.gradle
      final File flutterBuildGradle = projectDir
          .childDirectory('.android')
          .childDirectory('Flutter')
          .childFile('build.gradle');
      expect(flutterBuildGradle.existsSync(), isTrue);
      final String flutterContent = flutterBuildGradle.readAsStringSync();
      expect(flutterContent, contains('repositories {'));
      expect(flutterContent, contains('google()'));
      expect(flutterContent, contains('mavenCentral()'));
    },
    overrides: <Type, Generator>{
      Pub: () => Pub.test(
        fileSystem: globals.fs,
        logger: globals.logger,
        processManager: globals.processManager,
        botDetector: globals.botDetector,
        platform: globals.platform,
        stdio: FakeStdio(),
      ),
    },
  );
}
