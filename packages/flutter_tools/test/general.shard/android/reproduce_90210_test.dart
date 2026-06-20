// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_studio.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/android/gradle_errors.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart';
import 'package:flutter_tools/src/android/java.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/fake.dart';
import 'package:unified_analytics/unified_analytics.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fakes.dart';

const minimalV2EmbeddingManifest = r'''
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <application
        android:name="${applicationName}">
        <meta-data
            android:name="flutterEmbedding"
            android:value="2" />
    </application>
</manifest>
''';

class FakeAndroidStudio extends Fake implements AndroidStudio {}

class FakeGradleUtils extends Fake implements GradleUtils {
  @override
  String getExecutable(FlutterProject project) {
    return 'gradlew';
  }
}

void main() {
  group('reproduce issue 90210', () {
    late BufferLogger logger;
    late FakeAnalytics fakeAnalytics;
    late MemoryFileSystem fileSystem;
    late FakeProcessManager processManager;

    setUp(() {
      processManager = FakeProcessManager.empty();
      logger = BufferLogger.test();
      fileSystem = MemoryFileSystem.test();
      Cache.flutterRoot = '';

      fakeAnalytics = getInitializedFakeAnalyticsInstance(
        fs: fileSystem,
        fakeFlutterVersion: FakeFlutterVersion(),
      );
    });

    testUsingContext('Gradle network error does not spew raw multi-line failure logs', () async {
      final builder = AndroidGradleBuilder(
        java: FakeJava(),
        logger: logger,
        processManager: processManager,
        fileSystem: fileSystem,
        artifacts: Artifacts.test(),
        analytics: fakeAnalytics,
        gradleUtils: FakeGradleUtils(),
        platform: FakePlatform(),
        androidStudio: FakeAndroidStudio(),
      );

      const gradleRawSpew = r'''
FAILURE: Build failed with an exception.

* What went wrong:
Could not determine the dependencies of task ':app:lintVitalRelease'.
> Could not resolve all artifacts for configuration ':app:releaseRuntimeClasspath'.
   > Could not resolve io.flutter:armeabi_v7a_release:1.0.0-f0826da7ef2d301eb8f4ead91aaf026aa2b52881.
     Required by:
         project :app
      > Could not resolve io.flutter:armeabi_v7a_release:1.0.0-f0826da7ef2d301eb8f4ead91aaf026aa2b52881.
         > Could not get resource 'https://jcenter.bintray.com/io/flutter/armeabi_v7a_release/1.0.0-f0826da7ef2d301eb8f4ead91aaf026aa2b52881/armeabi_v7a_release-1.0.0-f0826da7ef2d301eb8f4ead91aaf026aa2b52881.pom'.
            > Could not HEAD 'https://jcenter.bintray.com/io/flutter/armeabi_v7a_release/1.0.0-f0826da7ef2d301eb8f4ead91aaf026aa2b52881/armeabi_v7a_release-1.0.0-f0826da7ef2d301eb8f4ead91aaf026aa2b52881.pom'.
               > Read timed out
''';

      // We simulate a gradle run that fails with a network error, triggering a retry.
      // We queue two commands: the initial attempt and the retry.
      const gradleCommand = FakeCommand(
        command: <String>[
          'gradlew',
          '-q',
          '-Ptarget-platform=android-arm,android-arm64,android-x64',
          '-Ptarget=lib/main.dart',
          '-Pbase-application-name=android.app.Application',
          '-Pdart-obfuscation=false',
          '-Ptrack-widget-creation=false',
          '-Ptree-shake-icons=false',
          'assembleRelease',
        ],
        exitCode: 1,
        stdout: gradleRawSpew,
      );
      processManager.addCommand(gradleCommand);
      processManager.addCommand(gradleCommand);

      fileSystem.directory('android').childFile('build.gradle').createSync(recursive: true);
      fileSystem.directory('android').childFile('gradle.properties').createSync(recursive: true);
      fileSystem.directory('android').childDirectory('app').childFile('build.gradle')
        ..createSync(recursive: true)
        ..writeAsStringSync('apply from: irrelevant/flutter.gradle');

      final FlutterProject project = FlutterProject.fromDirectoryTest(fileSystem.currentDirectory);
      project.android.appManifestFile
        ..createSync(recursive: true)
        ..writeAsStringSync(minimalV2EmbeddingManifest);

      await expectLater(() async {
        await builder.buildGradleApp(
          project: project,
          androidBuildInfo: const AndroidBuildInfo(
            BuildInfo(
              BuildMode.release,
              null,
              treeShakeIcons: false,
              packageConfigPath: '.dart_tool/package_config.json',
            ),
          ),
          target: 'lib/main.dart',
          isBuildingBundle: false,
          configOnly: false,
          localGradleErrors: gradleErrors,
          maxRetries: 1,
        );
      }, throwsToolExit(message: 'Gradle task assembleRelease failed with exit code 1'));

      // 1. Assert that the raw multi-line Gradle spew was successfully suppressed.
      expect(
        logger.statusText + logger.errorText,
        isNot(contains('Could not resolve all artifacts for configuration')),
      );

      // 2. Assert that a clean warning about network issues was logged.
      expect(
        logger.statusText + logger.errorText,
        contains(
          'There are network issues attempting to contact jcenter.bintray.com, please stand by...',
        ),
      );

      // 3. Assert that the final fatal error logs the precise resource and server response cleanly.
      expect(
        logger.statusText + logger.errorText,
        contains(
          'Could not obtain the following resource:\nhttps://jcenter.bintray.com/io/flutter/armeabi_v7a_release/1.0.0-f0826da7ef2d301eb8f4ead91aaf026aa2b52881/armeabi_v7a_release-1.0.0-f0826da7ef2d301eb8f4ead91aaf026aa2b52881.pom',
        ),
      );
      expect(
        logger.statusText + logger.errorText,
        contains('The server responded with "Read timed out"'),
      );
    }, overrides: <Type, Generator>{AndroidStudio: () => FakeAndroidStudio(), Java: () => FakeJava()});
  });
}
