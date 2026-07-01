// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_device_discovery.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_workflow.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';
import '../../src/fakes.dart';

void main() {
  testWithoutContext(
    'AndroidDevices returns diagnostic message when Android SDK is null',
    () async {
      final androidDevices = AndroidDevices(
        logger: BufferLogger.test(),
        androidWorkflow: AndroidWorkflow(
          androidSdk: null,
          featureFlags: TestFeatureFlags(),
        ),
        processManager: FakeProcessManager.empty(),
        fileSystem: MemoryFileSystem.test(),
        platform: FakePlatform(),
        userMessages: UserMessages(),
      );

      final List<String> diagnostics = await androidDevices.getDiagnostics();
      expect(diagnostics, isNotEmpty);
      expect(diagnostics.first, contains('Unable to locate Android SDK'));
    },
  );

  testWithoutContext(
    'AndroidDevices returns diagnostic message when adb is null',
    () async {
      final androidDevices = AndroidDevices(
        androidSdk: FakeAndroidSdk(null),
        logger: BufferLogger.test(),
        androidWorkflow: AndroidWorkflow(
          androidSdk: FakeAndroidSdk(null),
          featureFlags: TestFeatureFlags(),
        ),
        processManager: FakeProcessManager.empty(),
        fileSystem: MemoryFileSystem.test(),
        platform: FakePlatform(),
        userMessages: UserMessages(),
      );

      final List<String> diagnostics = await androidDevices.getDiagnostics();
      expect(diagnostics, isNotEmpty);
      expect(diagnostics.first, contains('Android SDK is missing the adb tool'));
    },
  );

  testWithoutContext(
    'AndroidDevices returns diagnostic message when adb cannot be run',
    () async {
      final fakeProcessManager = FakeProcessManager.empty();
      fakeProcessManager.excludedExecutables.add('adb');
      final androidDevices = AndroidDevices(
        androidSdk: FakeAndroidSdk(),
        logger: BufferLogger.test(),
        androidWorkflow: AndroidWorkflow(
          androidSdk: FakeAndroidSdk(),
          featureFlags: TestFeatureFlags(),
        ),
        processManager: fakeProcessManager,
        fileSystem: MemoryFileSystem.test(),
        platform: FakePlatform(),
        userMessages: UserMessages(),
      );

      final List<String> diagnostics = await androidDevices.getDiagnostics();
      expect(diagnostics, isNotEmpty);
      expect(diagnostics.first, contains('Unable to run "adb"'));
    },
  );
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  FakeAndroidSdk([this.adbPath = 'adb']);

  @override
  final String? adbPath;
}
