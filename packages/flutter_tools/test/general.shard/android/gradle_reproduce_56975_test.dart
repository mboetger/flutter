// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/android/gradle_errors.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/file_system.dart';
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

const String minimalV2EmbeddingManifest = r'''
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
  group('gradle build executable check', () {
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

    testUsingContext(
      'throws ToolExit when gradle executable is missing and process manager throws ArgumentError',
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

        // Configure processManager to throw ArgumentError, simulating LocalProcessManager behavior when it cannot find the executable
        processManager.addCommand(
          FakeCommand(
            command: const <String>[
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
            exception: ArgumentError('Cannot find executable for gradlew'),
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

        // We expect a clean ToolExit with a friendly message instead of a raw ArgumentError crash.
        await expectLater(
          () => builder.buildGradleApp(
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
            localGradleErrors: <GradleHandledError>[],
          ),
          throwsToolExit(message: 'Gradlew executable not found'),
        );
      },
    );

    testUsingContext(
      'throws ToolExit when gradle executable is missing and process manager throws ArgumentError during AAR build',
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

        // We must match the command structure generated by buildGradleAar.
        // Cache.flutterRoot is '' in setUp, so absolute path resolves to '/' in memory FS.
        processManager.addCommand(
          FakeCommand(
            command: const <String>[
              'gradlew',
              '-I=/packages/flutter_tools/gradle/aar_init_script.gradle',
              '-Pflutter-root=/',
              '-Poutput-dir=build/',
              '-Pis-plugin=false',
              '-PbuildNumber=1.0',
              '-q',
              '-Pdart-obfuscation=false',
              '-Ptrack-widget-creation=false',
              '-Ptree-shake-icons=false',
              '-Ptarget-platform=android-arm,android-arm64,android-x64',
              'assembleAarRelease',
            ],
            exception: ArgumentError('Cannot find executable for gradlew'),
          ),
        );

        // Set up the project as a module, which is required by buildGradleAar
        final File manifestFile = fileSystem.file('pubspec.yaml');
        manifestFile.createSync(recursive: true);
        manifestFile.writeAsStringSync('''
flutter:
  module:
    androidPackage: com.example.test
''');

        fileSystem.file('.android/gradlew').createSync(recursive: true);
        fileSystem.file('.android/gradle.properties').writeAsStringSync('irrelevant');
        fileSystem.file('.android/build.gradle').createSync(recursive: true);

        final FlutterProject project = FlutterProject.fromDirectoryTest(
          fileSystem.currentDirectory,
        );

        await expectLater(
          () => builder.buildGradleAar(
            project: project,
            androidBuildInfo: const AndroidBuildInfo(
              BuildInfo(
                BuildMode.release,
                null,
                treeShakeIcons: false,
                packageConfigPath: '.dart_tool/package_config.json',
              ),
            ),
            target: '',
            outputDirectory: fileSystem.directory('build/'),
            buildNumber: '1.0',
          ),
          throwsToolExit(message: 'Gradlew executable not found'),
        );
      },
    );
  });
}

class FakeGradleUtils extends Fake implements GradleUtils {
  @override
  String getExecutable(FlutterProject project) {
    return 'gradlew';
  }
}
