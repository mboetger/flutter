// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_device_discovery.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_workflow.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/os.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/user_messages.dart';
import 'package:flutter_tools/src/device.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fakes.dart';

void main() {
  late MemoryFileSystem fileSystem;
  late FakeProcessManager processManager;
  late AdbFakeOperatingSystemUtils osUtils;
  late FakePlatform platform;

  setUp(() {
    fileSystem = MemoryFileSystem.test();
    processManager = FakeProcessManager.empty();
    platform = FakePlatform();
  });

  testUsingContext(
    'AndroidDevices does not run adb if Android SDK is invalid',
    () async {
      final Directory sdkDir = fileSystem.directory('/sdk');
      sdkDir.childDirectory('platform-tools').createSync(recursive: true);
      final File adbFile = sdkDir.childDirectory('platform-tools').childFile('adb');
      adbFile.createSync();

      osUtils = AdbFakeOperatingSystemUtils(adbFiles: <File>[adbFile]);

      final AndroidSdk? androidSdk = AndroidSdk.locateAndroidSdk();
      expect(androidSdk, isNotNull);
      expect(androidSdk!.directory.path, '/sdk');
      expect(androidSdk.latestVersion, isNull);

      final androidWorkflow = AndroidWorkflow(
        androidSdk: androidSdk,
        featureFlags: TestFeatureFlags(),
      );
      expect(androidWorkflow.canListDevices, isTrue);

      final androidDevices = AndroidDevices(
        androidSdk: androidSdk,
        logger: BufferLogger.test(),
        androidWorkflow: androidWorkflow,
        processManager: processManager,
        fileSystem: fileSystem,
        platform: platform,
        userMessages: UserMessages(),
      );

      // This should not run adb because the SDK is invalid.
      // If it does run adb, FakeProcessManager.empty() will throw, failing the test.
      final List<Device> devices = await androidDevices.pollingGetDevices();
      expect(devices, isEmpty);
    },
    overrides: <Type, Generator>{
      FileSystem: () => fileSystem,
      ProcessManager: () => processManager,
      OperatingSystemUtils: () => osUtils,
      Platform: () => platform,
    },
  );
}

class AdbFakeOperatingSystemUtils extends FakeOperatingSystemUtils {
  AdbFakeOperatingSystemUtils({required this.adbFiles});
  final List<File> adbFiles;

  @override
  List<File> whichAll(String execName) {
    if (execName == 'adb') {
      return adbFiles;
    }
    return <File>[];
  }
}
