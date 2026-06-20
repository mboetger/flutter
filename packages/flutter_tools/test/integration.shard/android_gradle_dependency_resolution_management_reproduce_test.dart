// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' show getGradlewFileName;
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('gradle_drm_reproduce_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext(
    'Flutter Gradle Plugin fails when dependencyResolutionManagement.repositoriesMode '
    'is FAIL_ON_PROJECT_REPOS',
    () async {
      // 1. Create a new flutter project.
      ProcessResult result = await processManager.run(<String>[
        flutterBin,
        'create',
        tempDir.path,
        '--project-name=testapp',
      ], workingDirectory: tempDir.path);
      expect(
        result.exitCode,
        0,
        reason: 'flutter create failed: ${result.stderr}\n${result.stdout}',
      );

      // 2. Run config-only build to generate gradle files.
      result = await processManager.run(<String>[
        flutterBin,
        'build',
        'apk',
        '--config-only',
      ], workingDirectory: tempDir.path);
      expect(
        result.exitCode,
        0,
        reason: 'flutter build apk --config-only failed: ${result.stderr}\n${result.stdout}',
      );

      final Directory androidDir = tempDir.childDirectory('android');
      final File buildGradleKts = androidDir.childFile('build.gradle.kts');
      final File settingsGradleKts = androidDir.childFile('settings.gradle.kts');

      // 3. Verify that they exist, and modify them.
      expect(buildGradleKts.existsSync(), isTrue);
      expect(settingsGradleKts.existsSync(), isTrue);

      // Modify build.gradle.kts to remove allprojects.repositories block.
      // This is because under FAIL_ON_PROJECT_REPOS, no project (including the root project)
      // is allowed to define repositories.
      String buildGradleContent = buildGradleKts.readAsStringSync().replaceAll('\r\n', '\n');
      const allProjectsPattern =
          'allprojects {\n    repositories {\n        google()\n        mavenCentral()\n    }\n}';

      // Guard assertion to ensure template structure matches expectations.
      expect(
        buildGradleContent,
        contains(allProjectsPattern),
        reason: 'The generated build.gradle.kts template structure has changed.',
      );

      buildGradleContent = buildGradleContent.replaceFirst(allProjectsPattern, '');
      buildGradleKts.writeAsStringSync(buildGradleContent, flush: true);

      // 4. Modify settings.gradle.kts to add dependencyResolutionManagement block
      // containing the flutter repository URL under FAIL_ON_PROJECT_REPOS mode.
      final String flutterRoot = getFlutterRoot();
      final File engineRealmFile = fileSystem.file(
        fileSystem.path.join(flutterRoot, 'bin', 'cache', 'engine.realm'),
      );
      var engineRealm = '';
      if (engineRealmFile.existsSync()) {
        engineRealm = engineRealmFile.readAsStringSync().trim();
        if (engineRealm.isNotEmpty) {
          engineRealm += '/';
        }
      }
      final String hostedRepository =
          platform.environment['FLUTTER_STORAGE_BASE_URL'] ?? 'https://storage.googleapis.com';
      final expectedRepoUrl = '$hostedRepository/${engineRealm}download.flutter.io';

      final String settingsGradleContent = settingsGradleKts.readAsStringSync().replaceAll(
        '\r\n',
        '\n',
      );
      final drmBlockWithFlutter =
          '''
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        maven {
            url = uri("$expectedRepoUrl")
        }
    }
}
''';
      settingsGradleKts.writeAsStringSync(
        '$settingsGradleContent\n$drmBlockWithFlutter',
        flush: true,
      );

      // 5. Run gradle tasks (like help) and verify that it SUCCEEDS under FAIL_ON_PROJECT_REPOS
      // because our plugin catches the registration exception and relies on settings repositories.
      final gradlew = '.${platform.pathSeparator}${getGradlewFileName(platform)}';
      result = await processManager.run(<String>[
        gradlew,
        ...getLocalEngineArguments(),
        'help',
      ], workingDirectory: androidDir.path);

      expect(
        result.exitCode,
        0,
        reason:
            'Gradle build failed under FAIL_ON_PROJECT_REPOS even though '
            'the repository was declared in settings.gradle.kts.\n'
            'stdout: ${result.stdout}\nstderr: ${result.stderr}',
      );
    },
  );
}
