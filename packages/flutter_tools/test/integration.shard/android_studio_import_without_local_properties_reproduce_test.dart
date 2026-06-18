// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' show getGradlewFileName;
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;
  late Directory androidApp;
  late File localProperties;
  late String originalLocalPropertiesContent;

  setUpAll(() async {
    tempDir = createResolvedTempDirectorySync('android_studio_import_test.');
    androidApp = tempDir.childDirectory('android');
    localProperties = androidApp.childFile('local.properties');

    // Create a new flutter project.
    ProcessResult result = await processManager.run(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      tempDir.path,
      '--project-name=testapp',
    ], workingDirectory: tempDir.path);
    expect(result, const ProcessResultMatcher());

    // Ensure gradle files are generated.
    result = await processManager.run(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'build',
      'apk',
      '--config-only',
    ], workingDirectory: tempDir.path);
    expect(result, const ProcessResultMatcher());

    originalLocalPropertiesContent = localProperties.readAsStringSync();
  });

  tearDown(() async {
    if (!localProperties.existsSync()) {
      localProperties.createSync();
    }
    localProperties.writeAsStringSync(originalLocalPropertiesContent);
  });

  tearDownAll(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext(
    'gradle execution fails with a clear message when local.properties is missing',
    () async {
      // Verify it exists, then delete it.
      expect(localProperties.existsSync(), isTrue);
      localProperties.deleteSync();

      // Run gradlew directly.
      final ProcessResult gradlewResult = await processManager.run(<String>[
        '.${platform.pathSeparator}${getGradlewFileName(platform)}',
        'projects',
      ], workingDirectory: androidApp.path);

      // The build should fail.
      expect(gradlewResult.exitCode, isNot(0));

      // It must contain a clear error message.
      expect(
        gradlewResult.stderr.toString() + gradlewResult.stdout.toString(),
        contains(
          'local.properties file not found. Please run "flutter pub get" or "flutter run" in the project root to generate it.',
        ),
      );
    },
  );

  testWithoutContext(
    'gradle execution fails with a clear message when flutter.sdk is not set in local.properties',
    () async {
      // Truncate/empty local.properties so it doesn't contain flutter.sdk.
      localProperties.writeAsStringSync('sdk.dir=/path/to/android/sdk\n');

      // Run gradlew directly.
      final ProcessResult gradlewResult = await processManager.run(<String>[
        '.${platform.pathSeparator}${getGradlewFileName(platform)}',
        'projects',
      ], workingDirectory: androidApp.path);

      // The build should fail.
      expect(gradlewResult.exitCode, isNot(0));

      // It must contain a clear error message.
      expect(
        gradlewResult.stderr.toString() + gradlewResult.stdout.toString(),
        contains(
          'flutter.sdk not set in local.properties. Please run "flutter pub get" or "flutter run" in the project root.',
        ),
      );
    },
  );
}
