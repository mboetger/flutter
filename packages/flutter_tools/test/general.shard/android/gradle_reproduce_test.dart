// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io' show ProcessException;

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_studio.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/android/gradle_errors.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' as gradle_utils;
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
import '../../src/fake_process_manager.dart';
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

void main() {
  group('gradle build ProcessException handling', () {
    late BufferLogger logger;
    late FakeAnalytics fakeAnalytics;
    late MemoryFileSystem fileSystem;
    late FakeProcessManager processManager;
    late String? oldFlutterRoot;

    setUp(() {
      oldFlutterRoot = Cache.flutterRoot;
      processManager = FakeProcessManager.empty();
      logger = BufferLogger.test();
      fileSystem = MemoryFileSystem.test();
      Cache.flutterRoot = '';

      fakeAnalytics = getInitializedFakeAnalyticsInstance(
        fs: fileSystem,
        fakeFlutterVersion: FakeFlutterVersion(),
      );
    });

    tearDown(() {
      Cache.flutterRoot = oldFlutterRoot;
    });

    testUsingContext(
      'ProcessException thrown during gradle build is caught and wraps in ToolExit',
      () async {
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

        processManager.addCommand(
          const FakeCommand(
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
            exception: ProcessException(
              'gradlew',
              <String>[],
              'Gradlew not found or failed to start',
            ),
          ),
        );

        fileSystem.directory('android').childFile('build.gradle').createSync(recursive: true);
        fileSystem.directory('android').childFile('gradle.properties').createSync(recursive: true);
        fileSystem.directory('android').childDirectory('app').childFile('build.gradle')
          ..createSync(recursive: true)
          ..writeAsStringSync('apply from: irrelevant/flutter.gradle');

        final FlutterProject project = FlutterProject.fromDirectoryTest(
          fileSystem.currentDirectory,
        );
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
          );
        }, throwsToolExit());

        expect(processManager, hasNoRemainingExpectations);
      },
    );
  });
}

class FakeGradleUtils extends Fake implements gradle_utils.GradleUtils {
  @override
  String getExecutable(FlutterProject project) {
    return 'gradlew';
  }
}

class FakeAndroidStudio extends Fake implements AndroidStudio {
  @override
  String get javaPath => '/android-studio/jbr';
}
