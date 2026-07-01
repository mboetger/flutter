// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_device.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/application_package.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/device.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

const kAdbVersionCommand = FakeCommand(
  command: <String>['adb', 'version'],
  stdout: 'Android Debug Bridge version 1.0.39',
);

const kStartServer = FakeCommand(command: <String>['adb', 'start-server']);

const kShaCommand = FakeCommand(
  command: <String>[
    'adb',
    '-s',
    '1234',
    'shell',
    'echo',
    '-n',
    '',
    '>',
    '/data/local/tmp/sky.FlutterApp.sha1',
  ],
);

void main() {
  late FileSystem fileSystem;
  late FakeProcessManager processManager;
  late AndroidSdk androidSdk;

  setUp(() {
    processManager = FakeProcessManager.empty();
    fileSystem = MemoryFileSystem.test();
    androidSdk = FakeAndroidSdk();
  });

  testWithoutContext('AndroidDevice.startApp passes --ei vm-service-port <port>', () async {
    final device = AndroidDevice(
      '1234',
      modelID: 'TestModel',
      fileSystem: fileSystem,
      processManager: processManager,
      logger: BufferLogger.test(),
      platform: FakePlatform(),
      androidSdk: androidSdk,
    );
    final File apkFile = fileSystem.file('app-debug.apk')..createSync();
    final apk = AndroidApk(
      id: 'FlutterApp',
      applicationPackage: apkFile,
      launchActivity: 'FlutterActivity',
      versionCode: 1,
    );

    final Completer<void> logcatCompleter = Completer<void>();

    processManager.addCommand(kAdbVersionCommand);
    processManager.addCommand(kStartServer);
    processManager.addCommand(
      const FakeCommand(
        command: <String>['adb', '-s', '1234', 'shell', 'getprop'],
        stdout: '[ro.product.cpu.abi]: [arm64-v8a]',
      ),
    );
    processManager.addCommand(
      const FakeCommand(
        command: <String>['adb', '-s', '1234', 'shell', 'am', 'force-stop', 'FlutterApp'],
      ),
    );
    processManager.addCommand(
      const FakeCommand(
        command: <String>['adb', '-s', '1234', 'install', '-t', '-r', 'app-debug.apk'],
      ),
    );
    processManager.addCommand(kShaCommand);
    processManager.addCommand(
      FakeCommand(
        command: const <String>['adb', '-s', '1234', 'shell', '-x', 'logcat', '-v', 'time'],
        stdout: '12-31 23:59:59.123 I/flutter ( 1234): The Dart VM service is listening on http://127.0.0.1:1234/\n',
        completer: logcatCompleter,
      ),
    );

    // This command should contain the --ei vm-service-port 1234 option.
    processManager.addCommand(
      const FakeCommand(
        command: <String>[
          'adb',
          '-s',
          '1234',
          'shell',
          'am',
          'start',
          '-a',
          'android.intent.action.MAIN',
          '-c',
          'android.intent.category.LAUNCHER',
          '-f',
          '0x20000000',
          '--ez', 'enable-dart-profiling', 'true',
          '--ez', 'enable-checked-mode', 'true',
          '--ez', 'verify-entry-points', 'true',
          '--ei', 'vm-service-port', '1234',
          'FlutterActivity',
        ],
      ),
    );

    // Mock adb forward command that will be run when VM service port is discovered.
    processManager.addCommand(
      const FakeCommand(
        command: <String>['adb', '-s', '1234', 'forward', 'tcp:0', 'tcp:1234'],
        stdout: '1234',
      ),
    );

    final LaunchResult launchResult = await device.startApp(
      apk,
      prebuiltApplication: true,
      debuggingOptions: DebuggingOptions.enabled(
        BuildInfo.debug,
        deviceVmServicePort: 1234,
        enableDartProfiling: true,
      ),
      platformArgs: <String, dynamic>{},
    );

    logcatCompleter.complete();

    expect(launchResult.started, true);
    expect(launchResult.vmServiceUri, Uri.parse('http://127.0.0.1:1234/'));
    expect(processManager, hasNoRemainingExpectations);
  });

  testWithoutContext('AndroidDevice.startApp does not pass --ei vm-service-port when port is null', () async {
    final device = AndroidDevice(
      '1234',
      modelID: 'TestModel',
      fileSystem: fileSystem,
      processManager: processManager,
      logger: BufferLogger.test(),
      platform: FakePlatform(),
      androidSdk: androidSdk,
    );
    final File apkFile = fileSystem.file('app-debug.apk')..createSync();
    final apk = AndroidApk(
      id: 'FlutterApp',
      applicationPackage: apkFile,
      launchActivity: 'FlutterActivity',
      versionCode: 1,
    );

    final Completer<void> logcatCompleter = Completer<void>();

    processManager.addCommand(kAdbVersionCommand);
    processManager.addCommand(kStartServer);
    processManager.addCommand(
      const FakeCommand(
        command: <String>['adb', '-s', '1234', 'shell', 'getprop'],
        stdout: '[ro.product.cpu.abi]: [arm64-v8a]',
      ),
    );
    processManager.addCommand(
      const FakeCommand(
        command: <String>['adb', '-s', '1234', 'shell', 'am', 'force-stop', 'FlutterApp'],
      ),
    );
    processManager.addCommand(
      const FakeCommand(
        command: <String>['adb', '-s', '1234', 'install', '-t', '-r', 'app-debug.apk'],
      ),
    );
    processManager.addCommand(kShaCommand);
    processManager.addCommand(
      FakeCommand(
        command: const <String>['adb', '-s', '1234', 'shell', '-x', 'logcat', '-v', 'time'],
        stdout: '12-31 23:59:59.123 I/flutter ( 1234): The Dart VM service is listening on http://127.0.0.1:1234/\n',
        completer: logcatCompleter,
      ),
    );

    // This command should NOT contain the --ei vm-service-port option.
    processManager.addCommand(
      const FakeCommand(
        command: <String>[
          'adb',
          '-s',
          '1234',
          'shell',
          'am',
          'start',
          '-a',
          'android.intent.action.MAIN',
          '-c',
          'android.intent.category.LAUNCHER',
          '-f',
          '0x20000000',
          '--ez', 'enable-dart-profiling', 'true',
          '--ez', 'enable-checked-mode', 'true',
          '--ez', 'verify-entry-points', 'true',
          'FlutterActivity',
        ],
      ),
    );

    // Mock adb forward command that will be run when VM service port is discovered.
    processManager.addCommand(
      const FakeCommand(
        command: <String>['adb', '-s', '1234', 'forward', 'tcp:0', 'tcp:1234'],
        stdout: '1234',
      ),
    );

    final LaunchResult launchResult = await device.startApp(
      apk,
      prebuiltApplication: true,
      debuggingOptions: DebuggingOptions.enabled(
        BuildInfo.debug,
        enableDartProfiling: true,
      ),
      platformArgs: <String, dynamic>{},
    );

    logcatCompleter.complete();

    expect(launchResult.started, true);
    expect(launchResult.vmServiceUri, Uri.parse('http://127.0.0.1:1234/'));
    expect(processManager, hasNoRemainingExpectations);
  });
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  String get adbPath => 'adb';

  @override
  bool get licensesAvailable => false;
}
