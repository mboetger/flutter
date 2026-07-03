// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';
import 'package:flutter_tools/src/cache.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() {
    Cache.flutterRoot = getFlutterRoot();
    tempDir = createResolvedTempDirectorySync('reproduce_62176_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  test('flutter build aar with a plugin', () async {
    // create flutter module project
    ProcessResult result = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      '--template=module',
      'flutter_project',
    ], workingDirectory: tempDir.path);

    final String projectPath = fileSystem.path.join(tempDir.path, 'flutter_project');

    final File modulePubspec = fileSystem.file(fileSystem.path.join(projectPath, 'pubspec.yaml'));
    String pubspecContent = modulePubspec.readAsStringSync();
    pubspecContent = pubspecContent.replaceFirst('dependencies:', '''
dependencies:
  path_provider: ^2.1.0
'''); // Use a simple plugin
    modulePubspec.writeAsStringSync(pubspecContent);

    // Run pub get first to ensure .android is generated
    result = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'pub',
      'get',
    ], workingDirectory: projectPath);
    expect(result.exitCode, 0);

    // Create a non-Android subproject
    final Directory customLibDir = fileSystem.directory(
      fileSystem.path.join(projectPath, '.android', 'my_custom_lib'),
    )..createSync(recursive: true);

    fileSystem.file(fileSystem.path.join(customLibDir.path, 'build.gradle')).writeAsStringSync('''
plugins {
    id 'java-library'
}
''');

    // Add it to settings.gradle
    final File settingsGradle = fileSystem.file(
      fileSystem.path.join(projectPath, '.android', 'settings.gradle'),
    );
    String settingsContent = settingsGradle.readAsStringSync();
    settingsContent += '''
include ':my_custom_lib'
project(':my_custom_lib').projectDir = new File(settingsDir, 'my_custom_lib')
''';
    settingsGradle.writeAsStringSync(settingsContent);

    // Run flutter build aar
    result = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'build',
      'aar',
      '--verbose',
      // No --no-profile or --no-debug to build all variants (debug, profile, release)
      '--target-platform=android-arm',
    ], workingDirectory: projectPath);

    expect(result.exitCode, 0);
  });
}
