// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@Timeout(Duration(minutes: 5))

import 'dart:io';
import 'package:file/file.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' show getGradlewFileName;
import 'package:flutter_tools/src/base/io.dart';
import 'package:test/test.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;
  late Directory airlockRepo;
  late Directory publicRepo;
  late Directory projectDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('fallback_repro_test.');
    airlockRepo = tempDir.childDirectory('airlock-repo')..createSync();
    publicRepo = tempDir.childDirectory('public-repo')..createSync();
    projectDir = tempDir.childDirectory('testapp');

    // 1. Retrieve the engine version stamp.
    final String engineVersion = fileSystem.file(
      fileSystem.path.join(getFlutterRoot(), 'bin', 'cache', 'engine.stamp'),
    ).readAsStringSync().trim();

    // Helper to create a mock artifact in the public repository.
    void createMockArtifact(String artifactId, String version) {
      final String artifactPath = fileSystem.path.join(
        publicRepo.path,
        'io', 'flutter', artifactId, version,
      );
      fileSystem.directory(artifactPath).createSync(recursive: true);

      final File pomFile = fileSystem.file(
        fileSystem.path.join(artifactPath, '$artifactId-$version.pom'),
      );
      pomFile.writeAsStringSync('''<?xml version="1.0" encoding="UTF-8"?>
<project xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 http://maven.apache.org/xsd/maven-4.0.0.xsd" xmlns="http://maven.apache.org/POM/4.0.0"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <modelVersion>4.0.0</modelVersion>
  <groupId>io.flutter</groupId>
  <artifactId>$artifactId</artifactId>
  <version>$version</version>
  <packaging>jar</packaging>
</project>
''');

      final File jarFile = fileSystem.file(
        fileSystem.path.join(artifactPath, '$artifactId-$version.jar'),
      );
      jarFile.writeAsStringSync('dummy jar content');
    }

    // Populate all required engine artifacts and our dummy test dependency.
    createMockArtifact('flutter_embedding_debug', '1.0.0-dummy');
    createMockArtifact('flutter_embedding_debug', '1.0.0-$engineVersion');
    createMockArtifact('armeabi_v7a_debug', '1.0.0-$engineVersion');
    createMockArtifact('arm64_v8a_debug', '1.0.0-$engineVersion');
    createMockArtifact('x86_64_debug', '1.0.0-$engineVersion');

    // 2. Create a new genuine Flutter project using the tool.
    final ProcessResult createResult = await processManager.run(<String>[
      flutterBin,
      'create',
      '--project-name=testapp',
      '--platforms=android',
      projectDir.path,
    ]);
    expect(createResult.exitCode, 0, reason: 'flutter create failed: ${createResult.stderr}');

    // Ensure gradle files are generated.
    final ProcessResult buildConfigResult = await processManager.run(<String>[
      flutterBin,
      'build',
      'apk',
      '--config-only',
    ], workingDirectory: projectDir.path);
    expect(buildConfigResult.exitCode, 0, reason: 'flutter build apk --config-only failed: ${buildConfigResult.stderr}');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext('gradle build fails if dependency is missing from airlock and fallback is restricted', () async {
    final Directory androidDir = projectDir.childDirectory('android');
    final File rootBuildGradle = androidDir.childFile('build.gradle.kts');
    final File appBuildGradle = androidDir.childDirectory('app').childFile('build.gradle.kts');

    // 3. Inject our mock public repository into allprojects.repositories in root build.gradle.kts
    expect(rootBuildGradle.existsSync(), isTrue);
    String rootBuildContent = rootBuildGradle.readAsStringSync();
    // Insert our mock public repository at the end of root build.gradle.kts
    rootBuildContent += '''
allprojects {
    repositories {
        maven {
            url = uri("${publicRepo.uri}")
        }
    }
}
''';
    rootBuildGradle.writeAsStringSync(rootBuildContent);

    // 4. Inject our dummy dependency and a custom resolution task into app build.gradle.kts
    expect(appBuildGradle.existsSync(), isTrue);
    String appBuildContent = appBuildGradle.readAsStringSync();
    appBuildContent += '''
dependencies {
    implementation("io.flutter:flutter_embedding_debug:1.0.0-dummy")
}

tasks.register("resolveDummyDependency") {
    doLast {
        configurations.getByName("debugCompileClasspath").resolve()
        logger.quiet("RESOLVED_SUCCESSFULLY")
    }
}
''';
    appBuildGradle.writeAsStringSync(appBuildContent);

    // 5. Run the gradle task with:
    //    - FLUTTER_STORAGE_BASE_URL pointing to the empty airlockRepo.
    final String gradlewExecutable = '.${platform.pathSeparator}${getGradlewFileName(platform)}';

    final ProcessResult result = await processManager.run(
      <String>[
        gradlewExecutable,
        ':app:resolveDummyDependency',
        '-q',
        '--no-daemon',
      ],
      workingDirectory: androidDir.path,
      environment: <String, String>{
        'FLUTTER_STORAGE_BASE_URL': airlockRepo.uri.toString(),
      },
    );

    print('=== GRADLE STDOUT ===\n${result.stdout}');
    print('=== GRADLE STDERR ===\n${result.stderr}');

    // Secure Behavior Expectation:
    // If the dependency belongs to the 'io.flutter' group, it should ONLY be resolved from the Flutter
    // repository (which is empty in this test). It must NOT fall back to our mock public repository.
    // Therefore, the build should FAIL (exitCode != 0).
    //
    // Under the current insecure behavior, Gradle falls back to the public repository, resolves the dependency,
    // and the build succeeds (exitCode == 0).
    // Thus, this assertion will FAIL on the current codebase, successfully demonstrating the vulnerability!
    expect(
      result.exitCode,
      isNot(0),
      reason: 'The build succeeded by falling back to the public repository, '
              'demonstrating the vulnerability! It should have failed because '
              'the dependency is missing from the mock Airlock repository.',
    );
  });
}
