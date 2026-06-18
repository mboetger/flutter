// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_workflow.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/time.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/base/version.dart';
import 'package:flutter_tools/src/doctor_validator.dart';
import 'package:flutter_tools/src/persistent_tool_state.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fakes.dart' hide FakeAndroidSdk;

void main() {
  late FakeAndroidSdk sdk;
  late Logger logger;
  late MemoryFileSystem fileSystem;
  late FakeProcessManager processManager;
  late PersistentToolState persistentToolState;

  setUp(() {
    sdk = FakeAndroidSdk();
    fileSystem = MemoryFileSystem.test();
    fileSystem.directory('/home/me').createSync(recursive: true);
    logger = BufferLogger.test();
    processManager = FakeProcessManager.empty();
    persistentToolState = PersistentToolState.test(
      directory: fileSystem.directory('/home/me'),
      logger: logger,
    );
  });

  testUsingContext(
    'AndroidValidator warns when updates to the Android SDK are available',
    () async {
      final sdkVersion = FakeAndroidSdkVersion()
        ..sdkLevel = 36
        ..buildToolsVersion = Version(36, 0, 0);

      sdk
        ..licensesAvailable = true
        ..platformToolsAvailable = true
        ..cmdlineToolsAvailable = true
        ..directory = fileSystem.directory('/foo/bar')
        ..sdkManagerPath = '/foo/bar/cmdline-tools/latest/bin/sdkmanager'
        ..latestVersion = sdkVersion
        ..emulatorPath = 'path/to/emulator';

      processManager.addCommand(
        const FakeCommand(
          command: <String>['/foo/bar/cmdline-tools/latest/bin/sdkmanager', '--list'],
          stdout: '''
Installed packages:
  Path    | Version | Description | Location
  ------- | ------- | ----------- | --------
  patcher;v4 | 1      | Patcher     | patcher/v4

Available Updates:
  ID      | Installed | Available
  ------- | --------- | ---------
  patcher;v4 | 1         | 2        
''',
        ),
      );

      final androidValidator = AndroidValidator(
        java: FakeJava(),
        androidSdk: sdk,
        logger: logger,
        platform: FakePlatform()..environment = <String, String>{'HOME': '/home/me'},
        userMessages: UserMessages(),
        processManager: processManager,
        osUtils: FakeOperatingSystemUtils(),
        persistentToolState: persistentToolState,
      );

      final ValidationResult validationResult = await androidValidator.validate();

      final bool hasUpdateWarning = validationResult.messages.any(
        (ValidationMessage message) =>
            message.type == ValidationMessageType.hint &&
            message.message.contains('update to the Android SDK is available') &&
            message.message.contains(
              'https://developer.android.com/studio/intro/update.html#sdk-manager',
            ),
      );

      expect(
        hasUpdateWarning,
        isTrue,
        reason: 'Should notify user about available Android SDK updates.',
      );
    },
  );

  testUsingContext(
    'AndroidValidator updates cache timestamp and respects 24h throttling',
    () async {
      final sdkVersion = FakeAndroidSdkVersion()
        ..sdkLevel = 36
        ..buildToolsVersion = Version(36, 0, 0);

      sdk
        ..licensesAvailable = true
        ..platformToolsAvailable = true
        ..cmdlineToolsAvailable = true
        ..directory = fileSystem.directory('/foo/bar')
        ..sdkManagerPath = '/foo/bar/cmdline-tools/latest/bin/sdkmanager'
        ..latestVersion = sdkVersion
        ..emulatorPath = 'path/to/emulator';

      // First run: sdkmanager is called.
      processManager.addCommand(
        const FakeCommand(
          command: <String>['/foo/bar/cmdline-tools/latest/bin/sdkmanager', '--list'],
          stdout: 'Available Updates:\npatcher;v4 | 1 | 2',
        ),
      );

      final initialTime = DateTime(2026, 1, 1, 12);
      final clock = SystemClock.fixed(initialTime);

      final androidValidator1 = AndroidValidator(
        java: FakeJava(),
        androidSdk: sdk,
        logger: logger,
        platform: FakePlatform()..environment = <String, String>{'HOME': '/home/me'},
        userMessages: UserMessages(),
        processManager: processManager,
        osUtils: FakeOperatingSystemUtils(),
        persistentToolState: persistentToolState,
        clock: clock,
      );

      expect(persistentToolState.lastAndroidSdkCheckTime, isNull);

      final ValidationResult result1 = await androidValidator1.validate();
      expect(persistentToolState.lastAndroidSdkCheckTime, initialTime);
      expect(
        result1.messages.any(
          (ValidationMessage m) => m.message.contains('update to the Android SDK is available'),
        ),
        isTrue,
      );

      // Second run: 12 hours later (less than 24h). sdkmanager should NOT be called.
      // If it tries to run, processManager will throw because there is no command queued.
      final clockAfter12h = SystemClock.fixed(initialTime.add(const Duration(hours: 12)));
      final androidValidator2 = AndroidValidator(
        java: FakeJava(),
        androidSdk: sdk,
        logger: logger,
        platform: FakePlatform()..environment = <String, String>{'HOME': '/home/me'},
        userMessages: UserMessages(),
        processManager: processManager,
        osUtils: FakeOperatingSystemUtils(),
        persistentToolState: persistentToolState,
        clock: clockAfter12h,
      );

      final ValidationResult result2 = await androidValidator2.validate();
      expect(persistentToolState.lastAndroidSdkCheckTime, initialTime); // check time unchanged
      expect(
        result2.messages.any(
          (ValidationMessage m) => m.message.contains('update to the Android SDK is available'),
        ),
        isFalse,
      );

      // Third run: 25 hours later (more than 24h). sdkmanager should be called again.
      processManager.addCommand(
        const FakeCommand(
          command: <String>['/foo/bar/cmdline-tools/latest/bin/sdkmanager', '--list'],
          stdout: 'No updates available',
        ),
      );

      final DateTime checkTimeAfter25h = initialTime.add(const Duration(hours: 25));
      final clockAfter25h = SystemClock.fixed(checkTimeAfter25h);
      final androidValidator3 = AndroidValidator(
        java: FakeJava(),
        androidSdk: sdk,
        logger: logger,
        platform: FakePlatform()..environment = <String, String>{'HOME': '/home/me'},
        userMessages: UserMessages(),
        processManager: processManager,
        osUtils: FakeOperatingSystemUtils(),
        persistentToolState: persistentToolState,
        clock: clockAfter25h,
      );

      final ValidationResult result3 = await androidValidator3.validate();
      expect(persistentToolState.lastAndroidSdkCheckTime, checkTimeAfter25h); // check time updated
      expect(
        result3.messages.any(
          (ValidationMessage m) => m.message.contains('update to the Android SDK is available'),
        ),
        isFalse,
      );
      expect(processManager.hasRemainingExpectations, isFalse);
    },
  );
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  String? sdkManagerPath;

  @override
  String? sdkManagerVersion;

  @override
  String? adbPath = 'path/to/adb';

  @override
  bool licensesAvailable = false;

  @override
  bool platformToolsAvailable = false;

  @override
  bool cmdlineToolsAvailable = false;

  @override
  late Directory directory;

  @override
  AndroidSdkVersion? latestVersion;

  @override
  String? emulatorPath;

  @override
  List<String> validateSdkWellFormed() => <String>[];
}

class FakeAndroidSdkVersion extends Fake implements AndroidSdkVersion {
  @override
  int sdkLevel = 0;

  @override
  Version buildToolsVersion = Version(0, 0, 0);

  @override
  String get buildToolsVersionName => buildToolsVersion.toString();

  @override
  String get platformName => 'android-$sdkLevel';
}
