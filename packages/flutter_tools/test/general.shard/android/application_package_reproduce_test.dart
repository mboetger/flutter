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
import 'package:flutter_tools/src/base/process.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/globals.dart' as globals;
import 'package:flutter_tools/src/project.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fake_process_manager.dart';
import '../../src/fakes.dart';

void main() {
  group('AndroidApk activity-alias reproduction test', () {
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
      final FlutterProject project = FlutterProject.fromDirectoryTest(fs.currentDirectory);
      fs
          .file(
            project.android.hostAppGradleRoot
                .childFile(globals.platform.isWindows ? 'gradlew.bat' : 'gradlew')
                .path,
          )
          .createSync(recursive: true);
    });

    testUsingContext('picks up launch activity from activity-alias', () async {
      final logger = BufferLogger.test();
      final FlutterProject project = FlutterProject.fromDirectoryTest(fs.currentDirectory);
      project.android.hostAppGradleRoot.childFile('build.gradle').createSync(recursive: true);
      final File appGradle = project.android.hostAppGradleRoot.childFile(
        fs.path.join('app', 'build.gradle'),
      );
      appGradle.createSync(recursive: true);
      appGradle.writeAsStringSync("def flutterPluginVersion = 'managed'");

      final File sourceManifest = project.android.appManifestFile;
      sourceManifest.createSync(recursive: true);
      sourceManifest.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="io.flutter.examples.hello_world">
    <application android:label="hello_world">
        <activity android:name=".MainActivity" android:enabled="true">
        </activity>
        <activity-alias android:name=".MainActivityAlias" android:targetActivity=".MainActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity-alias>
    </application>
</manifest>
''');

      final AndroidApk? androidApk = await AndroidApk.fromAndroidProject(
        project.android,
        androidSdk: sdk,
        processManager: fakeProcessManager,
        userMessages: UserMessages(),
        processUtils: ProcessUtils(processManager: fakeProcessManager, logger: logger),
        logger: logger,
        fileSystem: fs,
      );

      expect(androidApk, isNotNull);
      expect(androidApk!.id, 'io.flutter.examples.hello_world');
      expect(androidApk.launchActivity, 'io.flutter.examples.hello_world/.MainActivityAlias');
    }, overrides: overrides);

    testUsingContext('does not pick up disabled activity-alias', () async {
      final logger = BufferLogger.test();
      final FlutterProject project = FlutterProject.fromDirectoryTest(fs.currentDirectory);
      project.android.hostAppGradleRoot.childFile('build.gradle').createSync(recursive: true);
      final File appGradle = project.android.hostAppGradleRoot.childFile(
        fs.path.join('app', 'build.gradle'),
      );
      appGradle.createSync(recursive: true);
      appGradle.writeAsStringSync("def flutterPluginVersion = 'managed'");

      final File sourceManifest = project.android.appManifestFile;
      sourceManifest.createSync(recursive: true);
      sourceManifest.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="io.flutter.examples.hello_world">
    <application android:label="hello_world">
        <activity android:name=".MainActivity" android:enabled="true">
        </activity>
        <activity-alias android:name=".MainActivityAlias" android:targetActivity=".MainActivity" android:enabled="false">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity-alias>
    </application>
</manifest>
''');

      final AndroidApk? androidApk = await AndroidApk.fromAndroidProject(
        project.android,
        androidSdk: sdk,
        processManager: fakeProcessManager,
        userMessages: UserMessages(),
        processUtils: ProcessUtils(processManager: fakeProcessManager, logger: logger),
        logger: logger,
        fileSystem: fs,
      );

      expect(androidApk, isNull);
    }, overrides: overrides);
  });
}
