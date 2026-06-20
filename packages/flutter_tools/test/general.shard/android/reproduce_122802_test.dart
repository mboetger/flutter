// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/application_package.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/process.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/fake.dart';
import 'package:unified_analytics/unified_analytics.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/package_config.dart';

void main() {
  group('Reproduction of 122802 - Unified APK paths for Flutter Modules', () {
    late FakeAndroidSdk sdk;
    late FakeProcessManager fakeProcessManager;
    late MemoryFileSystem fs;
    late Cache cache;

    final overrides = <Type, Generator>{
      AndroidSdk: () => sdk,
      ProcessManager: () => fakeProcessManager,
      FileSystem: () => fs,
      Cache: () => cache,
    };

    setUp(() async {
      sdk = FakeAndroidSdk();
      fakeProcessManager = FakeProcessManager.empty();
      fs = MemoryFileSystem.test();
      cache = Cache.test(processManager: FakeProcessManager.any());
      Cache.flutterRoot = '../..';
      sdk.licensesAvailable = true;
    });

    testUsingContext(
      'resolved module APK path is flattened (no build mode subdirectory)',
      () async {
        const aaptPath = 'aaptPath';
        final sdkVersion = FakeAndroidSdkVersion();
        sdkVersion.aaptPath = aaptPath;
        sdk.latestVersion = sdkVersion;
        sdk.platformToolsAvailable = true;
        sdk.licensesAvailable = false;

        final logger = BufferLogger.test();
        final FlutterProject project = await aModuleProject(fs);
        project.android.hostAppGradleRoot.childFile('build.gradle').createSync(recursive: true);
        final File appGradle = project.android.hostAppGradleRoot.childFile(
          fs.path.join('app', 'build.gradle'),
        );
        appGradle.createSync(recursive: true);
        appGradle.writeAsStringSync("def flutterPluginVersion = 'managed'");

        // Create the APK in the flattened directory (no 'debug' subdirectory)
        final File apkDebugFile = project.directory
            .childDirectory('build')
            .childDirectory('host')
            .childDirectory('outputs')
            .childDirectory('apk')
            .childFile('app-debug.apk');
        apkDebugFile.createSync(recursive: true);

        // Mock the aapt command for the APK inside the module project.
        // If the tool correctly looks in the flattened directory, this command will be executed.
        fakeProcessManager.addCommand(
          FakeCommand(
            command: <String>[
              aaptPath,
              'dump',
              'xmltree',
              apkDebugFile.path,
              'AndroidManifest.xml',
            ],
            stdout: _aaptDataWithDefaultEnabledAndMainLauncherActivity,
          ),
        );

        final AndroidApk? androidApk = await AndroidApk.fromAndroidProject(
          project.android,
          androidSdk: sdk,
          processManager: fakeProcessManager,
          userMessages: UserMessages(),
          processUtils: ProcessUtils(processManager: fakeProcessManager, logger: logger),
          logger: logger,
          fileSystem: fs,
          buildInfo: const BuildInfo(
            BuildMode.debug,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
        );

        expect(androidApk, isNotNull);
        expect(androidApk!.applicationPackage.path, apkDebugFile.path);
      },
      overrides: overrides,
    );

    testUsingContext(
      'findApkFilesModule looks for APK in flattened directory (no build mode subdirectory)',
      () async {
        final FlutterProject project = await aModuleProject(fs);

        // Create the APK in the flattened directory
        final File apkFile = project.directory
            .childDirectory('build')
            .childDirectory('host')
            .childDirectory('outputs')
            .childDirectory('apk')
            .childFile('app-debug.apk');
        apkFile.createSync(recursive: true);

        final Iterable<String> apks = findApkFilesModule(
          project,
          const AndroidBuildInfo(
            BuildInfo(
              BuildMode.debug,
              null,
              treeShakeIcons: false,
              packageConfigPath: '.dart_tool/package_config.json',
            ),
          ),
          BufferLogger.test(),
          FakeAnalytics(),
        );

        expect(apks.first, apkFile.path);
      },
      overrides: overrides,
    );
  });
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  late bool platformToolsAvailable;

  @override
  late bool licensesAvailable;

  @override
  AndroidSdkVersion? latestVersion;
}

class FakeAndroidSdkVersion extends Fake implements AndroidSdkVersion {
  @override
  late String aaptPath;
}

class FakeAnalytics extends Fake implements Analytics {}

Future<FlutterProject> aModuleProject(FileSystem fs) async {
  final Directory directory = fs.directory('module_project');
  writePackageConfigFiles(directory: directory, mainLibName: 'my_app');
  directory.childFile('pubspec.yaml').writeAsStringSync('''
name: my_module
flutter:
  module:
    androidPackage: com.example
''');
  return FlutterProject.fromDirectory(directory);
}

const String _aaptDataWithDefaultEnabledAndMainLauncherActivity = '''
N: android=http://schemas.android.com/apk/res/android
  E: manifest (line=2)
    A: android:versionCode(0x0101021b)=(type 0x10)0x1
    A: android:versionName(0x0101021c)="1.0"
    A: package="com.twitter.sdk.android.tweetcomposer.test" (Raw: "com.twitter.sdk.android.tweetcomposer.test")
    E: uses-sdk (line=3)
      A: android:minSdkVersion(0x01010272)=(type 0x10)0x9
      A: android:targetSdkVersion(0x01010270)=(type 0x10)0x16
    E: application (line=4)
      A: android:label(0x01010001)="package_or_activity" (Raw: "package_or_activity")
      E: activity (line=5)
        A: android:theme(0x01010000)=@0x7f050005
        A: android:name(0x01010003)="com.twitter.sdk.android.tweetcomposer.ComposerActivity" (Raw: "com.twitter.sdk.android.tweetcomposer.ComposerActivity")
        A: android:enabled(0x0101000e)=(type 0x12)0xffffffff
        E: intent-filter (line=6)
          E: action (line=7)
            A: android:name(0x01010003)="android.intent.action.MAIN" (Raw: "android.intent.action.MAIN")
          E: category (line=8)
            A: android:name(0x01010003)="android.intent.category.LAUNCHER" (Raw: "android.intent.category.LAUNCHER")
''';
