// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io' show Platform;

import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('flutter_jni_mismatch_reproduce_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext('JNI libraries mismatch detection catches missing libflutter.so', () async {
    final Directory projectDir = tempDir.childDirectory('app');

    // Create a new flutter app project
    processManager.runSync(<String>[
      flutterBin,
      'create',
      '--template=app',
      '--platforms=android',
      'app',
    ], workingDirectory: tempDir.path);

    // Create a dummy JNI library in armeabi-v7a (32-bit ARM)
    final Directory jniDir = projectDir.childDirectory('android/app/src/main/jniLibs/armeabi-v7a');
    jniDir.createSync(recursive: true);
    jniDir.childFile('libother.so').writeAsStringSync('dummy content');

    // Build the app targeting ONLY 64-bit ARM.
    // This will compile/package libflutter.so only for arm64-v8a.
    // However, the dummy JNI library libother.so will be packaged for armeabi-v7a.
    // This creates an architecture mismatch where libflutter.so is missing for armeabi-v7a.
    final ProcessResult result = processManager.runSync(
      <String>[flutterBin, 'build', 'apk', '--debug', '--target-platform', 'android-arm64'],
      workingDirectory: projectDir.path,
      environment: <String, String>{...Platform.environment, 'FORCE_JNI_MISMATCH_CHECK': 'true'},
    );

    // Verify if the build failed due to the mismatch.
    expect(result.exitCode, isNot(0), reason: 'Build should have failed due to JNI mismatch!');
    final output = '${result.stdout}\n${result.stderr}';
    expect(output, contains('JNI libraries mismatch.'));
    expect(
      output,
      contains(
        'The application contains native libraries for the following ABI(s) but is missing the corresponding libflutter.so:',
      ),
    );
    expect(output, contains('- armeabi-v7a'));
  });
}
