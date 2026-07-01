// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('flutter_plugin_reproduce_19830_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext('plugin android directory can be built standalone', () async {
    const projectName = 'plugin_reproduce_19830';

    // 1. Create the plugin
    ProcessResult result = processManager.runSync(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      '--template=plugin',
      '--platforms=android',
      projectName,
    ], workingDirectory: tempDir.path);

    expect(result.exitCode, 0, reason: 'flutter create failed: ${result.stderr}\n${result.stdout}');

    final Directory pluginDir = tempDir.childDirectory(projectName);
    final Directory pluginAndroidDir = pluginDir.childDirectory('android');
    final Directory exampleAndroidDir = pluginDir
        .childDirectory('example')
        .childDirectory('android');

    // 2. Parse AGP and Kotlin versions from the generated build.gradle.kts
    final File buildGradleFile = pluginAndroidDir.childFile('build.gradle.kts');
    expect(buildGradleFile, exists);
    final String buildGradleContent = buildGradleFile.readAsStringSync();

    final agpRegExp = RegExp(r'com\.android\.tools\.build:gradle:([0-9.]+)');
    final String? agpVersion = agpRegExp.firstMatch(buildGradleContent)?.group(1);
    expect(agpVersion, isNotNull, reason: 'Could not parse AGP version from build.gradle.kts');

    final kotlinRegExp = RegExp(r'kotlinVersion\s*=\s*"([0-9.]+)"');
    final String? kotlinVersion = kotlinRegExp.firstMatch(buildGradleContent)?.group(1);
    expect(
      kotlinVersion,
      isNotNull,
      reason: 'Could not parse Kotlin version from build.gradle.kts',
    );

    // 3. Create settings.gradle.kts with pluginManagement to resolve plugin versions
    final File settingsGradleFile = pluginAndroidDir.childFile('settings.gradle.kts');
    settingsGradleFile.writeAsStringSync('''
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
    resolutionStrategy {
        eachPlugin {
            if (requested.id.id == "com.android.library") {
                useVersion("$agpVersion")
            }
            if (requested.id.id == "org.jetbrains.kotlin.android") {
                useVersion("$kotlinVersion")
            }
        }
    }
}

rootProject.name = "$projectName"
''');

    // 4. Copy Gradle wrapper from example to plugin android
    final Directory gradleWrapperSrc = exampleAndroidDir.childDirectory('gradle');
    final Directory gradleWrapperDst = pluginAndroidDir.childDirectory('gradle');
    _copyDirectory(gradleWrapperSrc, gradleWrapperDst);

    // Copy both gradlew and gradlew.bat for cross-platform compatibility
    for (final gradlewName in <String>['gradlew', 'gradlew.bat']) {
      final File gradlewSrc = exampleAndroidDir.childFile(gradlewName);
      if (gradlewSrc.existsSync()) {
        final File gradlewDst = pluginAndroidDir.childFile(gradlewName);
        gradlewSrc.copySync(gradlewDst.path);
        if (!platform.isWindows && gradlewName == 'gradlew') {
          // Make gradlew executable on non-Windows platforms
          result = processManager.runSync(<String>['chmod', '+x', gradlewDst.path]);
          expect(result.exitCode, 0);
        }
      }
    }

    // 5. We do NOT copy local.properties from example to plugin android here.
    // This verifies that the fallback logic (to ../example/android/local.properties)
    // in the plugin's build.gradle.kts works correctly.

    // 6. Run gradle compileDebugSources in plugin android
    final gradlewName = platform.isWindows ? 'gradlew.bat' : 'gradlew';
    final String gradlewPath = pluginAndroidDir.childFile(gradlewName).path;

    result = processManager.runSync(<String>[
      gradlewPath,
      '-Dorg.gradle.daemon=false', // Prevent leaking Gradle daemon processes
      'compileDebugSources',
    ], workingDirectory: pluginAndroidDir.path);

    // We expect this to succeed because the plugin's build.gradle.kts now
    // declares a dependency on the Flutter Android Embedding using the fallback.
    expect(
      result.exitCode,
      0,
      reason: 'Gradle compilation failed:\n${result.stderr}\n${result.stdout}',
    );
  });
}

/// Helper to recursively copy a directory.
void _copyDirectory(Directory source, Directory destination) {
  destination.createSync(recursive: true);
  for (final FileSystemEntity entity in source.listSync()) {
    final String newPath = destination.fileSystem.path.join(
      destination.path,
      destination.fileSystem.path.basename(entity.path),
    );
    if (entity is File) {
      entity.copySync(newPath);
    } else if (entity is Directory) {
      final Directory newDirectory = destination.fileSystem.directory(newPath);
      _copyDirectory(entity, newDirectory);
    }
  }
}
