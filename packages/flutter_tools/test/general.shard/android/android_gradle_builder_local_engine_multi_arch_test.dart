// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_studio.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/android/gradle_errors.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:flutter_tools/src/runner/local_engine.dart';
import 'package:test/fake.dart';

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
  group('LocalEngineLocator multi-arch reproduction (flutter/flutter#71120)', () {
    testWithoutContext(
      'fails when multiple local engine targets (comma-separated) are specified',
      () async {
        final fileSystem = MemoryFileSystem.test();
        const engineRoot = '/arbitrary/engine';
        fileSystem.directory('$engineRoot/src/out/android_debug').createSync(recursive: true);
        fileSystem.directory('$engineRoot/src/out/android_debug_arm64').createSync(recursive: true);
        fileSystem.directory('$engineRoot/src/out/host_debug').createSync(recursive: true);

        final logger = BufferLogger.test();
        final localEngineLocator = LocalEngineLocator(
          fileSystem: fileSystem,
          flutterRoot: '',
          logger: logger,
          userMessages: UserMessages(),
          platform: FakePlatform(environment: <String, String>{}),
        );

        // In flutter/flutter#71120 attempt 1:
        // Specifying multiple engines via comma-separated string:
        // --local-engine=android_debug,android_debug_arm64
        // LocalEngineLocator treats the string as a single directory name and
        // throws ToolExit. This test asserts findEnginePath locates paths for
        // multi-arch build, which currently fails with ToolExit: "No Flutter
        // engine build found at /arbitrary/engine/src/out/android_debug,..."
        final EngineBuildPaths? paths = await localEngineLocator.findEnginePath(
          engineSourcePath: '$engineRoot/src',
          localEngine: 'android_debug,android_debug_arm64',
          localHostEngine: 'host_debug',
        );

        expect(paths, isNotNull);
      },
    );
  });

  group('AndroidGradleBuilder multi-arch reproduction (flutter/flutter#71120)', () {
    testUsingContext('passes multiple target-platforms to Gradle with local engine', () async {
      final fileSystem = MemoryFileSystem.test();
      final logger = BufferLogger.test();
      final processManager = FakeProcessManager.empty();

      final builder = AndroidGradleBuilder(
        java: FakeJava(),
        logger: logger,
        processManager: processManager,
        fileSystem: fileSystem,
        artifacts: Artifacts.testLocalEngine(
          localEngine: 'out/android_debug',
          localEngineHost: 'out/host_debug',
        ),
        analytics: getInitializedFakeAnalyticsInstance(
          fs: fileSystem,
          fakeFlutterVersion: FakeFlutterVersion(),
        ),
        gradleUtils: FakeGradleUtils(),
        platform: FakePlatform(),
        androidStudio: FakeAndroidStudio(),
      );

      // In flutter/flutter#71120 attempt 2:
      // When building an AppBundle for multiple target architectures while
      // using a local engine, gradle.dart overrides target-platform to only
      // a single architecture (e.g. -Ptarget-platform=android-arm) derived
      // from localEngineInfo.targetOutPath, ignoring androidBuildInfo.
      // Here we expect Gradle to be invoked with both target architectures:
      // -Ptarget-platform=android-arm,android-arm64.
      processManager.addCommand(
        const FakeCommand(
          command: <String>[
            'gradlew',
            '-q',
            '-Plocal-engine-repo=/.tmp_rand0/flutter_tool_local_engine_repo.rand0',
            '-Plocal-engine-build-mode=debug',
            '-Plocal-engine-out=out/android_debug',
            '-Plocal-engine-host-out=out/host_debug',
            '-Ptarget-platform=android-arm,android-arm64',
            '-Ptarget=lib/main.dart',
            '-Pbase-application-name=android.app.Application',
            '-Pdart-obfuscation=false',
            '-Ptrack-widget-creation=false',
            '-Ptree-shake-icons=false',
            'bundleDebug',
          ],
        ),
      );

      fileSystem.file('out/android_debug/flutter_embedding_debug.pom')
        ..createSync(recursive: true)
        ..writeAsStringSync('''
<?xml version="1.0" encoding="UTF-8"?>
<project>
  <version>1.0.0-73fd6b049a80bcea2db1f26c7cee434907cd188b</version>
  <dependencies>
  </dependencies>
</project>
''');
      fileSystem.file('out/android_debug/armeabi_v7a_debug.pom').createSync(recursive: true);
      fileSystem.file('out/android_debug/armeabi_v7a_debug.jar').createSync(recursive: true);
      fileSystem
          .file('out/android_debug/armeabi_v7a_debug.maven-metadata.xml')
          .createSync(recursive: true);
      fileSystem.file('out/android_debug/arm64_v8a_debug.pom').createSync(recursive: true);
      fileSystem.file('out/android_debug/arm64_v8a_debug.jar').createSync(recursive: true);
      fileSystem
          .file('out/android_debug/arm64_v8a_debug.maven-metadata.xml')
          .createSync(recursive: true);
      fileSystem.file('out/android_debug/flutter_embedding_debug.jar').createSync(recursive: true);
      fileSystem
          .file('out/android_debug/flutter_embedding_debug.maven-metadata.xml')
          .createSync(recursive: true);

      fileSystem.file('android/gradlew').createSync(recursive: true);
      fileSystem.directory('android').childFile('gradle.properties').createSync(recursive: true);
      fileSystem.file('android/build.gradle').createSync(recursive: true);
      fileSystem.directory('android').childDirectory('app').childFile('build.gradle')
        ..createSync(recursive: true)
        ..writeAsStringSync('apply from: irrelevant/flutter.gradle');
      final FlutterProject project = FlutterProject.fromDirectoryTest(fileSystem.currentDirectory);
      project.android.appManifestFile
        ..createSync(recursive: true)
        ..writeAsStringSync(minimalV2EmbeddingManifest);

      // Create mock .aab file so that findBundleFile succeeds after Gradle runs.
      fileSystem.file('/build/app/outputs/bundle/debug/app-debug.aab').createSync(recursive: true);

      await builder.buildGradleApp(
        project: project,
        androidBuildInfo: const AndroidBuildInfo(
          BuildInfo(
            BuildMode.debug,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
          targetArchs: <AndroidArch>[AndroidArch.armeabi_v7a, AndroidArch.arm64_v8a],
        ),
        target: 'lib/main.dart',
        isBuildingBundle: true,
        configOnly: false,
        localGradleErrors: const <GradleHandledError>[],
      );
    });
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
