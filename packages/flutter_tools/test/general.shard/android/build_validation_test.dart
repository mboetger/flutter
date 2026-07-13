// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/build_validation.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:process/process.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

void main() {
  final FileSystem fileSystem = MemoryFileSystem.test();
  final Platform platform = FakePlatform();
  final ProcessManager processManager = FakeProcessManager.any();
  final Directory projectDirectory = fileSystem.directory('/project');

  testWithoutContext('validateBuild does not throw on AOT supported architectures', () {
    expect(
      () => validateBuild(
        const AndroidBuildInfo(
          BuildInfo.release,
          targetArchs: <AndroidArch>[
            AndroidArch.x86_64,
            AndroidArch.armeabi_v7a,
            AndroidArch.arm64_v8a,
          ],
        ),
        fileSystem: fileSystem,
        platform: platform,
        processManager: processManager,
        projectDirectory: projectDirectory,
      ),
      returnsNormally,
    );
  });

  testWithoutContext('validateBuild throws if an invalid build number is specified', () {
    expect(
      () => validateBuild(
        const AndroidBuildInfo(
          // Invalid number
          BuildInfo(
            BuildMode.debug,
            '',
            treeShakeIcons: false,
            buildNumber: 'a',
            packageConfigPath: '.dart_tool/package_config.json',
          ),
          targetArchs: <AndroidArch>[AndroidArch.x86_64],
        ),
        fileSystem: fileSystem,
        platform: platform,
        processManager: processManager,
        projectDirectory: projectDirectory,
      ),
      throwsToolExit(message: 'buildNumber: a was not a valid integer value.'),
    );

    expect(
      () => validateBuild(
        const AndroidBuildInfo(
          // Negative number
          BuildInfo(
            BuildMode.debug,
            '',
            treeShakeIcons: false,
            buildNumber: '-1',
            packageConfigPath: '.dart_tool/package_config.json',
          ),
          targetArchs: <AndroidArch>[AndroidArch.x86_64],
        ),
        fileSystem: fileSystem,
        platform: platform,
        processManager: processManager,
        projectDirectory: projectDirectory,
      ),
      throwsToolExit(message: 'buildNumber: -1 must be a positive integer value.'),
    );

    expect(
      () => validateBuild(
        const AndroidBuildInfo(
          // bigger than maximum supported play store value
          BuildInfo(
            BuildMode.debug,
            '',
            treeShakeIcons: false,
            buildNumber: '2100000001',
            packageConfigPath: '.dart_tool/package_config.json',
          ),
          targetArchs: <AndroidArch>[AndroidArch.x86_64],
        ),
        fileSystem: fileSystem,
        platform: platform,
        processManager: processManager,
        projectDirectory: projectDirectory,
      ),
      throwsToolExit(
        message:
            'buildNumber: 2100000001 is greater than the maximum '
            'allowed value of 2100000000.',
      ),
    );
  });

  testWithoutContext('validateBuild does not throw on positive number', () {
    expect(
      () => validateBuild(
        const AndroidBuildInfo(
          BuildInfo(
            BuildMode.debug,
            '',
            treeShakeIcons: false,
            buildNumber: '2',
            packageConfigPath: '.dart_tool/package_config.json',
          ),
          targetArchs: <AndroidArch>[AndroidArch.x86_64],
        ),
        fileSystem: fileSystem,
        platform: platform,
        processManager: processManager,
        projectDirectory: projectDirectory,
      ),
      returnsNormally,
    );
  });
}
