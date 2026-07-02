// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter_tools/src/android/android_emulator.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/base/common.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

const String emulatorID = 'i1234';
const List<String> kEmulatorLaunchCommand = <String>['emulator', '-avd', emulatorID];

void main() {
  group('reproduce_48894', () {
    late FakeAndroidSdk mockSdk;

    setUp(() {
      mockSdk = FakeAndroidSdk();
      mockSdk.emulatorPath = 'emulator';
    });

    testWithoutContext('throws ToolExit on AVD lock error', () async {
      final logger = BufferLogger.test();
      final emulator = AndroidEmulator(
        emulatorID,
        processManager: FakeProcessManager.list(<FakeCommand>[
          const FakeCommand(
            command: kEmulatorLaunchCommand,
            exitCode: 1,
            stderr:
                'emulator: ERROR: Running multiple emulators with the same AVD is an experimental feature.\n'
                'Please use -read-only flag to enable this feature.',
          ),
        ]),
        androidSdk: mockSdk,
        logger: logger,
      );

      await expectLater(
        () => emulator.launch(startupDuration: Duration.zero),
        throwsA(isA<ToolExit>()),
      );
    });
  });
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  String? emulatorPath;
}
