// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/application_package.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/process.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:unified_analytics/unified_analytics.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fakes.dart';

void main() {
  late MemoryFileSystem fileSystem;
  late BufferLogger logger;
  late FakeAndroidSdk sdk;
  late FakeProcessManager fakeProcessManager;
  late FakeAnalytics fakeAnalytics;

  setUp(() {
    fileSystem = MemoryFileSystem.test();
    logger = BufferLogger.test();
    sdk = FakeAndroidSdk();
    fakeProcessManager = FakeProcessManager.empty();
    fakeAnalytics = getInitializedFakeAnalyticsInstance(
      fs: fileSystem,
      fakeFlutterVersion: FakeFlutterVersion(),
    );
  });

  group('archivesBaseName / base.archivesName reproduction', () {
    testUsingContext(
      'AndroidApk.fromAndroidProject finds the custom built APK when archivesBaseName is customized',
      () async {
        final FlutterProject project = FlutterProject.fromDirectoryTest(
          fileSystem.currentDirectory,
        );

        // Customize build.gradle with a custom archivesBaseName (legacy syntax).
        project.android.hostAppGradleFile.createSync(recursive: true);
        final File buildGradle = project.android.appGradleFile;
        buildGradle.createSync(recursive: true);
        buildGradle.writeAsStringSync('''
        android {
            defaultConfig {
                project.archivesBaseName = 'custom-legacy-name'
            }
        }
      ''');

        // Create a valid AndroidManifest.xml with a launchable activity so it doesn't fail parsing.
        final File manifest = project.android.appManifestFile;
        manifest.createSync(recursive: true);
        manifest.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.example.test">
  <application>
    <activity android:name=".MainActivity">
      <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
      </intent-filter>
    </activity>
  </application>
</manifest>
''');

        // Simulate a successful gradle build that outputs the custom named APK.
        final Directory apkDir = getApkDirectory(project);
        final File customApkFile = apkDir.childFile('custom-legacy-name-release.apk');
        customApkFile.createSync(recursive: true);

        final AndroidApk? androidApk = await AndroidApk.fromAndroidProject(
          project.android,
          androidSdk: sdk,
          processManager: fakeProcessManager,
          userMessages: UserMessages(),
          processUtils: ProcessUtils(processManager: fakeProcessManager, logger: logger),
          logger: logger,
          fileSystem: fileSystem,
          buildInfo: const BuildInfo(
            BuildMode.release,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
        );

        expect(androidApk, isNotNull);
        // SHOULD find the custom named APK instead of falling back to app-release.apk.
        // This is expected to FAIL on the current codebase because the tool does not read the custom name
        // and thus falls back to 'app-release.apk'.
        expect(androidApk!.applicationPackage.path, contains('custom-legacy-name-release.apk'));
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => fakeProcessManager,
      },
    );

    testUsingContext(
      'AndroidApk.fromAndroidProject finds the custom built APK when base.archivesName is customized',
      () async {
        final FlutterProject project = FlutterProject.fromDirectoryTest(
          fileSystem.currentDirectory,
        );

        // Customize build.gradle with modern Gradle base.archivesName syntax.
        project.android.hostAppGradleFile.createSync(recursive: true);
        final File buildGradle = project.android.appGradleFile;
        buildGradle.createSync(recursive: true);
        buildGradle.writeAsStringSync('''
        android {
            defaultConfig {
                base.archivesName = 'custom-modern-name'
            }
        }
      ''');

        // Create a valid AndroidManifest.xml with a launchable activity.
        final File manifest = project.android.appManifestFile;
        manifest.createSync(recursive: true);
        manifest.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.example.test">
  <application>
    <activity android:name=".MainActivity">
      <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
      </intent-filter>
    </activity>
  </application>
</manifest>
''');

        // Simulate a successful gradle build that outputs the custom named APK.
        final Directory apkDir = getApkDirectory(project);
        final File customApkFile = apkDir.childFile('custom-modern-name-release.apk');
        customApkFile.createSync(recursive: true);

        final AndroidApk? androidApk = await AndroidApk.fromAndroidProject(
          project.android,
          androidSdk: sdk,
          processManager: fakeProcessManager,
          userMessages: UserMessages(),
          processUtils: ProcessUtils(processManager: fakeProcessManager, logger: logger),
          logger: logger,
          fileSystem: fileSystem,
          buildInfo: const BuildInfo(
            BuildMode.release,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
        );

        expect(androidApk, isNotNull);
        // SHOULD find the custom named APK instead of falling back to app-release.apk.
        // This is expected to FAIL on the current codebase.
        expect(androidApk!.applicationPackage.path, contains('custom-modern-name-release.apk'));
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => fakeProcessManager,
      },
    );

    testWithoutContext(
      'findBundleFile respects custom archivesBaseName and ignores stale default AABs',
      () {
        final FlutterProject project = FlutterProject.fromDirectoryTest(
          fileSystem.currentDirectory,
        );

        // Customize build.gradle with a custom archivesBaseName.
        project.android.hostAppGradleFile.createSync(recursive: true);
        final File buildGradle = project.android.appGradleFile;
        buildGradle.createSync(recursive: true);
        buildGradle.writeAsStringSync('''
        android {
            defaultConfig {
                project.archivesBaseName = 'my-custom-output'
            }
        }
      ''');

        final Directory bundleDir = getBundleDirectory(project);
        final Directory releaseBundleDir = bundleDir.childDirectory('release');
        releaseBundleDir.createSync(recursive: true);

        // Create a stale default AAB and the new custom AAB.
        final File staleAab = releaseBundleDir.childFile('app-release.aab');
        staleAab.createSync();
        final File customAab = releaseBundleDir.childFile('my-custom-output-release.aab');
        customAab.createSync();

        final File bundle = findBundleFile(
          project,
          const BuildInfo(
            BuildMode.release,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
          logger,
          fakeAnalytics,
        );

        // SHOULD return the custom AAB, not the stale default one.
        // This is expected to FAIL on the current codebase because the tool does not read the custom name
        // and thus matches 'app-release.aab' since it appears first or matches the default pattern.
        expect(bundle.path, contains('my-custom-output-release.aab'));
      },
    );
  });
}
