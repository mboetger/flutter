// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_studio.dart';
import 'package:flutter_tools/src/android/java.dart';
import 'package:flutter_tools/src/android/gradle_errors.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/version.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:flutter_tools/src/globals.dart' as globals;
import 'package:test/test.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fake_process_manager.dart';

void main() {
  late MemoryFileSystem fileSystem;
  late FakeProcessManager processManager;

  setUp(() {
    fileSystem = MemoryFileSystem.test();
    processManager = FakeProcessManager.empty();
  });

  group('reproduce issue 91696', () {
    // Test 1: Android Studio Java path fallback
    testUsingContext(
      'Android Studio fallback when version is unknown and jbr does not exist but jre does',
      () {
        const String studioPath = '/opt/android-studio';
        fileSystem.directory(studioPath).createSync(recursive: true);
        
        // Only jre exists, not jbr
        final String jreJavaHome = fileSystem.path.join(studioPath, 'jre');
        final String jreJavaExecutable = fileSystem.path.join(jreJavaHome, 'bin', 'java');
        fileSystem.file(jreJavaExecutable).createSync(recursive: true);

        processManager.addCommand(
          FakeCommand(
            command: <String>[jreJavaExecutable, '-version'],
            stderr: 'openjdk version "11.0.18"',
          ),
        );

        // We construct AndroidStudio with unknown version
        final AndroidStudio studio = AndroidStudio(
          studioPath,
          version: null,
        );

        // Expected: it should have successfully found Java in 'jre' as fallback
        expect(studio.isValid, isTrue);
        expect(studio.javaPath, equals(jreJavaHome));
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => processManager,
        Platform: () => FakePlatform(
          operatingSystem: 'linux',
          environment: <String, String>{'PATH': ''},
        ),
      },
    );

    // Test 2: JVM version selection based on Gradle version
    testUsingContext(
      'Select JVM version based on project Gradle version',
      () async {
        // Setup a mock project that uses Gradle 7.0.0
        final Directory projectDir = fileSystem.currentDirectory;
        final File gradleWrapperFile = projectDir
            .childDirectory('android')
            .childDirectory('gradle')
            .childDirectory('wrapper')
            .childFile('gradle-wrapper.properties');
        gradleWrapperFile.createSync(recursive: true);
        gradleWrapperFile.writeAsStringSync('''
distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
distributionUrl=https\\://services.gradle.org/distributions/gradle-7.0.2-all.zip
''');

        // Setup a build.gradle to specify AGP version 7.0.0
        final File buildGradleFile = projectDir
            .childDirectory('android')
            .childFile('build.gradle');
        buildGradleFile.createSync(recursive: true);
        buildGradleFile.writeAsStringSync('''
dependencies {
    classpath 'com.android.tools.build:gradle:7.0.0'
}
''');

        // Setup two candidates:
        // 1. Android Studio bundled Java (version 1.8 - incompatible with Gradle 7.0.0)
        final String studioPath = '/opt/android-studio';
        final String studioJavaHome = fileSystem.path.join(studioPath, 'jre');
        final String studioJavaExecutable = fileSystem.path.join(studioJavaHome, 'bin', 'java');
        fileSystem.file(studioJavaExecutable).createSync(recursive: true);

        // 2. JAVA_HOME Java (version 11.0.18 - compatible with Gradle 7.0.2)
        const String javaHomePath = '/usr/lib/jvm/java-11';
        final String javaHomeExecutable = fileSystem.path.join(javaHomePath, 'bin', 'java');
        fileSystem.file(javaHomeExecutable).createSync(recursive: true);

        processManager.addCommands(<FakeCommand>[
          FakeCommand(
            command: <String>[studioJavaExecutable, '-version'],
            stderr: 'java version "1.8.0_202"',
          ),
          FakeCommand(
            command: <String>[studioJavaExecutable, '--version'],
            stdout: 'java version "1.8.0_202"',
          ),
          FakeCommand(
            command: <String>[javaHomeExecutable, '--version'],
            stdout: 'openjdk 11.0.18',
          ),
        ]);

        final AndroidStudio androidStudio = AndroidStudio(
          studioPath,
          version: Version(4, 0, 0), // Old AS version that bundles Java 8
        );

        final Platform platform = FakePlatform(
          operatingSystem: 'linux',
          environment: <String, String>{
            'PATH': '',
            'JAVA_HOME': javaHomePath,
          },
        );

        // Resolve Java
        final Java? resolvedJava = Java.find(
          config: globals.config,
          androidStudio: androidStudio,
          logger: globals.logger,
          fileSystem: fileSystem,
          platform: platform,
          processManager: processManager,
        );

        // Expected: Since the project requires Java 11 (due to Gradle 7.0.2),
        // and Android Studio bundles Java 8 (incompatible),
        // we should select Java 11 from JAVA_HOME instead of Java 8 from Android Studio.
        expect(resolvedJava, isNotNull);
        expect(resolvedJava!.version, equals(Version(11, 0, 18)));
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => processManager,
        Platform: () => FakePlatform(operatingSystem: 'linux'),
      },
    );

    // Test 3: Custom Gradle incompatibility error message
    testUsingContext(
      'incompatible java and gradle versions custom error message',
      () async {
        const String errorMessage = '''
Could not compile build file '…/example/android/build.gradle'.
> startup failed:
  General error during conversion: Unsupported class file major version 61
  java.lang.IllegalArgumentException: Unsupported class file major version 61
''';

        await incompatibleJavaAndGradleVersionsHandler.handler(
          line: errorMessage,
          project: FlutterProject.fromDirectoryTest(fileSystem.currentDirectory),
          usesAndroidX: true,
        );

        // Ensure the error notes the new custom user-friendly error message format
        final String normalizedStatusText = testLogger.statusText
            .replaceAll(RegExp(r'[┌─┐│└┘]'), '')
            .replaceAll(RegExp(r'\s+'), ' ');
        expect(
          normalizedStatusText,
          contains('Your build failed because you are using a version of Java that is incompatible with the Gradle version used in the current project.'),
        );
        expect(
          normalizedStatusText,
          contains('To fix this problem, go to https://'),
        );
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => processManager,
        Platform: () => FakePlatform(operatingSystem: 'linux'),
      },
    );
  });
}
