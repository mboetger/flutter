// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/build_validation.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:test/test.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

void main() {
  testWithoutContext('validateBuild throws ToolExit when project is on non-NTFS drive on Windows', () async {
    final FileSystem fileSystem = MemoryFileSystem.test(style: FileSystemStyle.windows);
    final Platform platform = FakePlatform(operatingSystem: 'windows');
    
    // We place the project on D: drive
    final Directory projectDirectory = fileSystem.directory(r'D:\project');
    projectDirectory.createSync(recursive: true);

    // Mock ProcessManager to return exFAT for D drive
    final ProcessManager processManager = FakeProcessManager.list(<FakeCommand>[
      const FakeCommand(
        command: <String>[
          'powershell.exe',
          '-NoProfile',
          '-NonInteractive',
          '-Command',
          'Get-Volume -DriveLetter D | Select-Object -ExpandProperty FileSystem',
        ],
        stdout: 'exFAT\r\n',
      ),
    ]);

    expect(
      () => validateBuild(
        const AndroidBuildInfo(
          BuildInfo.release,
          targetArchs: <AndroidArch>[AndroidArch.armeabi_v7a],
        ),
        fileSystem: fileSystem,
        platform: platform,
        processManager: processManager,
        projectDirectory: projectDirectory,
      ),
      throwsToolExit(
        message: 'The project directory is on a non-NTFS (exFAT) drive. '
            'Android builds on Windows are only supported on NTFS drives due to filesystem limitations.',
      ),
    );
  });

  testWithoutContext('validateBuild passes when project is on NTFS drive on Windows', () async {
    final FileSystem fileSystem = MemoryFileSystem.test(style: FileSystemStyle.windows);
    final Platform platform = FakePlatform(operatingSystem: 'windows');
    
    final Directory projectDirectory = fileSystem.directory(r'C:\project');
    projectDirectory.createSync(recursive: true);

    // Mock ProcessManager to return NTFS for C drive
    final ProcessManager processManager = FakeProcessManager.list(<FakeCommand>[
      const FakeCommand(
        command: <String>[
          'powershell.exe',
          '-NoProfile',
          '-NonInteractive',
          '-Command',
          'Get-Volume -DriveLetter C | Select-Object -ExpandProperty FileSystem',
        ],
        stdout: 'NTFS\r\n',
      ),
    ]);

    expect(
      () => validateBuild(
        const AndroidBuildInfo(
          BuildInfo.release,
          targetArchs: <AndroidArch>[AndroidArch.armeabi_v7a],
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
