// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' show getGradlewFileName;
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('custom_build_type_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  Future<void> setupProjectWithStagingBuildType() async {
    ProcessResult result = await processManager.run(<String>[
      flutterBin,
      'create',
      tempDir.path,
      '--project-name=testapp',
      '--platforms=android',
    ], workingDirectory: tempDir.path);
    expect(result.exitCode, 0, reason: 'flutter create failed:\n${result.stderr}');

    result = await processManager.run(<String>[
      flutterBin,
      'build',
      'apk',
      '--config-only',
    ], workingDirectory: tempDir.path);
    expect(result.exitCode, 0, reason: 'flutter build apk --config-only failed:\n${result.stderr}');

    final Directory androidApp = tempDir.childDirectory('android').childDirectory('app');
    final File buildGradleKts = androidApp.childFile('build.gradle.kts');
    final File buildGradle = androidApp.childFile('build.gradle');
    if (buildGradleKts.existsSync()) {
      final String content = buildGradleKts.readAsStringSync();
      final String newContent = content.replaceFirst(
        'buildTypes {',
        'buildTypes {\n        create("staging") {\n            initWith(getByName("release"))\n        }',
      );
      buildGradleKts.writeAsStringSync(newContent);
    } else if (buildGradle.existsSync()) {
      final String content = buildGradle.readAsStringSync();
      final String newContent = content.replaceFirst(
        'buildTypes {',
        'buildTypes {\n        staging {\n            initWith release\n        }',
      );
      buildGradle.writeAsStringSync(newContent);
    } else {
      fail('Neither build.gradle nor build.gradle.kts found in app directory.');
    }
  }

  testWithoutContext(
    'custom build type respects -PFLUTTER_BUILD_MODE=profile in gradle dependency resolution',
    () async {
      await setupProjectWithStagingBuildType();

      final Directory androidDir = tempDir.childDirectory('android');
      final ProcessResult result = await processManager.run(<String>[
        '.${platform.pathSeparator}${getGradlewFileName(platform)}',
        ...getLocalEngineArguments(),
        '-q', // quiet output.
        'app:dependencies',
        '--configuration',
        'stagingRuntimeClasspath',
        '-PFLUTTER_BUILD_MODE=profile',
      ], workingDirectory: androidDir.path);

      expect(result.exitCode, 0, reason: 'gradlew app:dependencies failed:\n${result.stderr}');
      expect(
        result.stdout.toString(),
        contains('io.flutter:flutter_embedding_profile'),
        reason:
            'Custom build type staging should respect -PFLUTTER_BUILD_MODE=profile.\n'
            'Actual stdout:\n${result.stdout}',
      );
    },
  );

  testWithoutContext(
    'custom build type respects FLUTTER_BUILD_MODE environment variable in gradle dependency resolution',
    () async {
      await setupProjectWithStagingBuildType();

      final Directory androidDir = tempDir.childDirectory('android');
      final ProcessResult result = await processManager.run(
        <String>[
          '.${platform.pathSeparator}${getGradlewFileName(platform)}',
          ...getLocalEngineArguments(),
          '-q', // quiet output.
          'app:dependencies',
          '--configuration',
          'stagingRuntimeClasspath',
        ],
        workingDirectory: androidDir.path,
        environment: <String, String>{'FLUTTER_BUILD_MODE': 'profile'},
      );

      expect(result.exitCode, 0, reason: 'gradlew app:dependencies failed:\n${result.stderr}');
      expect(
        result.stdout.toString(),
        contains('io.flutter:flutter_embedding_profile'),
        reason:
            'Custom build type staging should respect FLUTTER_BUILD_MODE environment variable.\n'
            'Actual stdout:\n${result.stdout}',
      );
    },
  );
}
