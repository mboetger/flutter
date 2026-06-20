// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';

import 'package:file/file.dart';
import 'package:flutter_tools/src/base/process.dart';
import 'package:flutter_tools/src/globals.dart' as globals;

import '../src/common.dart';
import '../src/context.dart';

void main() {
  late Directory tempDir;

  setUp(() {
    tempDir = globals.fs.systemTempDirectory.createTempSync('gradle_repo_override_test.');
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  testUsingContext('settings.gradle.kts respects FLUTTER_GRADLE_REPOS', () async {
    final gradleFileName = Platform.isWindows ? 'gradlew.bat' : 'gradlew';
    final gradleExecutable = Platform.isWindows ? '.\\$gradleFileName' : './$gradleFileName';
    final Directory flutterGradlePluginDirectory = globals.fs
        .directory(getFlutterRoot())
        .childDirectory('packages')
        .childDirectory('flutter_tools')
        .childDirectory('gradle');

    globals.gradleUtils?.injectGradleWrapperIfNeeded(flutterGradlePluginDirectory);
    makeExecutable(flutterGradlePluginDirectory.childFile(gradleFileName));

    // Create a temporary init script to print the settings repositories.
    final File initScript = tempDir.childFile('init.gradle');
    initScript.writeAsStringSync('''
      settingsEvaluated { settings ->
          var repos = []
          settings.dependencyResolutionManagement.repositories.each { repo ->
              if (repo instanceof MavenArtifactRepository) {
                  repos.add(repo.url.toString())
              } else {
                  repos.add(repo.name)
              }
          }
          println("SETTINGS_REPOS: " + repos)
      }
    ''');

    final environment = <String, String>{
      'FLUTTER_GRADLE_REPOS':
          'https://example.com/repo1,https://example.com/repo2;https://example.com/repo3',
    };

    // Use the Java Home detected by the Flutter tool.
    final String? javaHome = globals.java?.javaHome;
    if (javaHome != null) {
      environment['JAVA_HOME'] = javaHome;
    }

    final RunResult runResult = await globals.processUtils.run(
      <String>[gradleExecutable, '-I', initScript.path, 'help'],
      workingDirectory: flutterGradlePluginDirectory.path,
      environment: environment,
    );

    final String stdout = runResult.stdout;
    expect(
      stdout,
      contains('SETTINGS_REPOS:'),
      reason:
          'Gradle execution did not print SETTINGS_REPOS. Output:\n$stdout\nError:\n${runResult.stderr}',
    );

    // Extract the SETTINGS_REPOS line.
    final regex = RegExp(r'SETTINGS_REPOS: \[(.*)\]');
    final Match? match = regex.firstMatch(stdout);
    expect(match, isNotNull, reason: 'Could not find SETTINGS_REPOS in output:\n$stdout');

    final String reposString = match!.group(1)!;
    final List<String> repos = reposString.split(',').map((String s) => s.trim()).toList();

    expect(
      repos,
      containsAll(<String>[
        'https://example.com/repo1',
        'https://example.com/repo2',
        'https://example.com/repo3',
      ]),
      reason: 'Custom repositories were not all added in order. Repositories found: $repos',
    );
    expect(
      repos,
      isNot(contains('https://dl.google.com/dl/android/maven2/')),
      reason: 'Default Google repository was not removed when override was set.',
    );
    expect(
      repos,
      isNot(contains('https://repo.maven.apache.org/maven2/')),
      reason: 'Default Maven Central repository was not removed when override was set.',
    );
  });

  testUsingContext('resolve_dependencies.gradle.kts respects FLUTTER_GRADLE_REPOS', () async {
    final gradleFileName = Platform.isWindows ? 'gradlew.bat' : 'gradlew';
    final gradleExecutable = Platform.isWindows ? '.\\$gradleFileName' : './$gradleFileName';
    final Directory flutterGradlePluginDirectory = globals.fs
        .directory(getFlutterRoot())
        .childDirectory('packages')
        .childDirectory('flutter_tools')
        .childDirectory('gradle');

    globals.gradleUtils?.injectGradleWrapperIfNeeded(flutterGradlePluginDirectory);
    makeExecutable(flutterGradlePluginDirectory.childFile(gradleFileName));

    // Create a temporary init script to print the project repositories.
    final File initScript = tempDir.childFile('init.gradle');
    initScript.writeAsStringSync('''
      projectsEvaluated { gradle ->
          var repos = []
          gradle.rootProject.repositories.each { repo ->
              if (repo instanceof MavenArtifactRepository) {
                  repos.add(repo.url.toString())
              } else {
                  repos.add(repo.name)
              }
          }
          println("PROJECT_REPOS: " + repos)
      }
    ''');

    final environment = <String, String>{
      'FLUTTER_GRADLE_REPOS':
          'https://example.com/repo1,https://example.com/repo2;https://example.com/repo3',
    };

    // Use the Java Home detected by the Flutter tool.
    final String? javaHome = globals.java?.javaHome;
    if (javaHome != null) {
      environment['JAVA_HOME'] = javaHome;
    }

    // Temporarily swap settings.gradle.kts to point to resolve_dependencies.gradle.kts.
    // In Gradle 9.0, specifying a custom build file with `-b` is no longer supported,
    // so we configure it dynamically via the settings file.
    final File settingsFile = flutterGradlePluginDirectory.childFile('settings.gradle.kts');
    final File settingsBackup = flutterGradlePluginDirectory.childFile('settings.gradle.kts.bak');
    final File tempSettingsFile = flutterGradlePluginDirectory.childFile(
      'settings.gradle.kts.temp',
    );

    if (settingsBackup.existsSync()) {
      settingsBackup.deleteSync();
    }
    if (tempSettingsFile.existsSync()) {
      tempSettingsFile.deleteSync();
    }

    settingsFile.renameSync(settingsBackup.path);
    tempSettingsFile.writeAsStringSync(
      'rootProject.buildFileName = "resolve_dependencies.gradle.kts"\n',
    );
    tempSettingsFile.renameSync(settingsFile.path);

    try {
      final RunResult runResult = await globals.processUtils.run(
        <String>[gradleExecutable, '-I', initScript.path, 'help'],
        workingDirectory: flutterGradlePluginDirectory.path,
        environment: environment,
      );

      final String stdout = runResult.stdout;
      expect(
        stdout,
        contains('PROJECT_REPOS:'),
        reason:
            'Gradle execution did not print PROJECT_REPOS. Output:\n$stdout\nError:\n${runResult.stderr}',
      );

      // Extract the PROJECT_REPOS line.
      final regex = RegExp(r'PROJECT_REPOS: \[(.*)\]');
      final Match? match = regex.firstMatch(stdout);
      expect(match, isNotNull, reason: 'Could not find PROJECT_REPOS in output:\n$stdout');

      final String reposString = match!.group(1)!;
      final List<String> repos = reposString.split(',').map((String s) => s.trim()).toList();

      expect(
        repos,
        containsAll(<String>[
          'https://example.com/repo1',
          'https://example.com/repo2',
          'https://example.com/repo3',
        ]),
        reason: 'Custom repositories were not all added. Repositories found: $repos',
      );
      expect(
        repos,
        isNot(contains('https://dl.google.com/dl/android/maven2/')),
        reason: 'Default Google repository was not removed when override was set.',
      );
      expect(
        repos,
        isNot(contains('https://repo.maven.apache.org/maven2/')),
        reason: 'Default Maven Central repository was not removed when override was set.',
      );

      // Ensure the engine download repository is still there.
      final bool hasEngineRepo = repos.any((String url) => url.endsWith('download.flutter.io'));
      expect(
        hasEngineRepo,
        isTrue,
        reason: 'Engine download repository was removed. Repositories found: $repos',
      );
    } finally {
      // Restore settings.gradle.kts
      if (settingsFile.existsSync()) {
        settingsFile.deleteSync();
      }
      settingsBackup.renameSync(settingsFile.path);
    }
  });
}

void makeExecutable(File file) {
  if (Platform.isWindows) {
    return;
  }
  final ProcessResult result = Process.runSync('chmod', <String>['+x', file.path]);
  expect(result.exitCode, 0);
}
