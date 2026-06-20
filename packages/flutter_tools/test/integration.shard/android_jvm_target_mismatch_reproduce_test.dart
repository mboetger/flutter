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
    tempDir = createResolvedTempDirectorySync('android_jvm_target_mismatch_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  test('build warns/fails when plugin has mismatched Java and Kotlin JVM targets', () async {
    // Create dummy plugin
    final ProcessResult createResult = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      '--template=plugin',
      '--platforms=android',
      'test_plugin',
    ], workingDirectory: tempDir.path);
    expect(createResult, const ProcessResultMatcher());

    final Directory pluginAppDir = tempDir.childDirectory('test_plugin');
    final File pluginGradleFile = pluginAppDir
        .childDirectory('android')
        .childFile('build.gradle.kts');
    expect(pluginGradleFile, exists);

    // Append a PlatformViewFactory implementation to the plugin's Kotlin code to trigger
    // potential compilation issues when JVM targets mismatch.
    // Use fully qualified names to avoid placing import statements at the end of the file.
    final File kotlinSourceFile = pluginAppDir
        .childDirectory('android')
        .childDirectory('src')
        .childDirectory('main')
        .childDirectory('kotlin')
        .childDirectory('com')
        .childDirectory('example')
        .childDirectory('test_plugin')
        .childFile('TestPlugin.kt');
    expect(kotlinSourceFile, exists);

    final String originalKotlinCode = kotlinSourceFile.readAsStringSync();
    final modifiedKotlinCode =
        '''
$originalKotlinCode

class MyPlatformViewFactory : io.flutter.plugin.platform.PlatformViewFactory(io.flutter.plugin.common.StandardMessageCodec.INSTANCE) {
    override fun create(context: android.content.Context?, viewId: Int, args: Any?): io.flutter.plugin.platform.PlatformView {
        return object : io.flutter.plugin.platform.PlatformView {
            override fun getView(): android.view.View? = null
            override fun dispose() {}
        }
    }
}
''';
    kotlinSourceFile.writeAsStringSync(modifiedKotlinCode);

    final String pluginBuildGradle = pluginGradleFile.readAsStringSync();

    // Modify the plugin's build.gradle.kts to introduce a mismatch:
    // Set Java compatibility to 1.8, but keep Kotlin jvmTarget at the template default (e.g. 17).
    final String mismatchedGradle = pluginBuildGradle
        .replaceAll(
          RegExp(r'sourceCompatibility\s*=\s*JavaVersion\.VERSION_\d+'),
          'sourceCompatibility = JavaVersion.VERSION_1_8',
        )
        .replaceAll(
          RegExp(r'targetCompatibility\s*=\s*JavaVersion\.VERSION_\d+'),
          'targetCompatibility = JavaVersion.VERSION_1_8',
        );

    pluginGradleFile.writeAsStringSync(mismatchedGradle);

    final Directory pluginExampleAppDir = pluginAppDir.childDirectory('example');

    // Run flutter build apk to build the plugin example project
    final ProcessResult buildResult = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'build',
      'apk',
      '--target-platform=android-arm',
    ], workingDirectory: pluginExampleAppDir.path);

    // We expect the build to succeed. Under the mismatch condition, the build will fail,
    // confirming the bug.
    expect(
      buildResult.exitCode,
      0,
      reason:
          'Build should succeed, but failed due to JVM target mismatch:\n'
          'STDOUT:\n${buildResult.stdout}\n'
          'STDERR:\n${buildResult.stderr}',
    );
  });
}
