// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

void main() {
  late FileSystem fileSystem;
  late FakeProcessManager processManager;
  late AndroidSdk androidSdk;

  setUp(() {
    processManager = FakeProcessManager.empty();
    fileSystem = MemoryFileSystem.test();
    androidSdk = FakeAndroidSdk();
  });

  testWithoutContext('AndroidDevice.startApp runs perfetto and pulls trace when both trace-startup and trace-systrace are enabled', () async {
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

    // These commands are required to install and start the app
    processManager.addCommand(const FakeCommand(
      command: <String>['adb', 'version'],
      stdout: 'Android Debug Bridge version 1.0.39',
    ));
    processManager.addCommand(const FakeCommand(command: <String>['adb', 'start-server']));
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
    processManager.addCommand(
      const FakeCommand(
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
      ),
    );

    // Expect perfetto command execution on the device to start tracing
    processManager.addCommand(
      const FakeCommand(
        command: <String>[
          'adb',
          '-s',
          '1234',
          'shell',
          'perfetto',
          '-c',
          '-',
          '--txt',
          '-o',
          '/data/local/tmp/trace.perfetto',
        ],
        // Running in background or simulated async block
      ),
    );

    // The app launch command contains both trace-startup and trace-systrace.
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
          '--ez', 'trace-startup', 'true',
          '--ez', 'trace-systrace', 'true',
          'FlutterActivity',
        ],
      ),
    );

    // Expect pulling the perfetto trace file from the device.
    processManager.addCommand(
      const FakeCommand(
        command: <String>[
          'adb',
          '-s',
          '1234',
          'pull',
          '/data/local/tmp/trace.perfetto',
          'build/start_up_systrace.perfetto',
        ],
      ),
    );

    final LaunchResult launchResult = await device.startApp(
      apk,
      prebuiltApplication: true,
      debuggingOptions: DebuggingOptions.disabled(
        BuildInfo.debug,
        traceSystrace: true,
      ),
      platformArgs: <String, dynamic>{
        'trace-startup': true,
      },
    );

    expect(launchResult.started, true);
    expect(processManager, hasNoRemainingExpectations);
  });
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  String get adbPath => 'adb';

  @override
  bool get licensesAvailable => false;
}
