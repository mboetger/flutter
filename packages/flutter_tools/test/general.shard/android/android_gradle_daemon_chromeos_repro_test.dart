// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/os.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/commands/build_apk.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fake_process_manager.dart';

void main() {
  group('isCrostini', () {
    late MemoryFileSystem fileSystem;

    setUp(() {
      fileSystem = MemoryFileSystem.test();
    });

    testWithoutContext('is true on Linux if /mnt/chromeos exists', () {
      fileSystem.directory('/mnt/chromeos').createSync(recursive: true);
      final Platform platform = FakePlatform(operatingSystem: 'linux');
      final OperatingSystemUtils os = OperatingSystemUtils(
        fileSystem: fileSystem,
        logger: BufferLogger.test(),
        platform: platform,
        processManager: FakeProcessManager.empty(),
      );
      expect(os.isCrostini, isTrue);
    });

    testWithoutContext('is false on Linux if /mnt/chromeos does not exist', () {
      final Platform platform = FakePlatform(operatingSystem: 'linux');
      final OperatingSystemUtils os = OperatingSystemUtils(
        fileSystem: fileSystem,
        logger: BufferLogger.test(),
        platform: platform,
        processManager: FakeProcessManager.empty(),
      );
      expect(os.isCrostini, isFalse);
    });

    testWithoutContext('is false on non-Linux even if /mnt/chromeos exists', () {
      fileSystem.directory('/mnt/chromeos').createSync(recursive: true);
      final Platform platform = FakePlatform(operatingSystem: 'windows');
      final OperatingSystemUtils os = OperatingSystemUtils(
        fileSystem: fileSystem,
        logger: BufferLogger.test(),
        platform: platform,
        processManager: FakeProcessManager.empty(),
      );
      expect(os.isCrostini, isFalse);
    });
  });

  group('android-gradle-daemon default value', () {
    late MemoryFileSystem fileSystem;

    setUp(() {
      fileSystem = MemoryFileSystem.test();
    });

    testUsingContext('defaults to false on Crostini', () {
      final BuildApkCommand command = BuildApkCommand(logger: BufferLogger.test());
      expect(command.argParser.options['android-gradle-daemon']?.defaultsTo, isFalse);
    }, overrides: <Type, Generator>{
      Platform: () => FakePlatform(operatingSystem: 'linux'),
      FileSystem: () => fileSystem,
      ProcessManager: () => FakeProcessManager.empty(),
      OperatingSystemUtils: () {
        fileSystem.directory('/mnt/chromeos').createSync(recursive: true);
        return OperatingSystemUtils(
          fileSystem: fileSystem,
          logger: BufferLogger.test(),
          platform: FakePlatform(operatingSystem: 'linux'),
          processManager: FakeProcessManager.empty(),
        );
      },
    });

    testUsingContext('defaults to true when not on Crostini', () {
      final BuildApkCommand command = BuildApkCommand(logger: BufferLogger.test());
      expect(command.argParser.options['android-gradle-daemon']?.defaultsTo, isTrue);
    }, overrides: <Type, Generator>{
      Platform: () => FakePlatform(operatingSystem: 'linux'),
      FileSystem: () => fileSystem,
      ProcessManager: () => FakeProcessManager.empty(),
      OperatingSystemUtils: () {
        return OperatingSystemUtils(
          fileSystem: fileSystem,
          logger: BufferLogger.test(),
          platform: FakePlatform(operatingSystem: 'linux'),
          processManager: FakeProcessManager.empty(),
        );
      },
    });
  });
}
