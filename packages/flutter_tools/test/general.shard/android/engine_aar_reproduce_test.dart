// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/android/gradle_errors.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/fake.dart';
import 'package:unified_analytics/unified_analytics.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fake_process_manager.dart';
import '../../src/fakes.dart';

void main() {
  group('Android engine AAR reproduction', () {
    late BufferLogger logger;
    late MemoryFileSystem fileSystem;
    late FakeProcessManager processManager;
    late FlutterProject project;
    late AndroidGradleBuilder builder;
    late String engineOutPath;
    late String localEngineHost;

    setUp(() {
      processManager = FakeProcessManager.empty();
      logger = BufferLogger.test();
      fileSystem = MemoryFileSystem.test();
      Cache.flutterRoot = '';
      engineOutPath = fileSystem.path.absolute('out', 'android_arm');
      localEngineHost = fileSystem.path.absolute('out', 'host_debug');

      // Set up the project.
      fileSystem.directory('android').childFile('build.gradle').createSync(recursive: true);
      fileSystem.directory('android').childFile('gradle.properties').createSync(recursive: true);
      fileSystem.directory('android').childDirectory('app').childFile('build.gradle')
        ..createSync(recursive: true)
        ..writeAsStringSync('apply from: irrelevant/flutter.gradle');

      project = FlutterProject.fromDirectoryTest(fileSystem.currentDirectory);
      final File manifestFile = project.android.appManifestFile;
      manifestFile.createSync(recursive: true);
      manifestFile.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <application>
        <meta-data android:name="flutterEmbedding" android:value="2" />
    </application>
</manifest>
''');

      fileSystem
          .directory('build')
          .childDirectory('app')
          .childDirectory('outputs')
          .childDirectory('flutter-apk')
          .childFile('app-release.apk')
          .createSync(recursive: true);

      final localEngineArtifacts = Artifacts.testLocalEngine(
        localEngine: engineOutPath,
        localEngineHost: localEngineHost,
        fileSystem: fileSystem,
      );

      builder = AndroidGradleBuilder(
        java: FakeJava(),
        logger: logger,
        processManager: processManager,
        fileSystem: fileSystem,
        artifacts: localEngineArtifacts,
        analytics: const NoOpAnalytics(),
        gradleUtils: FakeGradleUtils(),
        platform: FakePlatform(),
        androidStudio: FakeAndroidStudio(),
      );
    });

    void setUpLocalEngine({
      required bool provideAar,
      required bool provideJar,
      required bool provideAbiAar,
      required bool provideAbiJar,
    }) {
      // Embedding
      if (provideAar) {
        fileSystem
            .file(fileSystem.path.join(engineOutPath, 'flutter_embedding_release.aar'))
            .createSync(recursive: true);
      }
      if (provideJar) {
        fileSystem
            .file(fileSystem.path.join(engineOutPath, 'flutter_embedding_release.jar'))
            .createSync(recursive: true);
      }
      final File pomFile = fileSystem.file(
        fileSystem.path.join(engineOutPath, 'flutter_embedding_release.pom'),
      );
      pomFile.createSync(recursive: true);
      pomFile.writeAsStringSync('''
<?xml version="1.0" encoding="UTF-8"?>
<project>
  <version>1.0.0-12345</version>
</project>
''');
      fileSystem
          .file(fileSystem.path.join(engineOutPath, 'flutter_embedding_release.maven-metadata.xml'))
          .createSync(recursive: true);

      // ABI
      if (provideAbiAar) {
        fileSystem
            .file(fileSystem.path.join(engineOutPath, 'armeabi_v7a_release.aar'))
            .createSync(recursive: true);
      }
      if (provideAbiJar) {
        fileSystem
            .file(fileSystem.path.join(engineOutPath, 'armeabi_v7a_release.jar'))
            .createSync(recursive: true);
      }
      final File abiPom = fileSystem.file(
        fileSystem.path.join(engineOutPath, 'armeabi_v7a_release.pom'),
      );
      abiPom.createSync(recursive: true);
      fileSystem
          .file(fileSystem.path.join(engineOutPath, 'armeabi_v7a_release.maven-metadata.xml'))
          .createSync(recursive: true);
    }

    Directory getLocalEngineRepo() {
      return fileSystem.systemTempDirectory.listSync().whereType<Directory>().firstWhere(
        (Directory d) =>
            fileSystem.path.basename(d.path).startsWith('flutter_tool_local_engine_repo.'),
      );
    }

    testUsingContext('local engine repo symlinks AAR instead of JAR (modern)', () async {
      setUpLocalEngine(
        provideAar: true,
        provideJar: false,
        provideAbiAar: false,
        provideAbiJar: true,
      );

      processManager.addCommand(
        FakeCommand(
          command: <Pattern>[
            'gradlew',
            '-q',
            RegExp(r'-Plocal-engine-repo=.*'),
            '-Plocal-engine-build-mode=release',
            '-Plocal-engine-out=$engineOutPath',
            '-Plocal-engine-host-out=$localEngineHost',
            '-Ptarget-platform=android-arm',
            '-Ptarget=lib/main.dart',
            '-Pbase-application-name=android.app.Application',
            '-Pdart-obfuscation=false',
            '-Ptrack-widget-creation=false',
            '-Ptree-shake-icons=false',
            'assembleRelease',
          ],
        ),
      );

      await builder.buildGradleApp(
        project: project,
        androidBuildInfo: const AndroidBuildInfo(
          BuildInfo(
            BuildMode.release,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
        ),
        target: 'lib/main.dart',
        isBuildingBundle: false,
        configOnly: false,
        localGradleErrors: const <GradleHandledError>[],
      );

      final Directory repo = getLocalEngineRepo();
      expect(
        fileSystem
            .file(
              fileSystem.path.join(
                repo.path,
                'io/flutter/flutter_embedding_release/1.0.0-12345/flutter_embedding_release-1.0.0-12345.aar',
              ),
            )
            .existsSync(),
        isTrue,
      );
      expect(
        fileSystem
            .file(
              fileSystem.path.join(
                repo.path,
                'io/flutter/flutter_embedding_release/1.0.0-12345/flutter_embedding_release-1.0.0-12345.jar',
              ),
            )
            .existsSync(),
        isFalse,
      );
      expect(
        fileSystem
            .file(
              fileSystem.path.join(
                repo.path,
                'io/flutter/armeabi_v7a_release/1.0.0-12345/armeabi_v7a_release-1.0.0-12345.jar',
              ),
            )
            .existsSync(),
        isTrue,
      );
      expect(processManager, hasNoRemainingExpectations);
    });

    testUsingContext('local engine repo symlinks JAR if AAR is missing (legacy)', () async {
      setUpLocalEngine(
        provideAar: false,
        provideJar: true,
        provideAbiAar: false,
        provideAbiJar: true,
      );

      processManager.addCommand(
        FakeCommand(
          command: <Pattern>[
            'gradlew',
            '-q',
            RegExp(r'-Plocal-engine-repo=.*'),
            '-Plocal-engine-build-mode=release',
            '-Plocal-engine-out=$engineOutPath',
            '-Plocal-engine-host-out=$localEngineHost',
            '-Ptarget-platform=android-arm',
            '-Ptarget=lib/main.dart',
            '-Pbase-application-name=android.app.Application',
            '-Pdart-obfuscation=false',
            '-Ptrack-widget-creation=false',
            '-Ptree-shake-icons=false',
            'assembleRelease',
          ],
        ),
      );

      await builder.buildGradleApp(
        project: project,
        androidBuildInfo: const AndroidBuildInfo(
          BuildInfo(
            BuildMode.release,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
        ),
        target: 'lib/main.dart',
        isBuildingBundle: false,
        configOnly: false,
        localGradleErrors: const <GradleHandledError>[],
      );

      final Directory repo = getLocalEngineRepo();
      expect(
        fileSystem
            .file(
              fileSystem.path.join(
                repo.path,
                'io/flutter/flutter_embedding_release/1.0.0-12345/flutter_embedding_release-1.0.0-12345.jar',
              ),
            )
            .existsSync(),
        isTrue,
      );
      expect(
        fileSystem
            .file(
              fileSystem.path.join(
                repo.path,
                'io/flutter/flutter_embedding_release/1.0.0-12345/flutter_embedding_release-1.0.0-12345.aar',
              ),
            )
            .existsSync(),
        isFalse,
      );
      expect(processManager, hasNoRemainingExpectations);
    });

    testUsingContext('local engine repo symlinks both if both exist (transition)', () async {
      setUpLocalEngine(
        provideAar: true,
        provideJar: true,
        provideAbiAar: false,
        provideAbiJar: true,
      );

      processManager.addCommand(
        FakeCommand(
          command: <Pattern>[
            'gradlew',
            '-q',
            RegExp(r'-Plocal-engine-repo=.*'),
            '-Plocal-engine-build-mode=release',
            '-Plocal-engine-out=$engineOutPath',
            '-Plocal-engine-host-out=$localEngineHost',
            '-Ptarget-platform=android-arm',
            '-Ptarget=lib/main.dart',
            '-Pbase-application-name=android.app.Application',
            '-Pdart-obfuscation=false',
            '-Ptrack-widget-creation=false',
            '-Ptree-shake-icons=false',
            'assembleRelease',
          ],
        ),
      );

      await builder.buildGradleApp(
        project: project,
        androidBuildInfo: const AndroidBuildInfo(
          BuildInfo(
            BuildMode.release,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
        ),
        target: 'lib/main.dart',
        isBuildingBundle: false,
        configOnly: false,
        localGradleErrors: const <GradleHandledError>[],
      );

      final Directory repo = getLocalEngineRepo();
      expect(
        fileSystem
            .file(
              fileSystem.path.join(
                repo.path,
                'io/flutter/flutter_embedding_release/1.0.0-12345/flutter_embedding_release-1.0.0-12345.aar',
              ),
            )
            .existsSync(),
        isTrue,
      );
      expect(
        fileSystem
            .file(
              fileSystem.path.join(
                repo.path,
                'io/flutter/flutter_embedding_release/1.0.0-12345/flutter_embedding_release-1.0.0-12345.jar',
              ),
            )
            .existsSync(),
        isTrue,
      );
      expect(processManager, hasNoRemainingExpectations);
    });

    testUsingContext('local engine repo fails if both are missing', () async {
      setUpLocalEngine(
        provideAar: false,
        provideJar: false,
        provideAbiAar: false,
        provideAbiJar: true,
      );

      expect(
        () => builder.buildGradleApp(
          project: project,
          androidBuildInfo: const AndroidBuildInfo(
            BuildInfo(
              BuildMode.release,
              null,
              treeShakeIcons: false,
              packageConfigPath: '.dart_tool/package_config.json',
            ),
          ),
          target: 'lib/main.dart',
          isBuildingBundle: false,
          configOnly: false,
          localGradleErrors: const <GradleHandledError>[],
        ),
        throwsToolExit(
          message:
              'Neither flutter_embedding_release.aar nor flutter_embedding_release.jar was found in the local engine out directory.',
        ),
      );
      expect(processManager, hasNoRemainingExpectations);
    });

    testUsingContext('local engine repo fails if ABI artifacts are missing', () async {
      setUpLocalEngine(
        provideAar: true,
        provideJar: false,
        provideAbiAar: false,
        provideAbiJar: false,
      );

      expect(
        () => builder.buildGradleApp(
          project: project,
          androidBuildInfo: const AndroidBuildInfo(
            BuildInfo(
              BuildMode.release,
              null,
              treeShakeIcons: false,
              packageConfigPath: '.dart_tool/package_config.json',
            ),
          ),
          target: 'lib/main.dart',
          isBuildingBundle: false,
          configOnly: false,
          localGradleErrors: const <GradleHandledError>[],
        ),
        throwsToolExit(
          message:
              'Neither armeabi_v7a_release.aar nor armeabi_v7a_release.jar was found in the local engine out directory.',
        ),
      );
      expect(processManager, hasNoRemainingExpectations);
    });
  });
}

class FakeGradleUtils extends Fake implements GradleUtils {
  @override
  String getExecutable(FlutterProject project) {
    return 'gradlew';
  }
}
