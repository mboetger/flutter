// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_studio.dart';
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

const String apkanalyzerOutputWithX86NoFlutter = '''
/
/base/
/base/lib/
/base/lib/arm64-v8a/
/base/lib/arm64-v8a/libflutter.so
/base/lib/arm64-v8a/libapp.so
/base/lib/armeabi-v7a/
/base/lib/armeabi-v7a/libflutter.so
/base/lib/armeabi-v7a/libapp.so
/base/lib/x86_64/
/base/lib/x86_64/libother.so
/base/lib/x86/
/base/lib/x86/libother.so
/BUNDLE-METADATA/com.android.tools.build.debugsymbols/
/BUNDLE-METADATA/com.android.tools.build.debugsymbols/arm64-v8a/
/BUNDLE-METADATA/com.android.tools.build.debugsymbols/arm64-v8a/libflutter.so.sym
/BUNDLE-METADATA/com.android.tools.build.debugsymbols/arm64-v8a/libapp.so.sym
/BUNDLE-METADATA/com.android.tools.build.debugsymbols/armeabi-v7a/
/BUNDLE-METADATA/com.android.tools.build.debugsymbols/armeabi-v7a/libflutter.so.sym
/BUNDLE-METADATA/com.android.tools.build.debugsymbols/armeabi-v7a/libapp.so.sym
''';

void main() {
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

  void createSharedGradleFiles() {
    fileSystem.directory('android').childFile('build.gradle').createSync(recursive: true);

    fileSystem.directory('android').childFile('gradle.properties').createSync(recursive: true);

    fileSystem.directory('android').childDirectory('app').childFile('build.gradle')
      ..createSync(recursive: true)
      ..writeAsStringSync('apply from: irrelevant/flutter.gradle');
  }

  File createAabFile(BuildMode buildMode) {
    final File aabFile = fileSystem
        .directory('/build')
        .childDirectory('app')
        .childDirectory('outputs')
        .childDirectory('bundle')
        .childDirectory('$buildMode')
        .childFile('app-$buildMode.aab');

    aabFile.createSync(recursive: true);
    return aabFile;
  }

  testUsingContext('warns when release App Bundle contains x86/x86_64 without libflutter.so '
      'causing app stuck on production splash screen (#73649)', () async {
    final builder = AndroidGradleBuilder(
      java: FakeJava(),
      logger: logger,
      processManager: processManager,
      fileSystem: fileSystem,
      artifacts: Artifacts.test(),
      analytics: fakeAnalytics,
      gradleUtils: FakeGradleUtils(),
      platform: FakePlatform(environment: <String, String>{'HOME': '/home'}),
      androidStudio: FakeAndroidStudio(),
    );

    final commandPortion = <String>[
      'gradlew',
      '-q',
      '-Ptarget-platform=android-arm64,android-arm,android-x64',
      '-Ptarget=lib/main.dart',
      '-Pbase-application-name=android.app.Application',
      '-Pdart-obfuscation=true',
      '-Psplit-debug-info=./',
      '-Ptrack-widget-creation=false',
      '-Ptree-shake-icons=false',
      'bundleRelease',
    ];
    processManager.addCommand(FakeCommand(command: commandPortion));

    createSharedGradleFiles();
    final File aabFile = createAabFile(BuildMode.release);
    final AndroidSdk sdk = AndroidSdk.locateAndroidSdk()!;

    processManager.addCommand(
      FakeCommand(
        command: <String>[
          sdk.getCmdlineToolsPath(apkAnalyzerBinaryName)!,
          'files',
          'list',
          aabFile.path,
        ],
        stdout: apkanalyzerOutputWithX86NoFlutter,
      ),
    );

    final FlutterProject project = FlutterProject.fromDirectoryTest(fileSystem.currentDirectory);
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
          dartObfuscation: true,
          splitDebugInfoPath: './',
        ),
        targetArchs: <AndroidArch>[
          AndroidArch.arm64_v8a,
          AndroidArch.armeabi_v7a,
          AndroidArch.x86_64,
        ],
      ),
      target: 'lib/main.dart',
      isBuildingBundle: true,
      configOnly: false,
      localGradleErrors: const <GradleHandledError>[],
    );

    expect(
      logger.warningText,
      contains('x86'),
      reason:
          'Should warn when release App Bundle contains x86/x86_64 native '
          'libraries without libflutter.so, which causes the app to crash '
          'or get stuck at splash screen on production (Play Store).',
    );
  }, overrides: <Type, Generator>{AndroidStudio: () => FakeAndroidStudio()});
}

class FakeGradleUtils extends Fake implements GradleUtils {
  @override
  String getExecutable(FlutterProject project) => 'gradlew';
}

class FakeAndroidStudio extends Fake implements AndroidStudio {
  @override
  String get javaPath => '/android-studio/jbr';
}
