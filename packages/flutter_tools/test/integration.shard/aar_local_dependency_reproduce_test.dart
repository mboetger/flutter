// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';

import 'package:archive/archive.dart';
import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('flutter_aar_reproduce.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  test('local aar in plugin resolves in example app', () async {
    const pluginName = 'test_plugin';

    // 1. Create a dummy Flutter plugin
    ProcessResult result = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      '--template=plugin',
      '--platforms=android',
      pluginName,
    ], workingDirectory: tempDir.path);

    if (result.exitCode != 0) {
      fail('flutter create failed: ${result.exitCode}\n${result.stderr}\n${result.stdout}');
    }

    final Directory pluginDir = tempDir.childDirectory(pluginName);
    final Directory androidDir = pluginDir.childDirectory('android');
    final Directory libsDir = androidDir.childDirectory('libs');
    libsDir.createSync(recursive: true);

    // 2. Create a dummy .aar file (must be a valid zip with AndroidManifest.xml)
    const manifestContent = '''

<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.dummy">
</manifest>
''';
    final archive = Archive()
      ..addFile(
        ArchiveFile('AndroidManifest.xml', manifestContent.length, utf8.encode(manifestContent)),
      );
    final List<int>? zipBytes = ZipEncoder().encode(archive);
    final File dummyAar = libsDir.childFile('dummy.aar');
    dummyAar.writeAsBytesSync(zipBytes!);

    // 3. Configure test_plugin/android/build.gradle.kts to use flatDir and depend on the dummy aar.
    final File buildGradle = androidDir.childFile('build.gradle.kts');
    expect(buildGradle, exists);

    String buildGradleContent = buildGradle.readAsStringSync();

    // We use rootProject.allprojects to apply flatDir, which is a common (but flawed) way
    // because 'libs' will be resolved relative to the root project (the app).
    // With our fix, this is now automatically handled and resolved correctly.
    const repositoryBlock = '''

rootProject.allprojects {
    repositories {
        flatDir {
            dirs("libs")
        }
    }
}
''';

    // Append the repository block and a new dependencies block to the end of the file.
    buildGradleContent =
        '''
$buildGradleContent
$repositoryBlock
dependencies {
    add("implementation", mapOf("name" to "dummy", "ext" to "aar"))
}
''';

    buildGradle.writeAsStringSync(buildGradleContent);

    // 4. Try to build the example app of the plugin
    final Directory exampleAppDir = pluginDir.childDirectory('example');

    result = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'build',
      'apk',
      '--debug',
      '--target-platform=android-arm',
    ], workingDirectory: exampleAppDir.path);

    // 5. Verify that the build succeeds
    if (result.exitCode != 0) {
      fail('flutter build apk failed: ${result.exitCode}\n${result.stderr}\n${result.stdout}');
    }
    expect(result.exitCode, 0);
  });
}
