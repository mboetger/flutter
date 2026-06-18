// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_studio.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/android/gradle_errors.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart';
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

void main() {
  group('gradle granular progress', () {
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
      'Gradle build shows granular progress updates',
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
              '-Ptarget-platform=android-arm,android-arm64,android-x64',
              '-Ptarget=lib/main.dart',
              '-Pbase-application-name=android.app.Application',
              '-Pdart-obfuscation=false',
              '-Ptrack-widget-creation=false',
              '-Ptree-shake-icons=false',
              'assembleRelease',
            ],
            stdout:
                '> Task :app:compileReleaseJavaWithJavac\n'
                '> Task :app:flutterBuildRelease\n'
                '> Task :app:assembleRelease\n',
          ),
        );

        fileSystem.directory('android').childFile('build.gradle').createSync(recursive: true);
        fileSystem.directory('android').childFile('gradle.properties').createSync(recursive: true);
        fileSystem.directory('android').childDirectory('app').childFile('build.gradle')
          ..createSync(recursive: true)
          ..writeAsStringSync('apply from: irrelevant/flutter.gradle');

        fileSystem
            .directory('build')
            .childDirectory('app')
            .childDirectory('outputs')
            .childDirectory('flutter-apk')
            .childFile('app-release.apk')
            .createSync(recursive: true);

        final FlutterProject project = FlutterProject.fromDirectoryTest(
          fileSystem.currentDirectory,
        );
        project.android.appManifestFile
          ..createSync(recursive: true)
          ..writeAsStringSync(minimalV2EmbeddingManifest);

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
          localGradleErrors: <GradleHandledError>[],
        );

        // Expect to show granular progress updates like Xcode build progression.
        expect(logger.statusText, contains('Building Dart code...'));
        expect(logger.statusText, contains('Compiling Kotlin/Java...'));
      },
      overrides: <Type, Generator>{
        AndroidSdk: () => FakeAndroidSdk(fileSystem.directory('/android-sdk')),
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

class FakeAndroidStudio extends Fake implements AndroidStudio {
  @override
  String get javaPath => '/android-studio/jbr';
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  FakeAndroidSdk(this._directory);

  final Directory _directory;

  @override
  Directory get directory => _directory;
}
