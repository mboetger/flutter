// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@Timeout(Duration(minutes: 5))
library;

import 'dart:convert';

import 'package:file/file.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' show getGradlewFileName;
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('proguard_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  // Helper to parse the custom Gradle task output.
  ({bool? minifyEnabled, List<String> proguardPaths}) parseGradleOutput(String stdout) {
    bool? minifyEnabled;
    final proguardPaths = <String>[];
    for (final String line in LineSplitter.split(stdout)) {
      if (line.startsWith('MinifyEnabled: ')) {
        minifyEnabled = line.substring('MinifyEnabled: '.length) == 'true';
      }
      if (line.startsWith('ProguardClassPath: ')) {
        proguardPaths.add(line.substring('ProguardClassPath: '.length));
      }
    }
    return (minifyEnabled: minifyEnabled, proguardPaths: proguardPaths);
  }

  testWithoutContext(
    'Flutter Gradle Plugin automatically includes default Flutter Proguard rules',
    () async {
      // Create a new flutter project.
      ProcessResult result = await processManager.run(<String>[
        flutterBin,
        'create',
        tempDir.path,
        '--project-name=testapp',
      ], workingDirectory: tempDir.path);
      expect(result, const ProcessResultMatcher());

      // Ensure that gradle files exist.
      result = await processManager.run(<String>[
        flutterBin,
        'build',
        'apk',
        '--config-only',
      ], workingDirectory: tempDir.path);
      expect(result, const ProcessResultMatcher());

      final Directory androidApp = tempDir.childDirectory('android');
      final File buildGradle = androidApp.childDirectory('app').childFile('build.gradle.kts');
      expect(buildGradle.existsSync(), isTrue);

      // Append a custom gradle task to print the proguard files of the release build type.
      // We use kotlin syntax since build.gradle.kts is Kotlin DSL.
      const customTask = r'''

tasks.register("printReleaseProguardFiles") {
    doLast {
        val android = project.extensions.getByType(com.android.build.api.dsl.ApplicationExtension::class.java)
        val release = android.buildTypes.getByName("release")
        println("MinifyEnabled: ${release.isMinifyEnabled}")
        release.proguardFiles.forEach { file ->
            println("ProguardClassPath: ${file.absolutePath}")
        }
    }
}
''';
      buildGradle.writeAsStringSync('${buildGradle.readAsStringSync()}\n$customTask');

      // 1. Default run (Minification Enabled)
      result = await processManager.run(<String>[
        '.${platform.pathSeparator}${getGradlewFileName(platform)}',
        ...getLocalEngineArguments(),
        '-q', // quiet output.
        'printReleaseProguardFiles',
      ], workingDirectory: androidApp.path);
      expect(result, const ProcessResultMatcher());

      final ({bool? minifyEnabled, List<String> proguardPaths}) output = parseGradleOutput(
        result.stdout.toString(),
      );

      expect(output.minifyEnabled, isTrue, reason: 'Minification should be enabled by default');

      final String flutterRulesPath = output.proguardPaths.firstWhere(
        (String p) => p.contains('flutter_proguard_rules.pro'),
        orElse: () => '',
      );
      expect(
        flutterRulesPath,
        isNotEmpty,
        reason: 'Output did not contain flutter_proguard_rules.pro. Output:\n${result.stdout}',
      );
      expect(
        fileSystem.file(flutterRulesPath).existsSync(),
        isTrue,
        reason: 'Proguard rules file does not exist at $flutterRulesPath',
      );

      // 2. Opt-out run (-Pshrink=false)
      result = await processManager.run(<String>[
        '.${platform.pathSeparator}${getGradlewFileName(platform)}',
        ...getLocalEngineArguments(),
        '-Pshrink=false',
        '-q', // quiet output.
        'printReleaseProguardFiles',
      ], workingDirectory: androidApp.path);
      expect(result, const ProcessResultMatcher());

      final ({bool? minifyEnabled, List<String> proguardPaths}) outputOptOut = parseGradleOutput(
        result.stdout.toString(),
      );

      expect(
        outputOptOut.minifyEnabled,
        isFalse,
        reason: 'Minification should be disabled when -Pshrink=false is passed',
      );
      expect(
        outputOptOut.proguardPaths.any((String p) => p.contains('flutter_proguard_rules.pro')),
        isFalse,
        reason:
            'Output should not contain flutter_proguard_rules.pro when shrinking is disabled. Output:\n${result.stdout}',
      );

      // 3. Local rules run
      // Create a local proguard-rules.pro file.
      final File localProguardRules = androidApp
          .childDirectory('app')
          .childFile('proguard-rules.pro');
      localProguardRules.writeAsStringSync('# Local rules');

      result = await processManager.run(<String>[
        '.${platform.pathSeparator}${getGradlewFileName(platform)}',
        ...getLocalEngineArguments(),
        '-q', // quiet output.
        'printReleaseProguardFiles',
      ], workingDirectory: androidApp.path);
      expect(result, const ProcessResultMatcher());

      final ({bool? minifyEnabled, List<String> proguardPaths}) outputLocal = parseGradleOutput(
        result.stdout.toString(),
      );

      expect(
        outputLocal.proguardPaths.any((String p) => p.contains('proguard-rules.pro')),
        isTrue,
        reason: 'Output did not contain local proguard-rules.pro. Output:\n${result.stdout}',
      );
    },
  );
}
