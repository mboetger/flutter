// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_workflow.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' as gradle_utils;
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/base/version.dart';
import 'package:flutter_tools/src/doctor_validator.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fake_process_manager.dart';
import '../../src/fakes.dart';

void main() {
  late FakeAndroidSdk sdk;
  late Logger logger;
  late MemoryFileSystem fileSystem;
  late FakeProcessManager processManager;

  setUp(() {
    sdk = FakeAndroidSdk();
    fileSystem = MemoryFileSystem.test();
    fileSystem.directory('/home/me').createSync(recursive: true);
    logger = BufferLogger.test();
    processManager = FakeProcessManager.empty();
  });

  testUsingContext(
    'AndroidValidator warns when multiple adb binaries are found (reproduction)',
    () async {
      sdk
        ..licensesAvailable = true
        ..platformToolsAvailable = true
        ..cmdlineToolsAvailable = true
        ..directory = fileSystem.directory('/foo/bar')
        ..emulatorPath = 'path/to/emulator'
        ..latestVersion = (FakeAndroidSdkVersion()
          ..sdkLevel = gradle_utils.compileSdkVersionInt
          ..buildToolsVersion = gradle_utils.minBuildToolsVersion)
        ..adbPath = '/foo/bar/platform-tools/adb';

      processManager.excludedExecutables.add('path/to/emulator');

      final File adb1 = fileSystem.file('/foo/bar/platform-tools/adb')..createSync(recursive: true);
      final File adb2 = fileSystem.file('/usr/bin/adb')..createSync(recursive: true);

      final osUtils = ConflictFakeOperatingSystemUtils(<File>[adb1, adb2]);

      final ValidationResult validationResult = await AndroidValidator(
        java: FakeJava(),
        androidSdk: sdk,
        logger: logger,
        platform: FakePlatform(),
        userMessages: UserMessages(),
        processManager: processManager,
        osUtils: osUtils,
      ).validate();

      expect(
        validationResult.messages.any(
          (ValidationMessage message) =>
              message.type == ValidationMessageType.hint &&
              message.message.contains('Multiple adb binaries found') &&
              message.message.contains('/foo/bar/platform-tools/adb') &&
              message.message.contains('/usr/bin/adb'),
        ),
        true,
      );

      expect(processManager, hasNoRemainingExpectations);
    },
  );
}

class ConflictFakeOperatingSystemUtils extends FakeOperatingSystemUtils {
  ConflictFakeOperatingSystemUtils(this.adbPaths);
  final List<File> adbPaths;

  @override
  List<File> whichAll(String execName) => execName == 'adb' ? adbPaths : <File>[];
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  String? sdkManagerPath;

  @override
  String? sdkManagerVersion;

  @override
  String? adbPath;

  @override
  bool licensesAvailable = false;

  @override
  bool platformToolsAvailable = false;

  @override
  bool cmdlineToolsAvailable = false;

  @override
  Directory directory = MemoryFileSystem.test().directory('/foo/bar');

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
  String get buildToolsVersionName => '';

  @override
  String get platformName => '';
}
