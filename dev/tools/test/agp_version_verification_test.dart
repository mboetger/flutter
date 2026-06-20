// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';
import 'package:path/path.dart' as path;
import 'package:test/test.dart';

void main() {
  test('Verify all Android projects in dev/ use AGP >= 7.4.2 and Gradle >= 8.0.2', () {
    final Directory repoRoot = _findRepoRoot();
    final devDir = Directory(path.join(repoRoot.path, 'dev'));

    expect(devDir.existsSync(), isTrue, reason: 'Dev directory not found at ${devDir.path}');

    final failures = <String>[];

    // Find all gradle-wrapper.properties files, avoiding large build/generated directories
    final wrappers = <File>[];
    _findFiles(devDir, wrappers, (file) => file.path.endsWith('gradle-wrapper.properties'));

    expect(wrappers, isNotEmpty, reason: 'No gradle-wrapper.properties files found.');

    for (final wrapper in wrappers) {
      final String relativePath = path.relative(wrapper.path, from: devDir.path);
      final String content = wrapper.readAsStringSync();

      // Parse Gradle version
      // e.g. distributionUrl=https\://services.gradle.org/distributions/gradle-8.13-all.zip
      final Match? match = _gradleRegex.firstMatch(content);
      if (match == null) {
        failures.add('Could not parse Gradle version from $relativePath');
        continue;
      }
      final String gradleVersion = match.group(1)!;
      if (gradleVersion != 'REPLACEME') {
        if (!isVersionAtLeast(gradleVersion, '8.0.2')) {
          failures.add('Project $relativePath uses Gradle $gradleVersion, which is < 8.0.2');
        }
      }

      // Find the Gradle project root (which is the parent of the gradle/ directory or similar)
      // Usually wrapper path is .../[gradleProjectRoot]/gradle/wrapper/gradle-wrapper.properties
      // or .../[gradleProjectRoot]/android/gradle/wrapper/gradle-wrapper.properties
      final Directory projectRoot = wrapper.parent.parent.parent;

      // Search for build.gradle, build.gradle.kts, settings.gradle, settings.gradle.kts, libs.versions.toml
      final gradleFiles = <File>[];
      _findFiles(projectRoot, gradleFiles, (file) {
        final String name = path.basename(file.path);
        return name == 'build.gradle' ||
            name == 'build.gradle.kts' ||
            name == 'settings.gradle' ||
            name == 'settings.gradle.kts' ||
            name == 'libs.versions.toml';
      });

      for (final gradleFile in gradleFiles) {
        final String fileContent = gradleFile.readAsStringSync();
        final String fileRelativePath = path.relative(gradleFile.path, from: devDir.path);

        // Parse AGP versions
        final List<String> agpVersions = extractAgpVersions(fileContent, gradleFile.path);
        for (final agpVersion in agpVersions) {
          if (agpVersion != 'REPLACEME') {
            if (!isVersionAtLeast(agpVersion, '7.4.2')) {
              failures.add(
                'File $fileRelativePath specifies AGP version $agpVersion, which is < 7.4.2',
              );
            }
          }
        }
      }
    }

    if (failures.isNotEmpty) {
      fail(
        'AGP/Gradle version verification failed with the following errors:\n${failures.join('\n')}',
      );
    }
  });
}

// Regex definitions moved to top-level constants to avoid recompilation on every function call.
final RegExp _gradleRegex = RegExp(r'gradle-([A-Za-z0-9.-]+)-(all|bin)\.zip');
final RegExp _agpTomlRegex = RegExp(r'''(?:\bagp\b\s*=\s*["\']([^"\']+)["\'])''');
final RegExp _classpathRegex = RegExp(r'com\.android\.tools\.build:gradle:([0-9A-Za-z.-]+)');
final RegExp _pluginsRegex = RegExp(
  r'''id\s+["\']com\.android\.(?:application|library|test|dynamic-feature)["\']\s+version\s+["\']([0-9A-Za-z.-]+)["\']''',
);

// Robust Kotlin DSL plugin regex matching id("...") version "..." as well as id("...").version("...")
final RegExp _kotlinPluginsRegex = RegExp(
  r'''id\s*\(\s*["\']com\.android\.(?:application|library|test|dynamic-feature)["\']\s*\)\s*(?:\.|\s)\s*version\s*\(?\s*["\']([0-9A-Za-z.-]+)["\']\s*\)?''',
);

Directory _findRepoRoot() {
  Directory dir = Directory.current;
  while (dir.path != dir.parent.path) {
    if (Directory(path.join(dir.path, '.git')).existsSync() ||
        Directory(path.join(dir.path, 'dev', 'integration_tests')).existsSync()) {
      return dir;
    }
    dir = dir.parent;
  }
  throw StateError('Could not find repository root starting from ${Directory.current.path}');
}

// Custom recursive directory traversal helper that ignores huge build/generated directories.
void _findFiles(Directory dir, List<File> results, bool Function(File) filter) {
  if (!dir.existsSync()) {
    return;
  }
  for (final FileSystemEntity entity in dir.listSync(followLinks: false)) {
    if (entity is Directory) {
      final String name = path.basename(entity.path);
      // Skip common build, cache, and hidden directories to keep execution fast and hermetic.
      if (name == 'build' ||
          name == '.gradle' ||
          name == '.dart_tool' ||
          name == 'ios' ||
          name == 'macos' ||
          name == 'windows' ||
          name == 'linux' ||
          name == 'web' ||
          name.startsWith('.')) {
        continue;
      }
      _findFiles(entity, results, filter);
    } else if (entity is File) {
      if (filter(entity)) {
        results.add(entity);
      }
    }
  }
}

int? parseVersionPart(String part) {
  final digitPrefix = RegExp(r'^\d+');
  final String? match = digitPrefix.stringMatch(part);
  if (match == null) {
    return null;
  }
  return int.tryParse(match);
}

bool isVersionAtLeast(String version, String minimum) {
  final versionParts = <int>[for (final String s in version.split('.')) parseVersionPart(s) ?? 0];
  final minimumParts = <int>[for (final String s in minimum.split('.')) parseVersionPart(s) ?? 0];
  for (var i = 0; i < versionParts.length || i < minimumParts.length; i++) {
    final int v = i < versionParts.length ? versionParts[i] : 0;
    final int m = i < minimumParts.length ? minimumParts[i] : 0;
    if (v > m) {
      return true;
    }
    if (v < m) {
      return false;
    }
  }
  return true;
}

List<String> extractAgpVersions(String content, String filePath) {
  final versions = <String>[];

  // Strip comments to avoid matching commented-out versions
  final String cleanContent = content
      .replaceAll(RegExp(r'//.*'), '')
      .replaceAll(RegExp(r'/\*.*?\*/', dotAll: true), '');

  if (filePath.endsWith('libs.versions.toml')) {
    for (final Match match in _agpTomlRegex.allMatches(cleanContent)) {
      versions.add(match.group(1)!);
    }
  } else {
    for (final Match match in _classpathRegex.allMatches(cleanContent)) {
      versions.add(match.group(1)!);
    }
    for (final Match match in _pluginsRegex.allMatches(cleanContent)) {
      versions.add(match.group(1)!);
    }
    for (final Match match in _kotlinPluginsRegex.allMatches(cleanContent)) {
      versions.add(match.group(1)!);
    }
  }

  return versions;
}
