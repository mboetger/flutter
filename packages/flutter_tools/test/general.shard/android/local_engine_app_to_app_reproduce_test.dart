// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' as gradle_utils;
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/utils.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/project.dart';

import '../../src/common.dart';
import '../../src/context.dart';

void main() {
  group('Local Engine Caching in local.properties', () {
    late Artifacts localEngineArtifacts;
    late FileSystem fs;

    setUp(() {
      fs = MemoryFileSystem.test();
      // Use 'android_debug_unopt_arm64' so that it correctly resolves to arm64/android-arm64.
      localEngineArtifacts = Artifacts.testLocalEngine(
        localEngine: 'out/android_debug_unopt_arm64',
        localEngineHost: 'out/host_debug_unopt',
      );
    });

    void testUsingAndroidContext(String description, dynamic Function() testMethod) {
      testUsingContext(
        description,
        testMethod,
        overrides: <Type, Generator>{
          Artifacts: () => localEngineArtifacts,
          Platform: () => FakePlatform(),
          FileSystem: () => fs,
          ProcessManager: () => FakeProcessManager.any(),
        },
      );
    }

    String? propertyFor(String key, File file) {
      if (!file.existsSync()) {
        return null;
      }
      return SettingsFile.parseFromFile(file).values[key];
    }

    testUsingAndroidContext('writes local engine properties to local.properties', () async {
      // 1. Create a fake local engine directory with necessary files for POM/Jar resolution
      final Directory localEngineOut = fs.directory('out/android_debug_unopt_arm64')
        ..createSync(recursive: true);
      fs.directory('out/host_debug_unopt').createSync(recursive: true);

      // Create fake POM, JAR, and metadata files
      localEngineOut.childFile('flutter_embedding_debug.pom').writeAsStringSync('''
<?xml version="1.0" encoding="UTF-8"?>
<project>
  <version>1.0.0-mock</version>
</project>
''');
      localEngineOut.childFile('flutter_embedding_debug.jar').createSync();
      localEngineOut
          .childFile('flutter_embedding_debug.maven-metadata.xml')
          .writeAsStringSync('<metadata></metadata>');

      localEngineOut.childFile('arm64_v8a_debug.pom').writeAsStringSync('''
<?xml version="1.0" encoding="UTF-8"?>
<project>
  <version>1.0.0-mock</version>
</project>
''');
      localEngineOut.childFile('arm64_v8a_debug.jar').createSync();
      localEngineOut
          .childFile('arm64_v8a_debug.maven-metadata.xml')
          .writeAsStringSync('<metadata></metadata>');

      // 2. Create the fake project structure
      final Directory projectDir = fs.directory('path/to/project');
      final File pubspecFile = projectDir.childFile('pubspec.yaml')..createSync(recursive: true);
      pubspecFile.writeAsStringSync('name: test_project\n');

      final File localPropertiesFile =
          projectDir.childDirectory('android').childFile('local.properties')
            ..createSync(recursive: true);

      // 3. Call updateLocalProperties
      gradle_utils.updateLocalProperties(
        project: FlutterProject.fromDirectoryTest(projectDir),
        buildInfo: const BuildInfo(
          BuildMode.debug,
          null,
          treeShakeIcons: false,
          packageConfigPath: '.dart_tool/package_config.json',
        ),
        requireAndroidSdk: false,
      );

      // 4. Verify local.properties contents
      expect(
        propertyFor('local-engine-repo', localPropertiesFile),
        fs.path.join(
          'path/to/project',
          '.dart_tool',
          'flutter_tool',
          'local_engine_repo',
          'android_debug_unopt_arm64',
        ),
      );
      expect(propertyFor('local-engine-build-mode', localPropertiesFile), 'debug');
      expect(propertyFor('local-engine-out', localPropertiesFile), 'out/android_debug_unopt_arm64');
      expect(propertyFor('local-engine-host-out', localPropertiesFile), 'out/host_debug_unopt');
      expect(propertyFor('target-platform', localPropertiesFile), 'android-arm64');

      // 5. Verify the stable local engine repo directory and files exist
      final Directory stableRepo = fs.directory(
        fs.path.join(
          'path/to/project',
          '.dart_tool',
          'flutter_tool',
          'local_engine_repo',
          'android_debug_unopt_arm64',
        ),
      );
      expect(stableRepo.existsSync(), isTrue);

      final File embeddingPom = stableRepo
          .childDirectory('io')
          .childDirectory('flutter')
          .childDirectory('flutter_embedding_debug')
          .childDirectory('1.0.0-mock')
          .childFile('flutter_embedding_debug-1.0.0-mock.pom');
      expect(embeddingPom.existsSync(), isTrue);
    });
  });
}
