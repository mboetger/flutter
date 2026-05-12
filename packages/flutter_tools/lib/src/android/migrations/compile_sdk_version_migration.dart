// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:meta/meta.dart';

import '../../base/project_migrator.dart';
import '../../project.dart';

@visibleForTesting
const replacementCompileSdkText = 'compileSdkVersion flutter.compileSdkVersion';

@visibleForTesting
const groovyReplacementWithEquals = 'compileSdkVersion = flutter.compileSdkVersion';

@visibleForTesting
const kotlinReplacementCompileSdkText = 'compileSdk = flutter.compileSdkVersion';

@visibleForTesting
const appGradleNotFoundWarning =
    'Module level build.gradle file not found, skipping compileSdkVersion migration.';

/// This matches compileSdkVersion lines in the module-level build.gradle
/// file which have compileSdkVersion or compileSdk followed by any integer
/// set with space syntax or equals syntax.
final compileSdkVersionEqualsMatch = RegExp(
  r'(?<=^\s*)compileSdk(Version)?\s*=\s*(\d+)(?=\s*(?://|$))',
  multiLine: true,
);

final compileSdkVersionSpaceMatch = RegExp(
  r'(?<=^\s*)compileSdk(Version)?\s+(\d+)(?=\s*(?://|$))',
  multiLine: true,
);

class CompileSdkVersionMigration extends ProjectMigrator {
  CompileSdkVersionMigration(AndroidProject project, super.logger) : _project = project;

  final AndroidProject _project;

  @override
  Future<void> migrate() async {
    // Skip applying migration in modules as the FlutterExtension is not applied.
    if (_project.isModule) {
      return;
    }
    if (!_project.appGradleFile.existsSync()) {
      // Skip if we cannot find the app level build.gradle file.
      logger.printTrace(appGradleNotFoundWarning);
      return;
    }
    processFileLines(_project.appGradleFile);
  }

  @override
  String migrateFileContents(String fileContents) {
    if (_project.appGradleFile.path.endsWith('.kts')) {
      // For Kotlin Gradle files, only the equals syntax is valid and we should use 'compileSdk'.
      return fileContents.replaceAll(compileSdkVersionEqualsMatch, kotlinReplacementCompileSdkText);
    }

    // For Groovy Gradle files, both space and equals syntax are valid, and the property name is 'compileSdkVersion'.
    return fileContents
        .replaceAll(compileSdkVersionSpaceMatch, replacementCompileSdkText)
        .replaceAll(compileSdkVersionEqualsMatch, groovyReplacementWithEquals);
  }
}
