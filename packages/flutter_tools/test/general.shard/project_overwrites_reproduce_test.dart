// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/test.dart';

import '../src/common.dart';
import '../src/context.dart';

void main() {
  group('AndroidProject version overwrites', () {
    late MemoryFileSystem fileSystem;

    setUp(() {
      fileSystem = MemoryFileSystem.test();
      Cache.flutterRoot = '/flutter';
    });

    testUsingContext(
      'overrides commonly modified template values in generated files',
      () async {
        final Directory projectDir = fileSystem.directory('/project')..createSync(recursive: true);
        projectDir.childFile('pubspec.yaml').writeAsStringSync('''
name: test_project
''');

        final FlutterProject project = FlutterProject.fromDirectory(projectDir);
        final AndroidProject androidProject = project.android;

        // Set up mock versions of the gradle/wrapper files as they would be after `flutter create`
        final File gradleWrapperProperties = androidProject.gradleWrapperPropertiesFile;
        gradleWrapperProperties.createSync(recursive: true);
        gradleWrapperProperties.writeAsStringSync(r'''
distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
distributionUrl=https\://services.gradle.org/distributions/gradle-9.1.0-all.zip
''');

        final File settingsGradle = androidProject.settingsGradleFile;
        settingsGradle.createSync(recursive: true);
        settingsGradle.writeAsStringSync('''
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
plugins {
    id("dev.flutter.flutter-plugin-loader") version "1.0.0"
    id("com.android.application") version "9.0.1" apply false
    id("org.jetbrains.kotlin.android") version "2.3.20" apply false
}
include(":app")
''');

        // 1. Call the override mechanism.
        await androidProject.overwriteVersionAndTemplateValues(
          gradleVersion: '8.5',
          agpVersion: '8.2.0',
          kotlinVersion: '1.9.20',
        );

        // 2. Verify that the files contain the overridden values.
        expect(gradleWrapperProperties.readAsStringSync(), contains('gradle-8.5-all.zip'));
        expect(
          settingsGradle.readAsStringSync(),
          contains('id("com.android.application") version "8.2.0"'),
        );
        expect(
          settingsGradle.readAsStringSync(),
          contains('id("org.jetbrains.kotlin.android") version "1.9.20"'),
        );
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => FakeProcessManager.any(),
      },
    );

    testUsingContext(
      'overrides alternative and legacy plugin styles, and respects directory exclusions',
      () async {
        final Directory projectDir = fileSystem.directory('/project')..createSync(recursive: true);
        projectDir.childFile('pubspec.yaml').writeAsStringSync('name: test_project\n');

        final FlutterProject project = FlutterProject.fromDirectory(projectDir);
        final AndroidProject androidProject = project.android;

        // 1. Root build.gradle using imperative styles and legacy Kotlin variable declarations
        final File rootBuildGradle = androidProject.hostAppGradleFile;
        rootBuildGradle.createSync(recursive: true);
        rootBuildGradle.writeAsStringSync('''
buildscript {
    ext.kotlin_version = '1.7.0'
    dependencies {
        classpath 'com.android.tools.build:gradle:7.3.0'
        classpath "org.jetbrains.kotlin:kotlin-gradle-plugin:1.7.0"
    }
}
''');

        // 2. An app-level build.gradle.kts using Kotlin DSL variable style
        final File appBuildGradleKts = androidProject.appGradleFile;
        appBuildGradleKts.createSync(recursive: true);
        appBuildGradleKts.writeAsStringSync(r'''
val kotlin_version = "1.8.0"
dependencies {
    implementation("org.jetbrains.kotlin:kotlin-stdlib:$kotlin_version")
}
''');

        // 3. A dummy gradle file inside an excluded 'build' directory (should NOT be modified)
        final Directory excludedBuildDir = androidProject.hostAppGradleRoot.childDirectory('build')
          ..createSync(recursive: true);
        final File buildGradleInBuildDir = excludedBuildDir.childFile('should_not_change.gradle')
          ..createSync();
        buildGradleInBuildDir.writeAsStringSync('''
ext.kotlin_version = '1.7.0'
''');

        // 4. Run the override
        await androidProject.overwriteVersionAndTemplateValues(
          agpVersion: '8.2.0',
          kotlinVersion: '1.9.20',
        );

        // 5. Assertions
        final String rootContent = rootBuildGradle.readAsStringSync();
        expect(rootContent, contains("ext.kotlin_version = '1.9.20'"));
        expect(rootContent, contains("classpath 'com.android.tools.build:gradle:8.2.0'"));
        expect(
          rootContent,
          contains('classpath "org.jetbrains.kotlin:kotlin-gradle-plugin:1.9.20"'),
        );

        final String appContent = appBuildGradleKts.readAsStringSync();
        expect(appContent, contains('val kotlin_version = "1.9.20"'));

        // Check that the file in the excluded build directory was untouched
        expect(buildGradleInBuildDir.readAsStringSync(), contains("ext.kotlin_version = '1.7.0'"));
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => FakeProcessManager.any(),
      },
    );
  });
}
