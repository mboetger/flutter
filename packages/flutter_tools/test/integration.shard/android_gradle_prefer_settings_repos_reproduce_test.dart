// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter_tools/src/android/gradle_utils.dart' show getGradlewFileName;
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/globals.dart' as globals;

import '../src/common.dart';
import '../src/context.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() {
    Cache.flutterRoot = getFlutterRoot();
    tempDir = createResolvedTempDirectorySync('android_gradle_prefer_settings_repos_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testUsingContext(
    'Gradle build does not emit warnings when RepositoriesMode.PREFER_SETTINGS is set and Flutter module is added',
    () async {
      await _runTest(tempDir: tempDir, repositoriesMode: 'PREFER_SETTINGS');
    },
  );

  testUsingContext(
    'Gradle build succeeds without errors when RepositoriesMode.FAIL_ON_PROJECT_REPOS is set and Flutter module is added',
    () async {
      await _runTest(tempDir: tempDir, repositoriesMode: 'FAIL_ON_PROJECT_REPOS');
    },
  );
}

Future<void> _runTest({required Directory tempDir, required String repositoriesMode}) async {
  // 1. Create a Flutter module.
  final ProcessResult createResult = processManager.runSync(<String>[
    flutterBin,
    ...getLocalEngineArguments(),
    'create',
    '--template=module',
    '--project-name=my_flutter_module',
    'my_flutter_module',
  ], workingDirectory: tempDir.path);
  expect(createResult.exitCode, 0, reason: 'flutter create failed: ${createResult.stderr}');

  final String modulePath = fileSystem.path.join(tempDir.path, 'my_flutter_module');
  final Directory moduleAndroidDir = fileSystem.directory(
    fileSystem.path.join(modulePath, '.android'),
  );

  // 2. Create the host app directory structure.
  final String hostAppPath = fileSystem.path.join(tempDir.path, 'host_app');
  final Directory hostAppDir = fileSystem.directory(hostAppPath)..createSync();
  fileSystem.directory(fileSystem.path.join(hostAppPath, 'app')).createSync();

  // Copy Gradle wrapper and properties from the module.
  copyDirectory(moduleAndroidDir.childDirectory('gradle'), hostAppDir.childDirectory('gradle'));
  moduleAndroidDir.childFile('gradlew').copySync(fileSystem.path.join(hostAppPath, 'gradlew'));
  moduleAndroidDir
      .childFile('gradlew.bat')
      .copySync(fileSystem.path.join(hostAppPath, 'gradlew.bat'));
  moduleAndroidDir
      .childFile('local.properties')
      .copySync(fileSystem.path.join(hostAppPath, 'local.properties'));
  moduleAndroidDir
      .childFile('gradle.properties')
      .copySync(fileSystem.path.join(hostAppPath, 'gradle.properties'));

  // Make gradlew executable.
  if (platform.isLinux || platform.isMacOS) {
    final ProcessResult chmodResult = processManager.runSync(<String>[
      'chmod',
      '+x',
      fileSystem.path.join(hostAppPath, 'gradlew'),
    ]);
    expect(chmodResult.exitCode, 0);
  }

  // 3. Create host_app/settings.gradle.
  // Read AGP version from the module settings.gradle to match it.
  final File moduleSettingsGradle = moduleAndroidDir.childFile('settings.gradle');
  final String moduleSettingsContent = moduleSettingsGradle.readAsStringSync();
  final agpVersionRegExp = RegExp(r'id\s+"com\.android\.library"\s+version\s+"([^"]+)"');
  final String agpVersion = agpVersionRegExp.firstMatch(moduleSettingsContent)?.group(1) ?? '9.0.1';

  final File settingsGradle = fileSystem.file(fileSystem.path.join(hostAppPath, 'settings.gradle'));
  settingsGradle.writeAsStringSync('''
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
plugins {
    id "com.android.application" version "$agpVersion" apply false
    id "com.android.library" version "$agpVersion" apply false
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.$repositoriesMode)
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = 'host_app'
include ':app'

setBinding(new Binding([gradle: this]))
evaluate(new File(
  settingsDir.parentFile,
  'my_flutter_module/.android/include_flutter.groovy'
))
''');

  // 4. Create host_app/build.gradle.
  final File rootBuildGradle = fileSystem.file(fileSystem.path.join(hostAppPath, 'build.gradle'));
  rootBuildGradle.writeAsStringSync('''
// Empty root build.gradle
''');

  // 5. Create host_app/app/build.gradle.
  final File appBuildGradle = fileSystem.file(
    fileSystem.path.join(hostAppPath, 'app', 'build.gradle'),
  );
  appBuildGradle.writeAsStringSync('''
plugins {
    id 'com.android.application'
}
android {
    namespace 'com.example.host_app'
    compileSdk 34
    defaultConfig {
        applicationId "com.example.host_app"
        minSdk 21
        targetSdk 34
        versionCode 1
        versionName "1.0"
    }
}
dependencies {
    implementation project(':flutter')
}
''');

  // 6. Run a Gradle task on the host app to trigger evaluation.
  final gradlew = '.${platform.pathSeparator}${getGradlewFileName(platform)}';
  final ProcessResult gradleResult = await processManager.run(
    <String>[gradlew, ...getLocalEngineArguments(), 'help'],
    workingDirectory: hostAppPath,
    environment: <String, String>{
      ...platform.environment,
      if (globals.java != null) ...globals.java!.environment,
    },
  );

  // We expect the build to SUCCEED.
  expect(gradleResult.exitCode, 0, reason: 'Gradle build failed: ${gradleResult.stderr}');

  final output = '${gradleResult.stdout}\n${gradleResult.stderr}';

  // Verify that Gradle does NOT emit warnings/errors about project-level repositories.
  expect(
    output,
    isNot(
      contains(
        "Build was configured to prefer settings repositories over project repositories but repository 'maven' was added by plugin 'dev.flutter.flutter-gradle-plugin'",
      ),
    ),
  );
}
