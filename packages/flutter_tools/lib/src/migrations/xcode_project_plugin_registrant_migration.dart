// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../base/file_system.dart';
import '../base/project_migrator.dart';
import '../xcode_project.dart';

/// Migrate the Xcode project to use the new ephemeral paths for GeneratedPluginRegistrant.
class XcodeProjectPluginRegistrantMigration extends ProjectMigrator {
  XcodeProjectPluginRegistrantMigration(XcodeBasedProject project, super.logger)
    : _xcodeProjectInfoFile = project.xcodeProjectInfoFile;

  final File _xcodeProjectInfoFile;

  @override
  Future<void> migrate() async {
    if (_xcodeProjectInfoFile.existsSync()) {
      processFileLines(_xcodeProjectInfoFile);
    } else {
      logger.printTrace(
        'Xcode project not found, skipping Xcode project plugin registrant migration.',
      );
    }
  }

  @override
  String? migrateLine(String line) {
    var updatedLine = line;
    if (line.contains('path = GeneratedPluginRegistrant.h;') &&
        line.contains('sourceTree = "<group>";')) {
      updatedLine = line
          .replaceAll(
            'path = GeneratedPluginRegistrant.h;',
            'path = ../.dart_tool/flutter_build/ios/GeneratedPluginRegistrant.h;',
          )
          .replaceAll('sourceTree = "<group>";', 'sourceTree = SOURCE_ROOT;');
    } else if (line.contains('path = GeneratedPluginRegistrant.m;') &&
        line.contains('sourceTree = "<group>";')) {
      updatedLine = line
          .replaceAll(
            'path = GeneratedPluginRegistrant.m;',
            'path = ../.dart_tool/flutter_build/ios/GeneratedPluginRegistrant.m;',
          )
          .replaceAll('sourceTree = "<group>";', 'sourceTree = SOURCE_ROOT;');
    } else if (line.contains('path = GeneratedPluginRegistrant.swift;') &&
        line.contains('sourceTree = "<group>";')) {
      updatedLine = line
          .replaceAll(
            'path = GeneratedPluginRegistrant.swift;',
            'path = ../.dart_tool/flutter_build/macos/GeneratedPluginRegistrant.swift;',
          )
          .replaceAll('sourceTree = "<group>";', 'sourceTree = SOURCE_ROOT;');
    }
    if (!migrationRequired && updatedLine != line) {
      logger.printStatus('Updating GeneratedPluginRegistrant paths in Xcode project.');
    }
    return updatedLine;
  }
}
