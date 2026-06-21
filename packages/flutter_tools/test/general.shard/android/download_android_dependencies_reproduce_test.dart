// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_studio.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:unified_analytics/unified_analytics.dart';

import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fake_process_manager.dart';
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
  group('Download and list Android dependencies reproduction', () {
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
      'buildApk with configOnly should download and list dependencies for application',
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

        // We expect Gradle to be run to download/list dependencies, e.g. with a task like 'dependencies'
        // or a custom task. Since the feature is not implemented, no command will be run,
        // so we add a command here that we *expect* to run. When the test is executed,
        // if the command is not run, it will fail.
        processManager.addCommand(
          const FakeCommand(
            command: <String>['gradlew', '-q', 'dependencies'],
            stdout: 'Resolved dependencies info...',
          ),
        );

        // Set up the application project
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

        // Run buildApk with configOnly: true
        await builder.buildApk(
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
          configOnly: true,
        );

        // Verify that the process manager executed the expected command
        expect(processManager, hasNoRemainingExpectations);
      },
      overrides: <Type, Generator>{AndroidSdk: () => _FakeAndroidSdk(fileSystem.directory('sdk'))},
    );

    testUsingContext(
      'download and list dependencies should work for a plugin project',
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

        // We expect Gradle to be run on the plugin project's android folder
        processManager.addCommand(
          const FakeCommand(
            command: <String>['gradlew', '-q', 'dependencies'],
            stdout: 'Plugin resolved dependencies...',
          ),
        );

        // Set up a plugin project
        final Directory pluginDirectory = fileSystem.directory('plugin_project');
        pluginDirectory.childDirectory('android').createSync(recursive: true);
        pluginDirectory.childFile('pubspec.yaml').writeAsStringSync('''
name: my_plugin
flutter:
  plugin:
    androidPackage: com.example
    pluginClass: MyPlugin
''');
        pluginDirectory
            .childDirectory('android')
            .childFile('build.gradle')
            .createSync(recursive: true);

        final FlutterProject project = FlutterProject.fromDirectoryTest(pluginDirectory);

        // Currently, there is no way to run buildApk or resolve dependencies on a plugin.
        // We attempt to build/configure the plugin's android dependencies.
        // Since we want this to work, we call buildApk with configOnly: true.
        await builder.buildApk(
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
          configOnly: true,
        );

        expect(processManager, hasNoRemainingExpectations);
      },
      overrides: <Type, Generator>{AndroidSdk: () => _FakeAndroidSdk(fileSystem.directory('sdk'))},
    );
  });
}

class FakeGradleUtils extends Fake implements GradleUtils {
  @override
  String getExecutable(FlutterProject project) {
    return 'gradlew';
  }
}

class _FakeAndroidSdk extends Fake implements AndroidSdk {
  _FakeAndroidSdk(this.directory);

  @override
  final Directory directory;
}
