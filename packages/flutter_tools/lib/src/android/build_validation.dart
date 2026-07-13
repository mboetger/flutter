// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../base/common.dart';
import '../base/file_system.dart';
import '../base/io.dart';
import '../base/logger.dart';
import '../base/platform.dart';
import '../build_info.dart';
import 'package:process/process.dart';

const kGooglePlayVersioning = 'https://developer.android.com/studio/publish/versioning.html';
const kSupportedAbis = 'https://flutter.dev/to/android-supported-architectures';

/// Validates that the build mode and build number are valid for a given build.
void validateBuild(
  AndroidBuildInfo androidBuildInfo, {
  required FileSystem fileSystem,
  required Platform platform,
  required ProcessManager processManager,
  required Directory projectDirectory,
  Logger? logger,
}) {
  if (platform.isWindows) {
    final String absolutePath = projectDirectory.absolute.path;
    final RegExp driveLetterRegex = RegExp(r'^([a-zA-Z]):');
    final Match? match = driveLetterRegex.firstMatch(absolutePath);
    if (match != null) {
      final String driveLetter = match.group(1)!;
      try {
        final ProcessResult result = processManager.runSync(<String>[
          'powershell.exe',
          '-NoProfile',
          '-NonInteractive',
          '-Command',
          'Get-Volume -DriveLetter $driveLetter | Select-Object -ExpandProperty FileSystem',
        ]);
        if (result.exitCode == 0) {
          final String fileSystemType = (result.stdout as String).trim();
          if (fileSystemType.isNotEmpty && fileSystemType.toUpperCase() != 'NTFS') {
            throwToolExit(
              'The project directory is on a non-NTFS ($fileSystemType) drive. '
              'Android builds on Windows are only supported on NTFS drives due to filesystem limitations.',
            );
          }
        } else {
          logger?.printTrace('Get-Volume failed with exit code ${result.exitCode}: ${result.stderr}');
        }
      } on ToolExit {
        rethrow;
      } catch (error, stackTrace) {
        logger?.printTrace('Failed to run powershell Get-Volume command: $error\n$stackTrace');
      }
    }
  }

  final BuildInfo buildInfo = androidBuildInfo.buildInfo;
  if (buildInfo.codeSizeDirectory != null && androidBuildInfo.targetArchs.length > 1) {
    throwToolExit(
      'Cannot perform code size analysis when building for multiple ABIs. '
      'Specify one of android-arm, android-arm64, or android-x64 in the '
      '--target-platform flag.',
    );
  }
  if (buildInfo.buildNumber != null) {
    final int? result = int.tryParse(buildInfo.buildNumber!);
    if (result == null) {
      throwToolExit(
        'buildNumber: ${buildInfo.buildNumber} was not a valid integer value.\n'
        'For more information see $kGooglePlayVersioning .',
      );
    }
    if (result < 0) {
      throwToolExit(
        'buildNumber: ${buildInfo.buildNumber} must be a positive integer value.\n'
        'For more information see $kGooglePlayVersioning .',
      );
    }
    if (result > 2100000000) {
      throwToolExit(
        'buildNumber: ${buildInfo.buildNumber} is greater than the maximum '
        'allowed value of 2100000000.\n'
        'For more information see $kGooglePlayVersioning .',
      );
    }
  }
}
