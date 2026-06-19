// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_builder.dart';
import 'package:flutter_tools/src/android/android_device.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/application_package.dart';
import 'package:flutter_tools/src/application_package.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/device.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/fake.dart';

import '../../src/android_common.dart';
import '../../src/common.dart';
import '../../src/context.dart';

void main() {
  late FakeAndroidSdk mockAndroidSdk;
  late MemoryFileSystem fileSystem;
  late FakeAndroidApk fakeApk;
  late _TargetPlatformCapturingAndroidBuilder mockAndroidBuilder;

  setUp(() {
    mockAndroidSdk = FakeAndroidSdk();
    fileSystem = MemoryFileSystem.test();
    final File apkFile = fileSystem.file('app.apk')..createSync();
    fakeApk = FakeAndroidApk(apkFile);
    mockAndroidBuilder = _TargetPlatformCapturingAndroidBuilder();
  });

  testUsingContext(
    'AndroidDevice.startApp respects targetPlatform from BuildInfo',
    () async {
      const deviceId = '1234';
      final device = AndroidDevice(
        deviceId,
        modelID: 'TestModel',
        logger: BufferLogger.test(),
        platform: FakePlatform(),
        androidSdk: mockAndroidSdk,
        fileSystem: fileSystem,
        processManager: FakeProcessManager.list(<FakeCommand>[
          // 1. adb version check during _adbIsValid
          const FakeCommand(
            command: <String>['adb', 'version'],
            stdout:
                'Android Debug Bridge version 1.0.39\nRevision 3db08f2c6889-android\nInstalled as /home/vboxuser/Android/Sdk/platform-tools/adb',
          ),
          // 2. adb start-server check during _adbIsValid
          const FakeCommand(command: <String>['adb', 'start-server']),
          // 3. adb shell getprop to load device properties (sdk version, cpu abi, etc.)
          const FakeCommand(
            command: <String>['adb', '-s', '1234', 'shell', 'getprop'],
            stdout:
                '[ro.product.cpu.abi]: [arm64-v8a]\n[ro.product.cpu.abilist]: [arm64-v8a]\n[ro.build.version.sdk]: [24]',
          ),
          // 4. stopApp call before install
          const FakeCommand(
            command: <String>['adb', '-s', '1234', 'shell', 'am', 'force-stop', 'com.example.app'],
          ),
          // 5. installApp call
          const FakeCommand(
            command: <String>['adb', '-s', '1234', 'install', '-t', '-r', 'app.apk'],
          ),
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
              '/data/local/tmp/sky.com.example.app.sha1',
            ],
          ),
          // 6. am start call to launch the activity
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
              '--ez',
              'enable-dart-profiling',
              'true',
              'com.example.app/.MainActivity',
            ],
          ),
        ]),
      );

      await device.startApp(
        fakeApk,
        debuggingOptions: DebuggingOptions.disabled(
          const BuildInfo(
            BuildMode.debug,
            null,
            targetPlatform: TargetPlatform.android_arm,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
        ),
      );

      // We expect the builder to be called with AndroidArch.armeabi_v7a (corresponding to targetPlatform: android_arm override).
      expect(
        mockAndroidBuilder.capturedAndroidBuildInfo?.targetArchs,
        contains(AndroidArch.armeabi_v7a),
      );
    },
    overrides: <Type, Generator>{
      AndroidSdk: () => mockAndroidSdk,
      AndroidBuilder: () => mockAndroidBuilder,
      ApplicationPackageFactory: () => FakeApplicationPackageFactory(fakeApk),
    },
  );
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  String get adbPath => 'adb';

  @override
  bool get licensesAvailable => true;

  @override
  AndroidSdkVersion? get latestVersion => null;
}

class FakeAndroidApk extends Fake implements AndroidApk {
  FakeAndroidApk(this.applicationPackage);

  @override
  String get id => 'com.example.app';

  @override
  String get name => 'app.apk';

  @override
  final File applicationPackage;

  @override
  String get launchActivity => 'com.example.app/.MainActivity';
}

class FakeApplicationPackageFactory extends Fake implements ApplicationPackageFactory {
  FakeApplicationPackageFactory(this.app);

  final ApplicationPackage app;

  @override
  Future<ApplicationPackage> getPackageForPlatform(
    TargetPlatform platform, {
    BuildInfo? buildInfo,
    File? applicationBinary,
  }) async {
    return app;
  }
}

class _TargetPlatformCapturingAndroidBuilder extends FakeAndroidBuilder {
  AndroidBuildInfo? capturedAndroidBuildInfo;

  @override
  Future<void> buildApk({
    required FlutterProject project,
    required AndroidBuildInfo androidBuildInfo,
    required String target,
    bool configOnly = false,
  }) async {
    capturedAndroidBuildInfo = androidBuildInfo;
  }
}
