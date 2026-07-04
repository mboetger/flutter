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
    tempDir = createResolvedTempDirectorySync('flutter_plugin_old_sdk_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  test(
    'plugin with older compileSdk and buildToolsVersion is compiled against app compileSdkVersion',
    () async {
      // Create dummy plugin
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'create',
        '--template=plugin',
        '--platforms=android',
        'test_plugin',
      ], workingDirectory: tempDir.path);

      final Directory pluginAppDir = tempDir.childDirectory('test_plugin');
      final File pluginGradleFile = pluginAppDir
          .childDirectory('android')
          .childFile('build.gradle.kts');
      expect(pluginGradleFile, exists);

      final String pluginBuildGradle = pluginGradleFile.readAsStringSync();

      // Change plugin compileSdk version to 30 and specify older buildToolsVersion 30.0.3
      // (when app is targeting compileSdkVersion 36).
      final androidCompileSdkVersionRegExp = RegExp(
        r'compileSdk = ([0-9]+|flutter.compileSdkVersion)',
      );
      final String newPluginGradleFile = pluginBuildGradle.replaceAll(
        androidCompileSdkVersionRegExp,
        'compileSdk = 30\n    buildToolsVersion = "30.0.3"',
      );
      pluginGradleFile.writeAsStringSync(newPluginGradleFile);

      final Directory pluginExampleAppDir = pluginAppDir.childDirectory('example');
      final File projectGradleFile = pluginExampleAppDir
          .childDirectory('android')
          .childDirectory('app')
          .childFile('build.gradle.kts');
      expect(projectGradleFile, exists);

      // Run flutter build apk to build plugin example project
      final ProcessResult result = processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'build',
        'apk',
        '--target-platform=android-arm',
      ], workingDirectory: pluginExampleAppDir.path);

      // When flutter/flutter#63533 is fixed, the Android build system should compile against the app's
      // compileSdkVersion rather than attempting to install/compile against the older Android SDK Platform
      // and Build Tools requested by the plugin.
      expect(
        result.exitCode,
        0,
        reason:
            'Expected APK build to succeed by compiling plugin against app compileSdkVersion rather than attempting to install/compile against older plugin SDK version (30).\nStderr: ${result.stderr}',
      );
      expect(
        result.stderr,
        isNot(contains(':test_plugin is currently compiled against android-30')),
      );
    },
  );
}
