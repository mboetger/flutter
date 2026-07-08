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
    tempDir = createResolvedTempDirectorySync('flutter_reproduce_46686_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  test(
    'Gradle build fails with a clear validation error when android.useAndroidX is disabled',
    () async {
      // Create a new app project.
      final ProcessResult createResult = processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'create',
        '--template=app',
        '--platforms=android',
        '--org',
        'com.example.reproduce',
        'reproduce_app',
      ], workingDirectory: tempDir.path);

      expect(createResult.exitCode, 0);

      final Directory appDir = tempDir.childDirectory('reproduce_app');
      final File gradleProperties = appDir.childDirectory('android').childFile('gradle.properties');
      expect(gradleProperties, exists);

      // Update gradle-wrapper.properties to use gradle-9.3.1-all.zip to avoid minimum Gradle version failure.
      // The default template might copy an older gradle version from the cache artifact.
      final File gradleWrapperProperties = appDir
          .childDirectory('android')
          .childDirectory('gradle')
          .childDirectory('wrapper')
          .childFile('gradle-wrapper.properties');
      expect(gradleWrapperProperties, exists);
      final String wrapperContent = gradleWrapperProperties.readAsStringSync();
      final String updatedWrapperContent = wrapperContent.replaceAll(
        RegExp(r'distributionUrl=.*'),
        r'distributionUrl=https\://services.gradle.org/distributions/gradle-9.3.1-all.zip',
      );
      gradleWrapperProperties.writeAsStringSync(updatedWrapperContent);

      // Disable AndroidX in gradle.properties
      final String propertiesContent = gradleProperties.readAsStringSync();
      final String modifiedPropertiesContent = propertiesContent.replaceAll(
        'android.useAndroidX=true',
        'android.useAndroidX=false',
      );
      gradleProperties.writeAsStringSync(modifiedPropertiesContent);

      // Run flutter build apk and expect AGP validation to fail early
      final ProcessResult buildResult = processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'build',
        'apk',
        '--debug',
      ], workingDirectory: appDir.path);

      expect(buildResult.exitCode, isNot(0));
      final String output = buildResult.stdout.toString() + buildResult.stderr.toString();
      expect(output, contains('android.useAndroidX'));
    },
  );
}
