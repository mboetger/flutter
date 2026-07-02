// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';

import 'package:flutter_tools/src/base/file_system.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('aar_plugin_dependency_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext('flutter build aar with plugin dependency in gradle', () async {
    // 1. Create a module.
    final Directory moduleDir = tempDir.childDirectory('my_module');
    final ProcessResult createModuleResult = processManager.runSync(<String>[
      flutterBin,
      'create',
      '--template=module',
      'my_module',
    ], workingDirectory: tempDir.path);
    expect(createModuleResult.exitCode, 0, reason: createModuleResult.stderr.toString());

    // 2. Create plugin_b (the dependency).
    final ProcessResult createPluginBResult = processManager.runSync(<String>[
      flutterBin,
      'create',
      '--template=plugin',
      '--platforms=android',
      'plugin_b',
    ], workingDirectory: tempDir.path);
    expect(createPluginBResult.exitCode, 0, reason: createPluginBResult.stderr.toString());

    // 3. Create plugin_a (depends on plugin_b).
    final Directory pluginADir = tempDir.childDirectory('plugin_a');
    final ProcessResult createPluginAResult = processManager.runSync(<String>[
      flutterBin,
      'create',
      '--template=plugin',
      '--platforms=android',
      'plugin_a',
    ], workingDirectory: tempDir.path);
    expect(createPluginAResult.exitCode, 0, reason: createPluginAResult.stderr.toString());

    // 4. Modify my_module's pubspec.yaml to depend on plugin_a and plugin_b.
    final File modulePubspec = moduleDir.childFile('pubspec.yaml');
    String pubspecContent = modulePubspec.readAsStringSync();
    pubspecContent = pubspecContent.replaceFirst('dependencies:', '''
dependencies:
  plugin_a:
    path: ../plugin_a
  plugin_b:
    path: ../plugin_b''');
    modulePubspec.writeAsStringSync(pubspecContent);

    // 5. Modify plugin_a's android/build.gradle (.kts) to depend on plugin_b in gradle.
    final File pluginABuildGradle = pluginADir.childDirectory('android').childFile('build.gradle');
    final File pluginABuildGradleKts = pluginADir
        .childDirectory('android')
        .childFile('build.gradle.kts');
    if (pluginABuildGradleKts.existsSync()) {
      pluginABuildGradleKts.writeAsStringSync(
        '${pluginABuildGradleKts.readAsStringSync()}\n\ndependencies {\n    implementation(project(":plugin_b"))\n}\n',
      );
    } else if (pluginABuildGradle.existsSync()) {
      pluginABuildGradle.writeAsStringSync(
        "${pluginABuildGradle.readAsStringSync()}\n\ndependencies {\n    implementation project(':plugin_b')\n}\n",
      );
    } else {
      fail('Neither build.gradle nor build.gradle.kts found in plugin_a');
    }

    // 6. Run flutter pub get in the module.
    final ProcessResult pubGetResult = processManager.runSync(<String>[
      flutterBin,
      'pub',
      'get',
    ], workingDirectory: moduleDir.path);
    expect(pubGetResult.exitCode, 0, reason: pubGetResult.stderr.toString());

    // 7. Run flutter build aar.
    final ProcessResult buildAarResult = processManager.runSync(<String>[
      flutterBin,
      'build',
      'aar',
    ], workingDirectory: moduleDir.path);

    // We expect the build to succeed because the bug is fixed.
    expect(buildAarResult.exitCode, 0, reason: buildAarResult.stderr.toString());
  });
}
