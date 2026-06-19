// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@Timeout(Duration(minutes: 5))
library;

import 'dart:io' hide Directory, File, Link;

import 'package:archive/archive.dart';
import 'package:file/file.dart';
import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' show getGradlewFileName;
import 'package:flutter_tools/src/globals.dart' as globals;

import '../src/common.dart';
import '../src/context.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('reproduce_66885_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testUsingContext('Release build via gradlew assembleRelease contains all three ABIs', () async {
    final Directory appDir = tempDir.childDirectory('app');

    // Create a new flutter project
    final ProcessResult createResult = await processManager.run(<String>[
      flutterBin,
      'create',
      '--template=app',
      '--platforms=android',
      'app',
    ], workingDirectory: tempDir.path);
    expect(createResult.exitCode, 0, reason: 'flutter create failed: ${createResult.stderr}');

    // 1. Build via flutter CLI first to cache all dependencies (including local engine if used)
    final ProcessResult buildResult = await processManager.run(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'build',
      'apk',
      '--release',
    ], workingDirectory: appDir.path);
    expect(buildResult.exitCode, 0, reason: 'flutter build apk failed: ${buildResult.stderr}');

    // Construct the environment map containing JAVA_HOME detected by the tool
    final gradleEnv = <String, String>{...Platform.environment, ...?globals.java?.environment};

    // 2. Clean the build directory to ensure we test direct gradle assembly
    final Directory androidDir = appDir.childDirectory('android');
    final String gradlew = androidDir.childFile(getGradlewFileName(platform)).path;
    final ProcessResult cleanResult = await processManager.run(
      <String>[gradlew, 'clean'],
      workingDirectory: androidDir.path,
      environment: gradleEnv,
    );
    expect(cleanResult.exitCode, 0, reason: 'gradlew clean failed: ${cleanResult.stderr}');

    // 3. Build directly via gradlew assembleRelease (no local engine args here)
    final ProcessResult gradleResult = await processManager.run(
      <String>[gradlew, 'assembleRelease'],
      workingDirectory: androidDir.path,
      environment: gradleEnv,
    );
    expect(
      gradleResult.exitCode,
      0,
      reason: 'gradlew assembleRelease failed: ${gradleResult.stderr}\n${gradleResult.stdout}',
    );

    // Verify that the generated APK exists
    final File apkFile = appDir
        .childDirectory('build/app/outputs/apk/release')
        .childFile('app-release.apk');
    expect(apkFile, exists);

    // Check ABIs in the APK using package:archive (100% hermetic and platform-independent)
    final bool hasArm = _checkLibIsInApk(apkFile, 'lib/armeabi-v7a/libflutter.so');
    final bool hasArm64 = _checkLibIsInApk(apkFile, 'lib/arm64-v8a/libflutter.so');
    final bool hasX86_64 = _checkLibIsInApk(apkFile, 'lib/x86_64/libflutter.so');

    expect(hasArm, isTrue, reason: 'armeabi-v7a/libflutter.so is missing from APK');
    expect(hasArm64, isTrue, reason: 'arm64-v8a/libflutter.so is missing from APK');
    expect(hasX86_64, isTrue, reason: 'x86_64/libflutter.so is missing from APK');
  });
}

bool _checkLibIsInApk(File apkFile, String filename) {
  if (!apkFile.existsSync()) {
    throw StateError('APK file not found at ${apkFile.path}');
  }
  final Archive archive = ZipDecoder().decodeBytes(apkFile.readAsBytesSync());
  return archive.findFile(filename) != null;
}
