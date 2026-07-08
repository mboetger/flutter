// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../base/file_system.dart';
import '../../base/project_migrator.dart';
import '../../project.dart';

const String _androidxFlagText = 'android.useAndroidX=true';

/// Migrate to enable AndroidX if it is missing from gradle.properties.
///
/// If it is explicitly set to false, this migrator does nothing, and the build
/// will fail later with a validation error.
class AndroidXMigration extends ProjectMigrator {
  AndroidXMigration(AndroidProject project, super.logger)
    : _gradlePropertiesFile = project.hostAppGradleRoot.childFile('gradle.properties');

  final File _gradlePropertiesFile;

  @override
  Future<void> migrate() async {
    if (!_gradlePropertiesFile.existsSync()) {
      logger.printTrace(
        'The gradle.properties file was not found. Creating it with $_androidxFlagText.',
      );
      try {
        await _gradlePropertiesFile.writeAsString('$_androidxFlagText\n');
      } on FileSystemException catch (e) {
        logger.printError('Failed to write to the gradle.properties during migration: $e');
      }
      return;
    }

    String contents;
    try {
      contents = await _gradlePropertiesFile.readAsString();
    } on FileSystemException catch (e) {
      logger.printError('Failed to read gradle.properties during migration: $e');
      return;
    }

    // Skip migration if the AndroidX flag already exists (either true or false).
    // If it is false, we want to fail later, not overwrite it here.
    if (contents.contains('android.useAndroidX')) {
      return;
    }

    processFileLines(_gradlePropertiesFile);
  }

  @override
  String migrateFileContents(String fileContents) {
    logger.printTrace('Migrating gradle.properties to enable AndroidX.');

    final propertyToAppend = StringBuffer();
    propertyToAppend.writeln(_androidxFlagText);

    final prefix = fileContents.isEmpty || fileContents.endsWith('\n') ? '' : '\n';
    return '$fileContents$prefix$propertyToAppend';
  }
}
