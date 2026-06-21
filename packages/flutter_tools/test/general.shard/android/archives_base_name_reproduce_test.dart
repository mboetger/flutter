// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/application_package.dart';
import 'package:flutter_tools/src/application_package.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/os.dart';
import 'package:flutter_tools/src/base/process.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/globals.dart' as globals;
import 'package:flutter_tools/src/project.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fake_process_manager.dart';
import '../../src/fakes.dart';
import '../../src/package_config.dart';

void main() {
  group('AndroidApk.fromAndroidProject respects custom archivesBaseName', () {
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

    testUsingContext('finds APK when archivesBaseName is customized to "custom_name"', () async {
      const String aaptPath = 'aaptPath';
      final sdkVersion = FakeAndroidSdkVersion();
      sdkVersion.aaptPath = aaptPath;
      sdk.latestVersion = sdkVersion;
      sdk.platformToolsAvailable = true;

      // 1. Set up a standard project.
      final Directory directory = fs.directory('my_app');
      writePackageConfigFiles(directory: directory, mainLibName: 'my_app');
      directory.childFile('pubspec.yaml').writeAsStringSync('name: my_app');
      
      final Directory androidDir = directory.childDirectory('android');
      androidDir.childFile('build.gradle').createSync(recursive: true);
      
      final Directory appDir = androidDir.childDirectory('app');
      final File appGradle = appDir.childFile('build.gradle');
      appGradle.createSync(recursive: true);
      
      // Customize archivesBaseName in the gradle file
      appGradle.writeAsStringSync('''
def flutterPluginVersion = 'managed'
android {
    defaultConfig {
        project.ext.set("archivesBaseName", "custom_name")
    }
}
''');
      
      final File manifestFile = appDir.childDirectory('src').childDirectory('main').childFile('AndroidManifest.xml');
      manifestFile.createSync(recursive: true);
      manifestFile.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.my_app">
    <application android:label="my_app">
        <activity android:name=".MainActivity" android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>
    </application>
</manifest>
''');

      final FlutterProject project = FlutterProject.fromDirectory(directory);
      
      // 2. Create the custom named APK that Gradle would produce
      final File customApkFile = project.directory
          .childDirectory('build')
          .childDirectory('app')
          .childDirectory('outputs')
          .childDirectory('flutter-apk')
          .childFile('custom_name-release.apk');
      customApkFile.createSync(recursive: true);

      // We expect aapt to be run on the custom APK if it is found
      fakeProcessManager.addCommand(
        FakeCommand(
          command: <String>[aaptPath, 'dump', 'xmltree', customApkFile.path, 'AndroidManifest.xml'],
          stdout: _aaptDataWithDefaultEnabledAndMainLauncherActivity,
        ),
      );

      final AndroidApk? androidApk = await AndroidApk.fromAndroidProject(
        project.android,
        androidSdk: sdk,
        processManager: fakeProcessManager,
        userMessages: UserMessages(),
        processUtils: ProcessUtils(processManager: fakeProcessManager, logger: BufferLogger.test()),
        logger: BufferLogger.test(),
        fileSystem: fs,
        buildInfo: const BuildInfo(
          BuildMode.release,
          null,
          treeShakeIcons: false,
          packageConfigPath: '.dart_tool/package_config.json',
        ),
      );

      // This assertion will FAIL on the current codebase because the tool looks for
      // 'app-release.apk' instead of 'custom_name-release.apk'.
      expect(androidApk, isNotNull);
      expect(androidApk!.applicationPackage.basename, 'custom_name-release.apk');
    }, overrides: overrides);

    testUsingContext('finds APK when base.archivesName is customized to "custom_name"', () async {
      const String aaptPath = 'aaptPath';
      final sdkVersion = FakeAndroidSdkVersion();
      sdkVersion.aaptPath = aaptPath;
      sdk.latestVersion = sdkVersion;
      sdk.platformToolsAvailable = true;

      // 1. Set up a standard project.
      final Directory directory = fs.directory('my_app');
      writePackageConfigFiles(directory: directory, mainLibName: 'my_app');
      directory.childFile('pubspec.yaml').writeAsStringSync('name: my_app');
      
      final Directory androidDir = directory.childDirectory('android');
      androidDir.childFile('build.gradle').createSync(recursive: true);
      
      final Directory appDir = androidDir.childDirectory('app');
      final File appGradle = appDir.childFile('build.gradle');
      appGradle.createSync(recursive: true);
      
      // Customize base.archivesName (modern Kotlin DSL style) in the gradle file
      appGradle.writeAsStringSync('''
def flutterPluginVersion = 'managed'
android {
    base {
        archivesName.set("custom_name")
    }
}
''');
      
      final File manifestFile = appDir.childDirectory('src').childDirectory('main').childFile('AndroidManifest.xml');
      manifestFile.createSync(recursive: true);
      manifestFile.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.my_app">
    <application android:label="my_app">
        <activity android:name=".MainActivity" android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>
    </application>
</manifest>
''');

      final FlutterProject project = FlutterProject.fromDirectory(directory);
      
      // 2. Create the custom named APK that Gradle would produce
      final File customApkFile = project.directory
          .childDirectory('build')
          .childDirectory('app')
          .childDirectory('outputs')
          .childDirectory('flutter-apk')
          .childFile('custom_name-release.apk');
      customApkFile.createSync(recursive: true);

      // We expect aapt to be run on the custom APK if it is found
      fakeProcessManager.addCommand(
        FakeCommand(
          command: <String>[aaptPath, 'dump', 'xmltree', customApkFile.path, 'AndroidManifest.xml'],
          stdout: _aaptDataWithDefaultEnabledAndMainLauncherActivity,
        ),
      );

      final AndroidApk? androidApk = await AndroidApk.fromAndroidProject(
        project.android,
        androidSdk: sdk,
        processManager: fakeProcessManager,
        userMessages: UserMessages(),
        processUtils: ProcessUtils(processManager: fakeProcessManager, logger: BufferLogger.test()),
        logger: BufferLogger.test(),
        fileSystem: fs,
        buildInfo: const BuildInfo(
          BuildMode.release,
          null,
          treeShakeIcons: false,
          packageConfigPath: '.dart_tool/package_config.json',
        ),
      );

      // This assertion will FAIL on the current codebase because the tool looks for
      // 'app-release.apk' instead of 'custom_name-release.apk'.
      expect(androidApk, isNotNull);
      expect(androidApk!.applicationPackage.basename, 'custom_name-release.apk');
    }, overrides: overrides);
  });
}

const String _aaptDataWithDefaultEnabledAndMainLauncherActivity = '''
N: android=http://schemas.android.com/apk/res/android
  E: manifest (line=2)
    A: android:versionCode(0x0101021b)=(type 0x10)0x1
    A: android:versionName(0x0101021c)="1.0.0" (Raw: "1.0.0")
    A: package="com.example.my_app" (Raw: "com.example.my_app")
    E: application (line=3)
      A: android:label(0x01010001)="my_app" (Raw: "my_app")
      A: android:name(0x01010003)="io.flutter.app.FlutterApplication" (Raw: "io.flutter.app.FlutterApplication")
      E: activity (line=4)
        A: android:name(0x01010003)="com.example.my_app.MainActivity" (Raw: "com.example.my_app.MainActivity")
        E: intent-filter (line=5)
          E: action (line=6)
            A: android:name(0x01010003)="android.intent.action.MAIN" (Raw: "android.intent.action.MAIN")
          E: category (line=7)
            A: android:name(0x01010003)="android.intent.category.LAUNCHER" (Raw: "android.intent.category.LAUNCHER")
''';

class FakeAndroidSdkVersion extends Fake implements AndroidSdkVersion {
  @override
  late String aaptPath;
}
