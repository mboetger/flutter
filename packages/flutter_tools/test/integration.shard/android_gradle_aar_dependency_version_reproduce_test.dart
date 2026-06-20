// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@Timeout(Duration(minutes: 5))
library;

import 'package:collection/collection.dart';
import 'package:file/file.dart';
import 'package:flutter_tools/src/base/io.dart';
import 'package:xml/xml.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('aar_dep_version_reproduce_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext(
    'generated AAR POM file of a module does not overwrite dependency plugin versions with the module build number',
    () async {
      final Directory testPluginDir = tempDir.childDirectory('test_plugin');
      final Directory testModuleDir = tempDir.childDirectory('test_module');

      // 1. Create a plugin project. Pass --no-pub to avoid redundant pub get.
      ProcessResult result = await processManager.run(<String>[
        flutterBin,
        'create',
        '--template=plugin',
        '--platforms=android',
        '--project-name=test_plugin',
        '--no-pub',
        testPluginDir.path,
      ], workingDirectory: tempDir.path);
      expect(
        result.exitCode,
        0,
        reason: 'Failed to create plugin: ${result.stderr}\n${result.stdout}',
      );

      // Modify the plugin's version to 1.2.3 in its pubspec.yaml.
      final File pluginPubspec = testPluginDir.childFile('pubspec.yaml');
      final String pluginPubspecContent = pluginPubspec.readAsStringSync();
      final String updatedPluginPubspecContent = pluginPubspecContent.replaceFirst(
        RegExp(r'version:\s+\S+'),
        'version: 1.2.3',
      );
      expect(
        updatedPluginPubspecContent,
        isNot(pluginPubspecContent),
        reason: 'Failed to update version in plugin pubspec.yaml',
      );
      pluginPubspec.writeAsStringSync(updatedPluginPubspecContent, flush: true);

      // Modify the plugin's version in its android/build.gradle or build.gradle.kts.
      // Since the Flutter tool does not automatically propagate pubspec.yaml version to Gradle,
      // the plugin's version in Gradle is determined by its build file (defaulting to 1.0-SNAPSHOT).
      // We must update the build file to use '1.2.3' to ensure we can verify it is not overridden.
      final File buildGradle = testPluginDir.childDirectory('android').childFile('build.gradle');
      final File buildGradleKts = testPluginDir
          .childDirectory('android')
          .childFile('build.gradle.kts');
      final targetBuildFile = buildGradle.existsSync() ? buildGradle : buildGradleKts;

      expect(targetBuildFile.existsSync(), isTrue, reason: 'Plugin Android build file not found');
      final String buildContent = targetBuildFile.readAsStringSync();
      final String updatedBuildContent = buildContent.replaceFirst(
        RegExp(r'''version\s*=\s*['"]\S+['"]'''),
        'version = "1.2.3"',
      );
      expect(
        updatedBuildContent,
        isNot(buildContent),
        reason: 'Failed to update version in plugin build file',
      );
      targetBuildFile.writeAsStringSync(updatedBuildContent, flush: true);

      // 2. Create a module project. Pass --no-pub to avoid redundant pub get.
      result = await processManager.run(<String>[
        flutterBin,
        'create',
        '--template=module',
        '--project-name=test_module',
        '--no-pub',
        testModuleDir.path,
      ], workingDirectory: tempDir.path);
      expect(
        result.exitCode,
        0,
        reason: 'Failed to create module: ${result.stderr}\n${result.stdout}',
      );

      // 3. Add the plugin as a dependency in the module's pubspec.yaml.
      final File modulePubspec = testModuleDir.childFile('pubspec.yaml');
      final String modulePubspecContent = modulePubspec.readAsStringSync();
      final String updatedModulePubspecContent = modulePubspecContent.replaceFirst(
        'dependencies:',
        'dependencies:\n  test_plugin:\n    path: ../test_plugin',
      );
      expect(
        updatedModulePubspecContent,
        isNot(modulePubspecContent),
        reason: 'Failed to add plugin dependency to module pubspec.yaml',
      );
      modulePubspec.writeAsStringSync(updatedModulePubspecContent, flush: true);

      // 4. Build AAR for the module with a specific build number.
      // This will trigger a single, necessary pub get offline.
      const buildNumber = '2.5.7';
      result = await processManager.run(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'build',
        'aar',
        '--build-number=$buildNumber',
      ], workingDirectory: testModuleDir.path);
      expect(result.exitCode, 0, reason: 'Failed to build AAR: ${result.stderr}\n${result.stdout}');

      // 5. Locate the generated POM file for the module under build/host/outputs/repo.
      final Directory repoDir = testModuleDir
          .childDirectory('build')
          .childDirectory('host')
          .childDirectory('outputs')
          .childDirectory('repo');

      expect(repoDir.existsSync(), isTrue, reason: 'Repo directory does not exist');

      // Find all .pom files recursively.
      final List<File> pomFiles = repoDir
          .listSync(recursive: true)
          .whereType<File>()
          .where((File file) => file.path.endsWith('.pom'))
          .toList();

      expect(pomFiles, isNotEmpty, reason: 'No POM files found in repo');

      // Find the POM file of the module.
      final File? modulePomFile = pomFiles.firstWhereOrNull(
        (File file) => file.basename == 'flutter_release-$buildNumber.pom',
      );

      expect(
        modulePomFile,
        isNotNull,
        reason:
            'Could not find module POM file (flutter_release-$buildNumber.pom) in: ${pomFiles.map((f) => f.path)}',
      );

      // Parse the POM file and check the dependency version for test_plugin.
      final String pomContent = modulePomFile!.readAsStringSync();
      final document = XmlDocument.parse(pomContent);

      final Iterable<XmlElement> dependencies = document.findAllElements('dependency');
      expect(dependencies, isNotEmpty, reason: 'No dependencies found in module POM');

      XmlElement? testPluginDependency;
      for (final dependency in dependencies) {
        final String? artifactId = dependency.findElements('artifactId').firstOrNull?.innerText;
        if (artifactId != null && artifactId.startsWith('test_plugin')) {
          testPluginDependency = dependency;
          break;
        }
      }

      expect(
        testPluginDependency,
        isNotNull,
        reason: 'Could not find test_plugin dependency in POM file:\n$pomContent',
      );

      final String? dependencyVersion = testPluginDependency!
          .findElements('version')
          .firstOrNull
          ?.innerText;

      expect(
        dependencyVersion,
        isNotNull,
        reason: 'Dependency version element not found in dependency: $testPluginDependency',
      );

      // The dependency version must be the plugin's package version (1.2.3), NOT the parent module's build number (2.5.7).
      expect(
        dependencyVersion,
        '1.2.3',
        reason: 'Dependency version was overridden by the build number',
      );
    },
  );
}
