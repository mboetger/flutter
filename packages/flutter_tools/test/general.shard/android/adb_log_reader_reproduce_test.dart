// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter_tools/src/android/android_device.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

const kLastLogcatTimestamp = '11-27 15:39:04.506';

void main() {
  testWithoutContext('AdbLogReader logs exit code and stderr when adb process exits', () async {
    final logger = BufferLogger.test();
    final processCompleter = Completer<void>();

    final processManager = FakeProcessManager.list(<FakeCommand>[
      FakeCommand(
        command: const <String>['adb', '-s', '1234', 'shell', '-x', 'logcat', '-v', 'time'],
        completer: processCompleter,
        exitCode: 1,
        stderr: 'adb: device lost',
      ),
    ]);

    final AdbLogReader logReader = await AdbLogReader.createLogReader(
      createFakeDevice(null),
      processManager,
      logger,
    );

    final onDone = Completer<void>.sync();
    logReader.logLines.listen((String _) {}, onDone: onDone.complete);

    // Complete the process, causing it to exit with code 1.
    processCompleter.complete();

    await onDone.future;

    // We expect a log message indicating the process exited with code 1 and showing the stderr.
    expect(logger.errorText, contains('adb logcat process exited with code 1'));
    expect(logger.errorText, contains('adb: device lost'));
    expect(processManager, hasNoRemainingExpectations);
  });

  testWithoutContext(
    'AdbLogReader logs exit code when adb process exits with 0 unexpectedly',
    () async {
      final logger = BufferLogger.test();
      final processCompleter = Completer<void>();

      final processManager = FakeProcessManager.list(<FakeCommand>[
        FakeCommand(
          command: const <String>['adb', '-s', '1234', 'shell', '-x', 'logcat', '-v', 'time'],
          completer: processCompleter,
        ),
      ]);

      final AdbLogReader logReader = await AdbLogReader.createLogReader(
        createFakeDevice(null),
        processManager,
        logger,
      );

      final onDone = Completer<void>.sync();
      logReader.logLines.listen((String _) {}, onDone: onDone.complete);

      processCompleter.complete();

      await onDone.future;

      expect(logger.errorText, contains('adb logcat process exited with code 0'));
      expect(processManager, hasNoRemainingExpectations);
    },
  );

  testWithoutContext('AdbLogReader does not log when stopped manually', () async {
    final logger = BufferLogger.test();
    final processCompleter = Completer<void>();

    final processManager = FakeProcessManager.list(<FakeCommand>[
      FakeCommand(
        command: const <String>['adb', '-s', '1234', 'shell', '-x', 'logcat', '-v', 'time'],
        completer: processCompleter,
      ),
    ]);

    final AdbLogReader logReader = await AdbLogReader.createLogReader(
      createFakeDevice(null),
      processManager,
      logger,
    );

    final StreamSubscription<String> subscription = logReader.logLines.listen((String _) {});

    // Simulate manual stop (cancelling subscription triggers _stop)
    await subscription.cancel();

    // Complete the process after stopping
    processCompleter.complete();

    // Verify no errors were logged
    expect(logger.errorText, isEmpty);
    expect(processManager, hasNoRemainingExpectations);
  });
}

AndroidDevice createFakeDevice(int? sdkLevel) {
  return FakeAndroidDevice(sdkLevel.toString(), kLastLogcatTimestamp);
}

class FakeAndroidDevice extends Fake implements AndroidDevice {
  FakeAndroidDevice(this._apiVersion, this._lastLogcatTimestamp);

  final String _lastLogcatTimestamp;
  final String _apiVersion;

  @override
  String get name => 'test-device';

  @override
  String get displayName => name;

  @override
  Future<String> get apiVersion => Future<String>.value(_apiVersion);

  @override
  Future<String> lastLogcatTimestamp() async => _lastLogcatTimestamp;

  @override
  List<String> adbCommandForDevice(List<String> command) {
    return <String>['adb', '-s', '1234', ...command];
  }
}
