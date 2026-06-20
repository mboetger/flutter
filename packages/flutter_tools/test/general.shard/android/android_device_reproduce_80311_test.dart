// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_device.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/application_package.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' as gradle_utils;
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
const kInstallCommand = FakeCommand(
  command: <String>['adb', '-s', '1234', 'install', '-t', '-r', '--user', '10', 'app-debug.apk'],
);

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

  testWithoutContext(
    'INSTALL_FAILED_UPDATE_INCOMPATIBLE warning message is filtered from non-verbose tool logs on successful recovery',
    () async {
      final processManager = FakeProcessManager.list(<FakeCommand>[
        kAdbVersionCommand,
        kAdbStartServerCommand,
        const FakeCommand(
          command: <String>['adb', '-s', '1234', 'shell', 'getprop'],
          stdout: '[ro.build.version.sdk]: [${gradle_utils.targetSdkVersion}]',
        ),
        // First install attempt fails with INSTALL_FAILED_UPDATE_INCOMPATIBLE
        const FakeCommand(
          command: <String>[
            'adb',
            '-s',
            '1234',
            'install',
            '-t',
            '-r',
            '--user',
            '10',
            'app-debug.apk',
          ],
          exitCode: 1,
          stderr:
              'adb: failed to install app-debug.apk: Failure\n'
              '[INSTALL_FAILED_UPDATE_INCOMPATIBLE: Package com.example.p2 signatures do not match previously installed\n'
              'version; ignoring!]',
        ),
        // Check if app is installed
        const FakeCommand(
          command: <String>[
            'adb',
            '-s',
            '1234',
            'shell',
            'pm',
            'list',
            'packages',
            '--user',
            '10',
            'app',
          ],
          stdout: 'package:app\n',
        ),
        // Uninstall the old version
        const FakeCommand(
          command: <String>['adb', '-s', '1234', 'uninstall', '--user', '10', 'app'],
        ),
        // Re-install the new version (succeeds)
        kInstallCommand,
        const FakeCommand(
          command: <String>[
            'adb',
            '-s',
            '1234',
            'shell',
            'echo',
            '-n',
            'example_sha',
            '>',
            '/data/local/tmp/sky.app.sha1',
          ],
        ),
      ]);

      final File apk = fileSystem.file('app-debug.apk')..createSync();
      fileSystem.file('app-debug.apk.sha1').writeAsStringSync('example_sha');
      final androidApk = AndroidApk(
        applicationPackage: apk,
        id: 'app',
        versionCode: 22,
        launchActivity: 'Main',
      );
      final AndroidDevice androidDevice = setUpAndroidDevice(processManager: processManager);

      // Perform the installation
      final bool installResult = await androidDevice.installApp(androidApk, userIdentifier: '10');

      // The installation should succeed eventually through recovery (uninstall & reinstall)
      expect(installResult, isTrue);
      expect(processManager, hasNoRemainingExpectations);

      // Verify that the INSTALL_FAILED_UPDATE_INCOMPATIBLE warning/error was NOT printed to non-verbose logs (errorText)
      expect(logger.errorText, isNot(contains('INSTALL_FAILED_UPDATE_INCOMPATIBLE')));
      expect(logger.errorText, isNot(contains('Error: ADB exited with exit code 1')));
    },
  );

  testWithoutContext(
    'INSTALL_FAILED_UPDATE_INCOMPATIBLE prints actionable error and does not uninstall if app is not installed for target user',
    () async {
      final processManager = FakeProcessManager.list(<FakeCommand>[
        kAdbVersionCommand,
        kAdbStartServerCommand,
        const FakeCommand(
          command: <String>['adb', '-s', '1234', 'shell', 'getprop'],
          stdout: '[ro.build.version.sdk]: [${gradle_utils.targetSdkVersion}]',
        ),
        // First install attempt fails with INSTALL_FAILED_UPDATE_INCOMPATIBLE
        const FakeCommand(
          command: <String>[
            'adb',
            '-s',
            '1234',
            'install',
            '-t',
            '-r',
            '--user',
            '10',
            'app-debug.apk',
          ],
          exitCode: 1,
          stderr:
              'adb: failed to install app-debug.apk: Failure\n'
              '[INSTALL_FAILED_UPDATE_INCOMPATIBLE: Package com.example.p2 signatures do not match previously installed\n'
              'version; ignoring!]',
        ),
        // Check if app is installed for the target user (returns false/empty)
        const FakeCommand(
          command: <String>[
            'adb',
            '-s',
            '1234',
            'shell',
            'pm',
            'list',
            'packages',
            '--user',
            '10',
            'app',
          ],
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

      // Perform the installation
      final bool installResult = await androidDevice.installApp(androidApk, userIdentifier: '10');

      // The installation should fail
      expect(installResult, isFalse);
      expect(processManager, hasNoRemainingExpectations);

      // Verify that it did not print ADB exit code error or the raw signature mismatch to non-verbose logs
      expect(logger.errorText, isNot(contains('INSTALL_FAILED_UPDATE_INCOMPATIBLE')));
      expect(logger.errorText, isNot(contains('Error: ADB exited with exit code 1')));

      // Verify that it printed the highly actionable error message instructing the user how to resolve the multi-user signature mismatch
      expect(
        logger.errorText,
        contains(
          'Installing APK failed because the app is not installed for user 10, '
          'but is installed for another user with an incompatible signature.\n'
          'Please uninstall the app for all users (e.g. using "adb uninstall app") and try again.',
        ),
      );
    },
  );

  testWithoutContext(
    'INSTALL_FAILED_UPDATE_INCOMPATIBLE on second install attempt prints actionable error message',
    () async {
      final processManager = FakeProcessManager.list(<FakeCommand>[
        kAdbVersionCommand,
        kAdbStartServerCommand,
        const FakeCommand(
          command: <String>['adb', '-s', '1234', 'shell', 'getprop'],
          stdout: '[ro.build.version.sdk]: [${gradle_utils.targetSdkVersion}]',
        ),
        // First install attempt fails with INSTALL_FAILED_UPDATE_INCOMPATIBLE
        const FakeCommand(
          command: <String>[
            'adb',
            '-s',
            '1234',
            'install',
            '-t',
            '-r',
            '--user',
            '10',
            'app-debug.apk',
          ],
          exitCode: 1,
          stderr:
              'adb: failed to install app-debug.apk: Failure\n'
              '[INSTALL_FAILED_UPDATE_INCOMPATIBLE: Package com.example.p2 signatures do not match previously installed\n'
              'version; ignoring!]',
        ),
        // Check if app is installed (returns package:app)
        const FakeCommand(
          command: <String>[
            'adb',
            '-s',
            '1234',
            'shell',
            'pm',
            'list',
            'packages',
            '--user',
            '10',
            'app',
          ],
          stdout: 'package:app\n',
        ),
        // Uninstall the old version for target user
        const FakeCommand(
          command: <String>['adb', '-s', '1234', 'uninstall', '--user', '10', 'app'],
        ),
        // Second install attempt still fails with INSTALL_FAILED_UPDATE_INCOMPATIBLE (due to other user profile)
        const FakeCommand(
          command: <String>[
            'adb',
            '-s',
            '1234',
            'install',
            '-t',
            '-r',
            '--user',
            '10',
            'app-debug.apk',
          ],
          exitCode: 1,
          stderr:
              'adb: failed to install app-debug.apk: Failure\n'
              '[INSTALL_FAILED_UPDATE_INCOMPATIBLE: Package com.example.p2 signatures do not match previously installed\n'
              'version; ignoring!]',
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

      // Perform the installation
      final bool installResult = await androidDevice.installApp(androidApk, userIdentifier: '10');

      // The installation should fail
      expect(installResult, isFalse);
      expect(processManager, hasNoRemainingExpectations);

      // Verify that it printed the highly actionable error message instructing the user how to resolve the multi-user signature mismatch
      expect(
        logger.errorText,
        contains(
          'Installing APK failed because the app is still installed for another user with an incompatible signature.\n'
          'Please uninstall the app for all users (e.g. using "adb uninstall app") and try again.',
        ),
      );
    },
  );
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  String get adbPath => 'adb';
}
