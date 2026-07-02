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
const String errorText = '[Android emulator test error]';
const List<String> kEmulatorLaunchCommand = <String>['emulator', '-avd', emulatorID];

void main() {
  group('Android emulator launch reproduction 47259', () {
    late FakeAndroidSdk mockSdk;

    setUp(() {
      mockSdk = FakeAndroidSdk();
      mockSdk.emulatorPath = 'emulator';
    });

    testWithoutContext(
      'throws ToolExit when emulator exits with non-zero code during startup',
      () async {
        final logger = BufferLogger.test();
        final emulator = AndroidEmulator(
          emulatorID,
          processManager: FakeProcessManager.list(<FakeCommand>[
            const FakeCommand(
              command: kEmulatorLaunchCommand,
              exitCode: 1,
              stderr: errorText,
              stdout: 'dummy text',
            ),
          ]),
          androidSdk: mockSdk,
          logger: logger,
        );

        await expectLater(
          () => emulator.launch(startupDuration: Duration.zero),
          throwsA(isA<ToolExit>()),
        );

        expect(logger.errorText, contains(errorText));
      },
    );
  });
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  String? emulatorPath;
}
