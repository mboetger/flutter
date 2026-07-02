// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:xml/xml.dart';

import '../../base/file_system.dart';
import '../../base/project_migrator.dart';
import '../../project.dart';

/// Migrate an Android project from V1 embedding to V2 embedding.
class AndroidEmbeddingMigration extends ProjectMigrator {
  AndroidEmbeddingMigration(this._project, super.logger);

  final AndroidProject _project;

  @override
  Future<void> migrate() async {
    if (!_project.appManifestFile.existsSync()) {
      logger.printTrace('AndroidManifest.xml not found, skipping Android embedding migration.');
      return;
    }

    logger.printTrace('Running AndroidEmbeddingMigration on ${_project.hostAppGradleRoot.path}');

    _migrateManifest();
    _migrateMainActivity();
  }

  void _migrateManifest() {
    final File manifestFile = _project.appManifestFile;
    String manifestContent = manifestFile.readAsStringSync();

    // 1. Remove android:name="io.flutter.app.FlutterApplication"
    manifestContent = manifestContent.replaceAll(
      RegExp(r'''\s*android:name\s*=\s*["']io\.flutter\.app\.FlutterApplication["']'''),
      '',
    );

    // 2. Update or insert flutterEmbedding metadata
    final embeddingRegExp = RegExp(
      r'''(<meta-data\s+[^>]*android:name\s*=\s*["']flutterEmbedding["']\s+[^>]*android:value\s*=\s*["'])(1)(["'][^>]*/>)''',
      dotAll: true,
    );
    final embeddingRegExpReverse = RegExp(
      r'''(<meta-data\s+[^>]*android:value\s*=\s*["'])(1)(["']\s+[^>]*android:name\s*=\s*["']flutterEmbedding["'][^>]*/>)''',
      dotAll: true,
    );

    if (manifestContent.contains(embeddingRegExp)) {
      manifestContent = manifestContent.replaceFirstMapped(
        embeddingRegExp,
        (Match match) => '${match.group(1)}2${match.group(3)}',
      );
    } else if (manifestContent.contains(embeddingRegExpReverse)) {
      manifestContent = manifestContent.replaceFirstMapped(
        embeddingRegExpReverse,
        (Match match) => '${match.group(1)}2${match.group(3)}',
      );
    } else if (!manifestContent.contains('flutterEmbedding')) {
      // Insert it inside <application> tag
      final applicationRegExp = RegExp(r'(<application\b[^>]*>)');
      if (manifestContent.contains(applicationRegExp)) {
        manifestContent = manifestContent.replaceFirstMapped(
          applicationRegExp,
          (Match match) => '${match.group(0)}\n        <meta-data\n            android:name="flutterEmbedding"\n            android:value="2" />',
        );
      }
    }

    // Validate the resulting XML
    try {
      XmlDocument.parse(manifestContent);
    } on XmlException {
      logger.printTrace('AndroidManifest.xml migration resulted in invalid XML, skipping write.');
      return;
    }

    manifestFile.writeAsStringSync(manifestContent);
  }

  void _migrateMainActivity() {
    final File manifestFile = _project.appManifestFile;
    if (!manifestFile.existsSync()) {
      return;
    }

    XmlDocument document;
    try {
      document = XmlDocument.parse(manifestFile.readAsStringSync());
    } on XmlException {
      return;
    }

    final String? packageName = document.rootElement.getAttribute('package');
    String? activityName;
    String? fallbackActivityName;

    for (final XmlElement activity in document.findAllElements('activity')) {
      final String? name = activity.getAttribute('android:name');
      if (name != null) {
        fallbackActivityName ??= name;
        var isLauncher = false;
        for (final XmlElement intentFilter in activity.findElements('intent-filter')) {
          var hasMain = false;
          var hasLauncher = false;
          for (final XmlElement action in intentFilter.findElements('action')) {
            if (action.getAttribute('android:name') == 'android.intent.action.MAIN') {
              hasMain = true;
            }
          }
          for (final XmlElement category in intentFilter.findElements('category')) {
            if (category.getAttribute('android:name') == 'android.intent.category.LAUNCHER') {
              hasLauncher = true;
            }
          }
          if (hasMain && hasLauncher) {
            isLauncher = true;
            break;
          }
        }
        if (isLauncher) {
          activityName = name;
          break;
        }
      }
    }

    activityName ??= fallbackActivityName;

    if (activityName == null || packageName == null) {
      return;
    }

    if (activityName.startsWith('.')) {
      activityName = '$packageName$activityName';
    } else if (!activityName.contains('.')) {
      activityName = '$packageName.$activityName';
    }

    final List<String> pathParts = activityName.split('.');
    final String relativePath = _project.hostAppGradleRoot.fileSystem.path.joinAll(pathParts);

    final possiblePaths = <String>[
      _project.hostAppGradleRoot.fileSystem.path.join('java', '$relativePath.java'),
      _project.hostAppGradleRoot.fileSystem.path.join('java', '$relativePath.kt'),
      _project.hostAppGradleRoot.fileSystem.path.join('kotlin', '$relativePath.java'),
      _project.hostAppGradleRoot.fileSystem.path.join('kotlin', '$relativePath.kt'),
    ];

    final Directory mainSrcDir = _project.hostAppGradleRoot
        .childDirectory('app')
        .childDirectory('src')
        .childDirectory('main');

    for (final path in possiblePaths) {
      final File file = mainSrcDir.childFile(path);
      if (file.existsSync()) {
        try {
          _migrateActivityFile(file);
        } on Exception catch (e) {
          logger.printTrace('Failed to migrate activity file ${file.path}: $e');
        }
      }
    }
  }

  void _migrateActivityFile(File file) {
    String content = file.readAsStringSync();

    // Replace the import. Semicolons (if present in Java) are preserved.
    content = content.replaceAll(
      RegExp(r'import\s+io\.flutter\.app\.FlutterActivity'),
      'import io.flutter.embedding.android.FlutterActivity',
    );

    // Removes GeneratedPluginRegistrant import (handles both Java and Kotlin)
    content = content.replaceAll(
      RegExp(r'import\s+io\.flutter\.plugins\.GeneratedPluginRegistrant[ \t]*;?[ \t]*\n?'),
      '',
    );

    // Removes the registerWith call without merging lines
    content = content.replaceAll(
      RegExp(r'[ \t]*GeneratedPluginRegistrant\.registerWith[ \t]*\([ \t]*this[ \t]*\)[ \t]*;?[ \t]*\n?'),
      '',
    );

    file.writeAsStringSync(content);
  }

  @override
  String migrateFileContents(String fileContents) {
    return fileContents;
  }
}
