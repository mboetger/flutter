// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    tempDir = createResolvedTempDirectorySync('flutter_ndk_version_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  test('templates do not pin ndkVersion to flutter.ndkVersion', () async {
    final FileSystem fs = tempDir.fileSystem;
    final Directory templatesDir = fs.directory(
      fs.path.join(Cache.flutterRoot!, 'packages', 'flutter_tools', 'templates'),
    );

    // Verify all relevant templates that previously pinned ndkVersion,
    // or should not pin it in the future.
    final templateFilesToCheck = <File>[
      templatesDir
          .childDirectory('app')
          .childDirectory('android-kotlin.tmpl')
          .childDirectory('app')
          .childFile('build.gradle.kts.tmpl'),
      templatesDir
          .childDirectory('app')
          .childDirectory('android-java.tmpl')
          .childDirectory('app')
          .childFile('build.gradle.kts.tmpl'),
      templatesDir
          .childDirectory('module')
          .childDirectory('android')
          .childDirectory('library_new_embedding')
          .childDirectory('Flutter.tmpl')
          .childFile('build.gradle.tmpl'),
      templatesDir
          .childDirectory('plugin_ffi')
          .childDirectory('android.tmpl')
          .childFile('build.gradle.tmpl'),
    ];

    for (final file in templateFilesToCheck) {
      expect(file, exists);
      final String content = file.readAsStringSync();
      expect(
        content,
        isNot(contains('ndkVersion = flutter.ndkVersion')),
        reason: 'Template file ${file.path} should not pin ndkVersion to flutter.ndkVersion.',
      );
    }
  });

  test('newly created project does not pin ndkVersion to flutter.ndkVersion and builds successfully', () async {
    // Create a new flutter project
    final ProcessResult createResult = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      '--platforms=android',
      'test_ndk_app',
    ], workingDirectory: tempDir.path);

    expect(createResult.exitCode, 0);

    final Directory appDir = tempDir.childDirectory('test_ndk_app');
    final File buildGradleFile = appDir
        .childDirectory('android')
        .childDirectory('app')
        .childFile('build.gradle.kts');
    expect(buildGradleFile, exists);

    final String buildGradleContent = buildGradleFile.readAsStringSync();

    // The reproduction test expects that the template does NOT pin ndkVersion.
    // This assertion will FAIL on the unmodified codebase because ndkVersion is pinned.
    expect(
      buildGradleContent,
      isNot(contains('ndkVersion = flutter.ndkVersion')),
      reason: 'The template should not pin ndkVersion to flutter.ndkVersion, allowing AGP to use its default.',
    );

    // Verify that the project still builds successfully (gradle configuration is successful)
    final ProcessResult buildResult = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'build',
      'apk',
      '--config-only',
    ], workingDirectory: appDir.path);

    expect(buildResult.exitCode, 0);
  });
}
