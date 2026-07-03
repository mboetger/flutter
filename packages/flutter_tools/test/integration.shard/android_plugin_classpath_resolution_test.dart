// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ignore_for_file: omit_obvious_local_variable_types, specify_nonobvious_local_variable_types, avoid_print

import 'dart:io';

import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/cache.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() {
    Cache.flutterRoot = getFlutterRoot();
    tempDir = createResolvedTempDirectorySync('flutter_plugin_classpath_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  test(
    'plugin buildscript classpath resolution fails if google() repository is missing when built standalone',
    () async {
      // Create Android plugin project
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'create',
        '--template=plugin',
        '--platforms=android',
        'test_android_plugin',
      ], workingDirectory: tempDir.path);

      final Directory pluginDir = tempDir.childDirectory('test_android_plugin');

      // Find the plugin's android directory.
      // Depending on whether it's federated, it might be in the root or a subdirectory.
      Directory pluginAndroidDir = pluginDir.childDirectory('android');
      if (!pluginAndroidDir.existsSync()) {
        pluginAndroidDir = pluginDir
            .childDirectory('test_android_plugin_android')
            .childDirectory('android');
      }
      expect(
        pluginAndroidDir.existsSync(),
        isTrue,
        reason: 'Could not find plugin android directory',
      );

      // Modify plugin's android/build.gradle.kts
      // We comment out the first "google()" repository (in buildscript)
      // and change the AGP version to a version that is likely not in the cache (e.g. 8.4.0)
      // to force a remote resolution attempt.
      final File pluginBuildGradle = pluginAndroidDir.childFile('build.gradle.kts');
      final String originalContent = pluginBuildGradle.readAsStringSync();

      expect(originalContent, contains('buildscript {'));

      // Comment out google() in buildscript
      String modifiedContent = originalContent.replaceFirst('google()', '// google()');

      // Find the AGP version currently in the file
      final RegExp agpVersionRegExp = RegExp(
        r'classpath\("com\.android\.tools\.build:gradle:([^"]+)"\)',
      );
      final Match? match = agpVersionRegExp.firstMatch(originalContent);
      final String currentAgpVersion = match?.group(1) ?? '8.1.0';
      final String targetAgpVersion = currentAgpVersion == '8.4.0' ? '8.3.0' : '8.4.0';

      // Replace the AGP version in the plugin's buildscript
      modifiedContent = modifiedContent.replaceFirst(
        'classpath("com.android.tools.build:gradle:$currentAgpVersion")',
        'classpath("com.android.tools.build:gradle:$targetAgpVersion")',
      );

      print('--- MODIFIED BUILD.GRADLE.KTS PATH ---');
      print(pluginBuildGradle.path);

      pluginBuildGradle.writeAsStringSync(modifiedContent);

      print('--- MODIFIED BUILD.GRADLE.KTS CONTENT ---');
      print(pluginBuildGradle.readAsStringSync());

      // Use the Gradle wrapper from the flutter_tools directory to run the build.
      final String gradleFileName = Platform.isWindows ? 'gradlew.bat' : 'gradlew';
      final File gradleExecutable = fileSystem
          .directory(getFlutterRoot())
          .childDirectory('packages')
          .childDirectory('flutter_tools')
          .childDirectory('gradle')
          .childFile(gradleFileName);

      expect(gradleExecutable.existsSync(), isTrue, reason: 'Gradle wrapper not found');

      // Run gradle properties on the plugin's android directory.
      // We expect it to fail during the configuration phase because it cannot resolve the AGP dependency.
      final ProcessResult gradleResult = await processManager.run(<String>[
        gradleExecutable.path,
        '-p',
        pluginAndroidDir.path,
        'properties',
      ]);

      print('--- GRADLE STDOUT ---');
      print(gradleResult.stdout);
      print('--- GRADLE STDERR ---');
      print(gradleResult.stderr);

      // The build should fail
      expect(gradleResult.exitCode, isNot(0));

      final String output = gradleResult.stderr.toString() + gradleResult.stdout.toString();
      expect(
        output,
        anyOf(
          contains('Could not find com.android.tools.build:gradle'),
          contains("Plugin [id: 'com.android.library'] was not found"),
        ),
      );
    },
  );

  test(
    'plugin buildscript classpath resolution succeeds if google() repository is missing when built as part of an app',
    () async {
      // Create Android plugin project
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'create',
        '--template=plugin',
        '--platforms=android',
        'test_android_plugin',
      ], workingDirectory: tempDir.path);

      final Directory pluginDir = tempDir.childDirectory('test_android_plugin');

      // Find the plugin's android directory.
      Directory pluginAndroidDir = pluginDir.childDirectory('android');
      if (!pluginAndroidDir.existsSync()) {
        pluginAndroidDir = pluginDir
            .childDirectory('test_android_plugin_android')
            .childDirectory('android');
      }

      // Modify plugin's android/build.gradle.kts
      final File pluginBuildGradle = pluginAndroidDir.childFile('build.gradle.kts');
      final String originalContent = pluginBuildGradle.readAsStringSync();
      String modifiedContent = originalContent.replaceFirst('google()', '// google()');

      final RegExp agpVersionRegExp = RegExp(
        r'classpath\("com\.android\.tools\.build:gradle:([^"]+)"\)',
      );
      final Match? match = agpVersionRegExp.firstMatch(originalContent);
      final String currentAgpVersion = match?.group(1) ?? '8.1.0';
      final String targetAgpVersion = currentAgpVersion == '8.4.0' ? '8.3.0' : '8.4.0';

      modifiedContent = modifiedContent.replaceFirst(
        'classpath("com.android.tools.build:gradle:$currentAgpVersion")',
        'classpath("com.android.tools.build:gradle:$targetAgpVersion")',
      );
      pluginBuildGradle.writeAsStringSync(modifiedContent);

      // Now run gradle properties on the *example* app's android directory.
      final Directory exampleAndroidDir = pluginDir
          .childDirectory('example')
          .childDirectory('android');
      expect(exampleAndroidDir.existsSync(), isTrue);

      final String gradleFileName = Platform.isWindows ? 'gradlew.bat' : 'gradlew';
      final File gradleExecutable = exampleAndroidDir.childFile(gradleFileName);
      expect(gradleExecutable.existsSync(), isTrue);

      // Run gradle properties on the example app.
      final ProcessResult gradleResult = await processManager.run(<String>[
        gradleExecutable.path,
        '-p',
        exampleAndroidDir.path,
        'properties',
      ]);

      print('--- GRADLE STDOUT ---');
      print(gradleResult.stdout);
      print('--- GRADLE STDERR ---');
      print(gradleResult.stderr);

      expect(gradleResult.exitCode, 0);
    },
  );

  test(
    'plugin buildscript classpath resolution succeeds if google() repository is missing when built with aar init script',
    () async {
      // Create Android plugin project
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'create',
        '--template=plugin',
        '--platforms=android',
        'test_android_plugin',
      ], workingDirectory: tempDir.path);

      // Create Android module project
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'create',
        '--template=module',
        'test_module',
      ], workingDirectory: tempDir.path);

      final Directory pluginDir = tempDir.childDirectory('test_android_plugin');
      final Directory moduleDir = tempDir.childDirectory('test_module');

      Directory pluginAndroidDir = pluginDir.childDirectory('android');
      if (!pluginAndroidDir.existsSync()) {
        pluginAndroidDir = pluginDir
            .childDirectory('test_android_plugin_android')
            .childDirectory('android');
      }

      final File pluginBuildGradle = pluginAndroidDir.childFile('build.gradle.kts');
      final String originalContent = pluginBuildGradle.readAsStringSync();
      String modifiedContent = originalContent.replaceFirst('google()', '// google()');

      final RegExp agpVersionRegExp = RegExp(
        r'classpath\("com\.android\.tools\.build:gradle:([^"]+)"\)',
      );
      final Match? match = agpVersionRegExp.firstMatch(originalContent);
      final String currentAgpVersion = match?.group(1) ?? '8.1.0';
      final String targetAgpVersion = currentAgpVersion == '8.4.0' ? '8.3.0' : '8.4.0';

      modifiedContent = modifiedContent.replaceFirst(
        'classpath("com.android.tools.build:gradle:$currentAgpVersion")',
        'classpath("com.android.tools.build:gradle:$targetAgpVersion")',
      );
      pluginBuildGradle.writeAsStringSync(modifiedContent);

      // Add plugin to module's pubspec.yaml
      final File pubspecFile = moduleDir.childFile('pubspec.yaml');
      String pubspecContent = pubspecFile.readAsStringSync();
      pubspecContent = pubspecContent.replaceFirst(
        'dependencies:',
        'dependencies:\n  test_android_plugin:\n    path: ../test_android_plugin',
      );
      pubspecFile.writeAsStringSync(pubspecContent);

      // Run flutter build aar on the module.
      final ProcessResult result = await processManager.run(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'build',
        'aar',
      ], workingDirectory: moduleDir.path);

      print('--- FLUTTER BUILD AAR STDOUT ---');
      print(result.stdout);
      print('--- FLUTTER BUILD AAR STDERR ---');
      print(result.stderr);

      expect(result.exitCode, 0);
    },
  );
}
