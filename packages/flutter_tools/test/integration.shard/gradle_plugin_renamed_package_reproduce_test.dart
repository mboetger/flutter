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
    tempDir = createResolvedTempDirectorySync('gradle_plugin_renamed_package_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  test('Flutter Gradle plugin does not treat plugin library projects as host modules', () async {
    // 1. Create a dummy app project
    final ProcessResult createAppResult = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      '--template=app',
      '--platforms=android',
      'my_app',
    ], workingDirectory: tempDir.path);
    expect(
      createAppResult.exitCode,
      0,
      reason: 'flutter create app failed: ${createAppResult.stderr}\n${createAppResult.stdout}',
    );

    // 2. Create a dummy plugin project named 'aaa_plugin'
    final ProcessResult createPluginResult = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      '--template=plugin',
      '--platforms=android',
      'aaa_plugin',
    ], workingDirectory: tempDir.path);
    expect(
      createPluginResult.exitCode,
      0,
      reason:
          'flutter create plugin failed: ${createPluginResult.stderr}\n${createPluginResult.stdout}',
    );

    final Directory appDir = tempDir.childDirectory('my_app');
    final Directory pluginDir = tempDir.childDirectory('aaa_plugin');

    // 3. Modify the plugin's android/build.gradle.kts to apply the flutter gradle plugin
    final File pluginGradleFile = pluginDir.childDirectory('android').childFile('build.gradle.kts');
    expect(pluginGradleFile, exists);

    String pluginGradleContent = pluginGradleFile.readAsStringSync();
    final pluginsBlockRegExp = RegExp(r'plugins\s*\{([^}]+)\}');
    pluginGradleContent = pluginGradleContent.replaceFirst(
      pluginsBlockRegExp,
      'plugins {\n    id("com.android.library")\n    id("dev.flutter.flutter-gradle-plugin")\n}',
    );
    pluginGradleFile.writeAsStringSync(pluginGradleContent);

    // 4. Manually include the plugin in the app's settings.gradle.kts
    final File appSettingsFile = appDir.childDirectory('android').childFile('settings.gradle.kts');
    expect(appSettingsFile, exists);

    String appSettingsContent = appSettingsFile.readAsStringSync();
    appSettingsContent +=
        '\ninclude(":aaa_plugin")\nproject(":aaa_plugin").projectDir = file("../../aaa_plugin/android")\n';
    appSettingsFile.writeAsStringSync(appSettingsContent);

    // 5. Manually add the plugin dependency and evaluation dependency in the app's app/build.gradle.kts
    final File appBuildGradleFile = appDir
        .childDirectory('android')
        .childDirectory('app')
        .childFile('build.gradle.kts');
    expect(appBuildGradleFile, exists);

    String appBuildGradleContent = appBuildGradleFile.readAsStringSync();

    // Add evaluationDependsOn at the top of the file to force aaa_plugin to evaluate first
    appBuildGradleContent = 'evaluationDependsOn(":aaa_plugin")\n$appBuildGradleContent';

    // Insert the dependency into the dependencies block
    final dependenciesBlockRegExp = RegExp(r'dependencies\s*\{');
    appBuildGradleContent = appBuildGradleContent.replaceFirst(
      dependenciesBlockRegExp,
      'dependencies {\n    implementation(project(":aaa_plugin"))',
    );
    appBuildGradleFile.writeAsStringSync(appBuildGradleContent);

    // 6. Delete .dart_tool, .packages, pubspec.lock in the plugin directory to simulate a clean git checkout
    final Directory dartToolDir = pluginDir.childDirectory('.dart_tool');
    if (dartToolDir.existsSync()) {
      dartToolDir.deleteSync(recursive: true);
    }
    final File packagesFile = pluginDir.childFile('.packages');
    if (packagesFile.existsSync()) {
      packagesFile.deleteSync();
    }
    final File pubspecLock = pluginDir.childFile('pubspec.lock');
    if (pubspecLock.existsSync()) {
      pubspecLock.deleteSync();
    }

    // 7. Build the app
    final ProcessResult buildResult = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'build',
      'apk',
      '--target-platform=android-arm',
    ], workingDirectory: appDir.path);

    // Verify that the build succeeds and does not incorrectly treat the plugin
    // as an Add-to-app host module (which would fail due to missing Dart config).
    expect(
      buildResult.exitCode,
      0,
      reason: 'Build failed: ${buildResult.stderr}\n${buildResult.stdout}',
    );
    expect(buildResult.stderr, isNot(contains('FileSystemException')));
  });
}
