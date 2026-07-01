// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@Timeout(Duration(minutes: 10))
library;

import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';
import 'package:flutter_tools/src/cache.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() {
    Cache.flutterRoot = getFlutterRoot();
    tempDir = createResolvedTempDirectorySync('android_plugin_subproject_reproduce_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  test('build succeeds when plugin has a subproject', () async {
    // 1. Create a new Flutter Plugin project.
    final ProcessResult createResult = await processManager.run(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      '--template=plugin',
      '--platforms=android',
      'test_plugin',
    ], workingDirectory: tempDir.path);
    expect(createResult, const ProcessResultMatcher());

    final Directory pluginDir = tempDir.childDirectory('test_plugin');
    final Directory androidDir = pluginDir.childDirectory('android');

    // 2. Add a new Gradle project under the `android/` folder called `foo`.
    final Directory fooDir = androidDir.childDirectory('foo');
    fooDir.createSync(recursive: true);

    // Create foo/build.gradle.kts
    final File fooBuildGradle = fooDir.childFile('build.gradle.kts');
    fooBuildGradle.writeAsStringSync('''
plugins {
    `java-library`
}
''');

    // 3. Add `include(":foo")` to the plugin's `android/settings.gradle.kts`.
    final File settingsGradle = androidDir.childFile('settings.gradle.kts');
    expect(settingsGradle, exists);
    settingsGradle.writeAsStringSync('\ninclude(":foo")\n', mode: FileMode.append);

    // 4. Make the `android/` project depend on the `foo` sub-project.
    final File pluginBuildGradle = androidDir.childFile('build.gradle.kts');
    expect(pluginBuildGradle, exists);
    pluginBuildGradle.writeAsStringSync('''
dependencies {
    implementation(project(":foo"))
}
''', mode: FileMode.append);

    // 5. Build the example app.
    final Directory exampleAppDir = pluginDir.childDirectory('example');
    final ProcessResult result = await processManager.run(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'build',
      'apk',
      '--debug',
      '--target-platform=android-arm',
    ], workingDirectory: exampleAppDir.path);

    // The build should succeed, but it will fail because of the bug.
    if (result.exitCode != 0) {
      fail(
        'Build failed with exit code ${result.exitCode}.\n'
        'Output:\n${result.stdout}\n${result.stderr}',
      );
    }
  });
}
