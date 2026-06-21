// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:args/command_runner.dart';
import 'package:file/file.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/commands/create.dart';
import 'package:flutter_tools/src/globals.dart' as globals;

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/test_flutter_command_runner.dart';
import 'utils/project_testing_utils.dart';

void main() {
  late Directory tempDir;

  setUpAll(() async {
    Cache.disableLocking();
    await ensureFlutterToolsSnapshot();
  });

  setUp(() {
    tempDir = globals.fs.systemTempDirectory.createTempSync(
      'flutter_tools_create_strandhogg_test.',
    );
    Cache.flutterRoot = '../..';
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  tearDownAll(() async {
    await restoreFlutterToolsSnapshot();
  });

  testUsingContext(
    'generated app build.gradle.kts contains StrandHogg mitigation comment (Kotlin)',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);
      final Directory kotlinProjectDir = tempDir.childDirectory('kotlin_project');

      await runner.run(<String>['create', '--no-pub', kotlinProjectDir.path]);

      final File buildGradleFile = kotlinProjectDir
          .childDirectory('android')
          .childDirectory('app')
          .childFile('build.gradle.kts');
      expect(buildGradleFile.existsSync(), isTrue);

      final String buildContent = await buildGradleFile.readAsString();

      const expectedComment =
          '        // The minimum Android SDK version supported by Flutter is 23, but\n'
          '        // we default to 24 to mitigate the StrandHogg vulnerability:\n'
          '        //    https://developer.android.com/privacy-and-security/risks/strandhogg\n'
          '        // If your application is not a high-value target, consider lowering the minSdkVersion\n'
          '        // to 23 to extend your reach to more users.\n'
          '        minSdk = flutter.minSdkVersion';

      expect(buildContent, contains(expectedComment));
    },
  );

  testUsingContext(
    'generated app build.gradle.kts contains StrandHogg mitigation comment (Java)',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);
      final Directory javaProjectDir = tempDir.childDirectory('java_project');

      await runner.run(<String>[
        'create',
        '--no-pub',
        '--android-language',
        'java',
        javaProjectDir.path,
      ]);

      final File buildGradleFile = javaProjectDir
          .childDirectory('android')
          .childDirectory('app')
          .childFile('build.gradle.kts');
      expect(buildGradleFile.existsSync(), isTrue);

      final String buildContent = await buildGradleFile.readAsString();

      const expectedComment =
          '        // The minimum Android SDK version supported by Flutter is 23, but\n'
          '        // we default to 24 to mitigate the StrandHogg vulnerability:\n'
          '        //    https://developer.android.com/privacy-and-security/risks/strandhogg\n'
          '        // If your application is not a high-value target, consider lowering the minSdkVersion\n'
          '        // to 23 to extend your reach to more users.\n'
          '        minSdk = flutter.minSdkVersion';

      expect(buildContent, contains(expectedComment));
    },
  );
}
