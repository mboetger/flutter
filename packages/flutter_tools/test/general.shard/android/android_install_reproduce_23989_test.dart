// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'package:fake_async/fake_async.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_device.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/application_package.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

const kAdbVersionCommand = FakeCommand(
  command: <String>['adb', 'version'],
  stdout: 'Android Debug Bridge version 1.0.39',
);
const kAdbStartServerCommand = FakeCommand(command: <String>['adb', 'start-server']);

void main() {
  late FileSystem fileSystem;
  late BufferLogger logger;

  setUp(() {
    fileSystem = MemoryFileSystem.test();
    logger = BufferLogger.test();
  });

  AndroidDevice setUpAndroidDevice({AndroidSdk? androidSdk, ProcessManager? processManager}) {
    androidSdk ??= FakeAndroidSdk();
    return AndroidDevice(
      '1234',
      modelID: 'TestModel',
      logger: logger,
      platform: FakePlatform(),
      androidSdk: androidSdk,
      fileSystem: fileSystem,
      processManager: processManager ?? FakeProcessManager.any(),
    );
  }

  testWithoutContext('installApp hangs when adb install hangs', () {
    final adbInstallCompleter = Completer<void>();

    final processManager = FakeProcessManager.list(<FakeCommand>[
      kAdbVersionCommand,
      kAdbStartServerCommand,
      const FakeCommand(
        command: <String>['adb', '-s', '1234', 'shell', 'getprop'],
        stdout: '[ro.build.version.sdk]: [27]',
      ),
      FakeCommand(
        command: const <String>['adb', '-s', '1234', 'install', '-t', '-r', 'app-debug.apk'],
        completer: adbInstallCompleter,
      ),
    ]);

    final File apk = fileSystem.file('app-debug.apk')..createSync();
    final androidApk = AndroidApk(
      applicationPackage: apk,
      id: 'app',
      versionCode: 22,
      launchActivity: 'Main',
    );
    final AndroidDevice androidDevice = setUpAndroidDevice(processManager: processManager);

    fakeAsync((FakeAsync time) {
      bool? result;
      androidDevice.installApp(androidApk).then((value) {
        result = value;
      });

      try {
        // Elapse 4 minutes. Timeout is 5 minutes, so it should still be pending (null).
        time.elapse(const Duration(minutes: 4));
        time.flushMicrotasks();
        expect(result, isNull);

        // Elapse another 2 minutes (total 6 minutes). It should have timed out and returned false.
        time.elapse(const Duration(minutes: 2));
        time.flushMicrotasks();
        expect(result, isFalse);
      } finally {
        // Clean up the completer to avoid leaks.
        if (!adbInstallCompleter.isCompleted) {
          adbInstallCompleter.complete();
        }
        time.flushMicrotasks();
      }
    });

    expect(processManager, hasNoRemainingExpectations);
  });

  testWithoutContext('isAppInstalled returns false when adb shell hangs', () {
    final adbShellCompleter = Completer<void>();
    final processManager = FakeProcessManager.list(<FakeCommand>[
      FakeCommand(
        command: const <String>['adb', '-s', '1234', 'shell', 'pm', 'list', 'packages', 'app'],
        completer: adbShellCompleter,
      ),
    ]);

    final File apk = fileSystem.file('app-debug.apk')..createSync();
    final androidApk = AndroidApk(
      applicationPackage: apk,
      id: 'app',
      versionCode: 22,
      launchActivity: 'Main',
    );
    final AndroidDevice androidDevice = setUpAndroidDevice(processManager: processManager);

    fakeAsync((FakeAsync time) {
      bool? result;
      androidDevice.isAppInstalled(androidApk).then((value) {
        result = value;
      });

      try {
        // Elapse 20 seconds. Timeout is 30 seconds, so it should still be null.
        time.elapse(const Duration(seconds: 20));
        time.flushMicrotasks();
        expect(result, isNull);

        // Elapse another 20 seconds (total 40 seconds). It should have timed out and returned false.
        time.elapse(const Duration(seconds: 20));
        time.flushMicrotasks();
        expect(result, isFalse);
      } finally {
        if (!adbShellCompleter.isCompleted) {
          adbShellCompleter.complete();
        }
        time.flushMicrotasks();
      }
    });

    expect(processManager, hasNoRemainingExpectations);
  });
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  String get adbPath => 'adb';
}
