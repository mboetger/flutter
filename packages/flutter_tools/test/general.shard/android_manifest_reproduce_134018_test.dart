// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/application_package.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/process.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/fake.dart';

import '../src/common.dart';
import '../src/context.dart';
import '../src/package_config.dart';

void main() {
  group('AndroidApk manifest parsing bug reproduction #134018', () {
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
      'successfully parses launch activity if it only exists in debug manifest',
      () async {
        final logger = BufferLogger.test();
        final FlutterProject project = await aProject(fs);

        // Create build.gradle files so isUsingGradle and isSupportedVersion return true
        fs
            .file(project.android.hostAppGradleRoot.childFile('build.gradle').path)
            .createSync(recursive: true);

        final File appBuildGradle = project.android.hostAppGradleRoot
            .childDirectory('app')
            .childFile('build.gradle');
        appBuildGradle.createSync(recursive: true);
        appBuildGradle.writeAsStringSync('''
plugins {
    id "dev.flutter.flutter-gradle-plugin"
}
''');

        // Create main AndroidManifest.xml WITHOUT launch activity
        // With isUsingGradle active, this correctly resolves to:
        // android/app/src/main/AndroidManifest.xml
        final File mainManifest = project.android.appManifestFile;
        mainManifest.createSync(recursive: true);
        mainManifest.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.my_app">
    <application android:label="my_app">
        <activity android:name=".MainActivity">
        </activity>
    </application>
</manifest>
''');

        // Create debug AndroidManifest.xml WITH launch activity
        final File debugManifest = project.android.hostAppGradleRoot
            .childDirectory('app')
            .childDirectory('src')
            .childDirectory('debug')
            .childFile('AndroidManifest.xml');
        debugManifest.createSync(recursive: true);
        debugManifest.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.my_app">
    <application android:label="my_app">
        <activity android:name=".MainActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
''');

        // Try to parse the package.
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

        // ASSERT THE CORRECT BEHAVIOR:
        // The tool should successfully find the launch activity in the debug manifest.
        // This will FAIL on the buggy codebase (reproducing the issue) and PASS once fixed.
        expect(androidApk, isNotNull);
        expect(androidApk!.id, 'com.example.my_app');
        expect(androidApk.launchActivity, 'com.example.my_app/.MainActivity');
        expect(
          logger.errorText,
          isNot(contains('package identifier or launch activity not found.')),
        );
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

Future<FlutterProject> aProject(FileSystem fs) async {
  final Directory directory = fs.directory('my_project');
  writePackageConfigFiles(directory: directory, mainLibName: 'my_app');
  directory.childFile('pubspec.yaml').writeAsStringSync('''
name: my_app
''');
  return FlutterProject.fromDirectory(directory);
}
