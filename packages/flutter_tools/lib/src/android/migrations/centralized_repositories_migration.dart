// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../base/file_system.dart';
import '../../base/project_migrator.dart';
import '../../project.dart';

/// Migrate from legacy top-level repositories declarations (allprojects) to
/// centralized repositories in settings.gradle/settings.gradle.kts.
class CentralizedRepositoriesMigration extends ProjectMigrator {
  CentralizedRepositoriesMigration(AndroidProject project, super.logger)
    : _buildGradleFile = project.hostAppGradleRoot.childFile('build.gradle'),
      _buildGradleKtsFile = project.hostAppGradleRoot.childFile('build.gradle.kts'),
      _settingsGradleFile = project.hostAppGradleRoot.childFile('settings.gradle'),
      _settingsGradleKtsFile = project.hostAppGradleRoot.childFile('settings.gradle.kts');

  final File _buildGradleFile;
  final File _buildGradleKtsFile;
  final File _settingsGradleFile;
  final File _settingsGradleKtsFile;

  static final RegExp legacyAllprojectsBlockRegex = RegExp(
    r'allprojects\s*\{\s*repositories\s*\{\s*google\(\s*\)\s*mavenCentral\(\s*\)\s*\}\s*\}\s*',
    multiLine: true,
  );

  static const String dependencyResolutionManagementBlock = r'''

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.PREFER_SETTINGS)
    repositories {
        google()
        mavenCentral()
    }
}
''';

  @override
  Future<void> migrate() async {
    if (_buildGradleFile.existsSync()) {
      processFileLines(_buildGradleFile);
    }
    if (_buildGradleKtsFile.existsSync()) {
      processFileLines(_buildGradleKtsFile);
    }

    if (_settingsGradleFile.existsSync()) {
      await _migrateSettingsFile(_settingsGradleFile);
    } else if (_settingsGradleKtsFile.existsSync()) {
      await _migrateSettingsFile(_settingsGradleKtsFile);
    }
  }

  Future<void> _migrateSettingsFile(File file) async {
    String contents;
    try {
      contents = await file.readAsString();
    } on FileSystemException catch (e) {
      logger.printError('Failed to read ${file.basename} during migration: $e');
      return;
    }

    if (contents.contains('dependencyResolutionManagement')) {
      return;
    }

    logger.printTrace('Adding dependencyResolutionManagement to ${file.basename}.');
    final prefix = contents.isEmpty || contents.endsWith('\n') ? '' : '\n';
    try {
      await file.writeAsString('$contents$prefix$dependencyResolutionManagementBlock');
    } on FileSystemException catch (e) {
      logger.printError('Failed to write ${file.basename} during migration: $e');
    }
  }

  @override
  String migrateFileContents(String fileContents) {
    if (fileContents.contains(legacyAllprojectsBlockRegex)) {
      logger.printTrace('Removing legacy allprojects block from build file.');
      return fileContents.replaceAll(legacyAllprojectsBlockRegex, '');
    }
    return fileContents;
  }
}
