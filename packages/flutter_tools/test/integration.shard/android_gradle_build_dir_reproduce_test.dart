// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('android_gradle_build_dir_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext(
    'Gradle evaluation with split subprojects blocks (default) does not leak build files to android/app/build',
    () async {
      // 1. Create a new flutter project using the default template.
      ProcessResult result = await processManager.run(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'create',
        tempDir.path,
        '--project-name=testapp',
      ], workingDirectory: tempDir.path);
      expect(result, const ProcessResultMatcher());

      final File buildGradleKtsFile = tempDir
          .childDirectory('android')
          .childFile('build.gradle.kts');
      expect(buildGradleKtsFile, exists);

      // 2. Add a dummy subproject ':aaa' alphabetically before ':app' to trigger eager evaluation.
      final File settingsGradleKtsFile = tempDir
          .childDirectory('android')
          .childFile('settings.gradle.kts');
      expect(settingsGradleKtsFile, exists);

      settingsGradleKtsFile.writeAsStringSync(
        '\ninclude(":aaa")\nproject(":aaa").projectDir = file("aaa")\n',
        mode: FileMode.append,
        flush: true,
      );

      final Directory aaaDir = tempDir.childDirectory('android').childDirectory('aaa');
      aaaDir.createSync(recursive: true);
      aaaDir.childFile('build.gradle.kts').writeAsStringSync('', flush: true);

      // 3. Run config-only to set up Gradle wrapper etc.
      result = await processManager.run(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'build',
        'apk',
        '--config-only',
      ], workingDirectory: tempDir.path);
      expect(result, const ProcessResultMatcher());

      final Directory androidDir = tempDir.childDirectory('android');
      final Directory androidAppBuildDir = androidDir.childDirectory('app').childDirectory('build');

      // Ensure it starts clean
      if (androidAppBuildDir.existsSync()) {
        androidAppBuildDir.deleteSync(recursive: true);
      }

      // 4. Run a real flutter build apk to trigger project evaluation and build tasks.
      result = await processManager.run(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'build',
        'apk',
        '--target-platform=android-arm',
      ], workingDirectory: tempDir.path);
      expect(result, const ProcessResultMatcher());

      // 5. Verify that android/app/build was NOT created.
      expect(androidAppBuildDir, isNot(exists));
    },
  );
}
