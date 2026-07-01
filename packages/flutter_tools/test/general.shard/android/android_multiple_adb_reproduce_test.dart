// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_workflow.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/doctor_validator.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';
import '../../src/fakes.dart';

void main() {
  late _FakeAndroidSdk sdk;
  late Logger logger;
  late MemoryFileSystem fileSystem;
  late FakeProcessManager processManager;

  setUp(() {
    sdk = _FakeAndroidSdk();
    fileSystem = MemoryFileSystem.test();
    fileSystem.directory('/home/me').createSync(recursive: true);
    logger = BufferLogger.test();
    processManager = FakeProcessManager.empty();
  });

  testWithoutContext(
    'AndroidValidator warns when multiple adb binaries are found (reproduction)',
    () async {
      sdk
        ..licensesAvailable = true
        ..platformToolsAvailable = true
        ..cmdlineToolsAvailable = true
        ..directory = fileSystem.directory('/foo/bar')
        ..adbPath = '/foo/bar/platform-tools/adb';

      final File adb1 = fileSystem.file('/foo/bar/platform-tools/adb')..createSync(recursive: true);
      final File adb2 = fileSystem.file('/usr/bin/adb')..createSync(recursive: true);

      final FakeOperatingSystemUtils osUtils = ConflictFakeOperatingSystemUtils(<File>[adb1, adb2]);

      final ValidationResult validationResult = await AndroidValidator(
        java: FakeJava(), // Using FakeJava from fakes.dart
        androidSdk: sdk,
        logger: logger,
        platform: FakePlatform(),
        userMessages: UserMessages(),
        processManager: processManager,
        osUtils: osUtils,
      ).validate();

      final bool hasWarning = validationResult.messages.any(
        (ValidationMessage message) =>
            message.type == ValidationMessageType.hint &&
            message.message.contains('Multiple adb binaries found') &&
            message.message.contains('/foo/bar/platform-tools/adb') &&
            message.message.contains('/usr/bin/adb'),
      );

      expect(
        hasWarning,
        isTrue,
        reason: 'Should issue a warning when multiple adb binaries are detected',
      );
    },
  );
}

class ConflictFakeOperatingSystemUtils extends FakeOperatingSystemUtils {
  ConflictFakeOperatingSystemUtils(this.adbPaths);
  final List<File> adbPaths;

  @override
  List<File> whichAll(String execName) {
    if (execName == 'adb') {
      return adbPaths;
    }
    return super.whichAll(execName);
  }
}

class _FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  bool licensesAvailable = false;

  @override
  bool platformToolsAvailable = false;

  @override
  bool cmdlineToolsAvailable = false;

  @override
  late Directory directory;

  @override
  String? adbPath;

  @override
  String? emulatorPath;

  @override
  AndroidSdkVersion? latestVersion;

  @override
  List<String> validateSdkWellFormed() => <String>[];
}
