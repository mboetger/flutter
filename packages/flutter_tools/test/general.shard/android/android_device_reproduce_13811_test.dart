// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_device.dart';
import 'package:flutter_tools/src/android/android_device_discovery.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_workflow.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/device.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';
import '../../src/fakes.dart' hide FakeAndroidSdk; // Hide to avoid shadowing conflict

void main() {
  late AndroidWorkflow androidWorkflow;

  setUp(() {
    androidWorkflow = AndroidWorkflow(
      androidSdk: FakeAndroidSdk(),
      featureFlags: TestFeatureFlags(),
    );
  });

  group('13811 Reproduction Tests', () {
    testWithoutContext('AndroidDevices discovers unauthorized devices and lists them', () async {
      final processManager = FakeProcessManager.list(<FakeCommand>[
        const FakeCommand(
          command: <String>['adb', 'devices', '-l'],
          stdout: '''
List of devices attached
????????!!!!       unauthorized usb:3-4
''',
        ),
      ]);

      final androidDevices = AndroidDevices(
        userMessages: UserMessages(),
        androidWorkflow: androidWorkflow,
        androidSdk: FakeAndroidSdk(),
        logger: BufferLogger.test(),
        processManager: processManager,
        platform: FakePlatform(),
        fileSystem: MemoryFileSystem.test(),
      );

      final List<Device> devices = await androidDevices.pollingGetDevices();

      // ASSERTION: The unauthorized device should be discovered so it can be listed.
      // This will FAIL on the current codebase because they are filtered out.
      expect(devices, hasLength(1));
      if (devices.isNotEmpty) {
        expect(devices.first.id, '????????!!!!');
      }
      expect(processManager, hasNoRemainingExpectations);
    });

    testWithoutContext('AndroidDevices discovers no permissions devices and lists them', () async {
      final processManager = FakeProcessManager.list(<FakeCommand>[
        const FakeCommand(
          command: <String>['adb', 'devices', '-l'],
          stdout: '''
List of devices attached
????????!!!!       no permissions usb:3-4
''',
        ),
      ]);

      final androidDevices = AndroidDevices(
        userMessages: UserMessages(),
        androidWorkflow: androidWorkflow,
        androidSdk: FakeAndroidSdk(),
        logger: BufferLogger.test(),
        processManager: processManager,
        platform: FakePlatform(),
        fileSystem: MemoryFileSystem.test(),
      );

      final List<Device> devices = await androidDevices.pollingGetDevices();

      // ASSERTION: The no-permissions device should be discovered so it can be listed.
      // This will FAIL on the current codebase because they are filtered out.
      expect(devices, hasLength(1));
      if (devices.isNotEmpty) {
        expect(devices.first.id, '????????!!!!');
      }
      expect(processManager, hasNoRemainingExpectations);
    });

    testWithoutContext(
      'AndroidDevice handles unauthorized/no permissions properties query gracefully',
      () async {
        final logger = BufferLogger.test();
        final processManager = FakeProcessManager.list(<FakeCommand>[
          // Mock adb getprop failing due to insufficient permissions / unauthorized
          const FakeCommand(
            command: <String>['adb', '-s', '????????!!!!', 'shell', 'getprop'],
            exitCode: 1,
            stderr: 'error: insufficient permissions for device\n',
          ),
        ]);

        final device = AndroidDevice(
          '????????!!!!',
          modelID: '????????!!!!',
          logger: logger,
          platform: FakePlatform(),
          androidSdk: FakeAndroidSdk(),
          fileSystem: MemoryFileSystem.test(),
          processManager: processManager,
        );

        // ASSERTIONS for graceful handling:
        // 1. Target platform should be unsupported instead of failing or throwing.
        expect(await device.targetPlatform, TargetPlatform.unsupported);

        // 2. SDK version should not contain null, maybe 'unknown' or 'unauthorized'.
        // Currently it returns 'Android null (API null)'.
        final String sdkVersion = await device.sdkNameAndVersion;
        expect(sdkVersion, isNot(contains('null')));

        // 3. It should NOT print raw "Error retrieving device properties..." to the logger.
        // This will FAIL on the current codebase because it prints the error.
        expect(logger.errorText, isEmpty);
        expect(processManager, hasNoRemainingExpectations);
      },
    );

    testWithoutContext(
      'AndroidDevice with known unauthorized state does not run getprop',
      () async {
        final logger = BufferLogger.test();
        final processManager = FakeProcessManager.list(
          <FakeCommand>[],
        ); // Assert NO commands are run

        final device = AndroidDevice(
          '????????!!!!',
          modelID: '????????!!!!',
          logger: logger,
          platform: FakePlatform(),
          androidSdk: FakeAndroidSdk(),
          fileSystem: MemoryFileSystem.test(),
          processManager: processManager,
          deviceState: 'unauthorized',
        );

        expect(await device.targetPlatform, TargetPlatform.unsupported);
        expect(await device.sdkNameAndVersion, 'Android unauthorized (API unknown)');
        expect(logger.errorText, isEmpty);
        expect(processManager, hasNoRemainingExpectations);
      },
    );
  });
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  FakeAndroidSdk([this.adbPath = 'adb']);

  @override
  final String? adbPath;
}
