// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../base/project_migrator.dart';
import '../../base/version.dart';
import '../../project.dart';

/// Migrates the Kotlin Gradle Plugin version to 1.8.10 if the current version is less than 1.8.10.
class KotlinVersionMigrator extends ProjectMigrator {
  KotlinVersionMigrator(AndroidProject project, super.logger) : _project = project;

  final AndroidProject _project;

  // ext.kotlin_version = '1.7.10'
  static final _kotlinVersionPattern = RegExp(r'''ext\.kotlin_version\s*=\s*['"]([^'"]+)['"]''');

  // classpath "org.jetbrains.kotlin:kotlin-gradle-plugin:1.7.10"
  static final _kotlinClasspathPattern = RegExp(
    r'''classpath\s+['"]org\.jetbrains\.kotlin:kotlin-gradle-plugin:([^'"]+)['"]''',
  );

  // id "org.jetbrains.kotlin.android" version "1.7.10"
  static final _kotlinPluginPattern = RegExp(
    r'''id\s*\(?\s*['"]org\.jetbrains\.kotlin\.android['"]\s*\)?\s+version\s+['"]([^'"]+)['"]''',
  );

  @override
  Future<void> migrate() async {
    if (_project.isModule) {
      return;
    }
    if (_project.hostAppGradleFile.existsSync()) {
      processFileLines(_project.hostAppGradleFile);
    }
    if (_project.settingsGradleFile.existsSync()) {
      processFileLines(_project.settingsGradleFile);
    }
  }

  @override
  String migrateFileContents(String fileContents) {
    // 1. Match and replace ext.kotlin_version
    fileContents = fileContents.replaceAllMapped(_kotlinVersionPattern, (Match match) {
      final String fullLine = match.group(0)!;
      final String versionStr = match.group(1)!;
      final Version? version = Version.parse(versionStr);
      if (version != null && version < Version(1, 8, 10)) {
        return fullLine.replaceAll(versionStr, '1.8.10');
      }
      return fullLine;
    });

    // 2. Match and replace classpath org.jetbrains.kotlin:kotlin-gradle-plugin
    fileContents = fileContents.replaceAllMapped(_kotlinClasspathPattern, (Match match) {
      final String fullLine = match.group(0)!;
      final String versionStr = match.group(1)!;
      final Version? version = Version.parse(versionStr);
      if (version != null && version < Version(1, 8, 10)) {
        return fullLine.replaceAll(versionStr, '1.8.10');
      }
      return fullLine;
    });

    // 3. Match and replace id "org.jetbrains.kotlin.android" version
    fileContents = fileContents.replaceAllMapped(_kotlinPluginPattern, (Match match) {
      final String fullLine = match.group(0)!;
      final String versionStr = match.group(1)!;
      final Version? version = Version.parse(versionStr);
      if (version != null && version < Version(1, 8, 10)) {
        return fullLine.replaceAll(versionStr, '1.8.10');
      }
      return fullLine;
    });

    return fileContents;
  }
}
