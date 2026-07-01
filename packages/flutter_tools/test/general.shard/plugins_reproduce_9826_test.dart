// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/dart/pub.dart';
import 'package:flutter_tools/src/darwin/darwin.dart';
import 'package:flutter_tools/src/flutter_manifest.dart';
import 'package:flutter_tools/src/flutter_plugins.dart';
import 'package:flutter_tools/src/macos/darwin_dependency_management.dart';
import 'package:flutter_tools/src/plugins.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/fake.dart';

import '../src/common.dart';
import '../src/context.dart';
import '../src/package_config.dart';
import '../src/throwing_pub.dart';

void main() {
  group('reproduce_issue_9826', () {
    late FileSystem fs;
    late FakeFlutterProject flutterProject;

    setUp(() {
      fs = MemoryFileSystem.test();
      flutterProject = FakeFlutterProject(fs);
      flutterProject.manifest = FakeFlutterManifest();
    });

    testUsingContext(
      'GeneratedPluginRegistrant files are NOT generated in user source directories',
      () async {
        // Setup a dummy plugin in the project
        final Directory pluginDir = fs.directory('/path/to/plugin');
        pluginDir.childFile('pubspec.yaml')
          ..createSync(recursive: true)
          ..writeAsStringSync('''
name: test_plugin
flutter:
  plugin:
    platforms:
      android:
        package: io.flutter.plugins.test
        pluginClass: TestPlugin
      ios:
        pluginClass: TestPlugin
      macos:
        pluginClass: TestPlugin
''');

        // Configure package config and package graph
        writePackageConfigFiles(
          directory: flutterProject.directory,
          mainLibName: 'my_app',
          packages: <String, String>{'test_plugin': pluginDir.path},
        );

        // Create the dummy plugin's Android file so that the tool detects it as a valid plugin
        pluginDir
            .childDirectory('android')
            .childDirectory('src')
            .childDirectory('main')
            .childDirectory('java')
            .childDirectory('io')
            .childDirectory('flutter')
            .childDirectory('plugins')
            .childDirectory('test')
            .childFile('TestPlugin.java')
          ..createSync(recursive: true)
          ..writeAsStringSync('import io.flutter.embedding.engine.plugins.FlutterPlugin;');

        // Run injectPlugins
        final dependencyManagement = FakeDarwinDependencyManagement();
        await injectPlugins(
          flutterProject,
          androidPlatform: true,
          iosPlatform: true,
          macOSPlatform: true,
          darwinDependencyManagement: dependencyManagement,
          releaseMode: false,
        );

        // Define the legacy user source directories
        final File legacyAndroidRegistrant = flutterProject.directory
            .childDirectory('android')
            .childDirectory('app')
            .childDirectory('src')
            .childDirectory('main')
            .childDirectory('java')
            .childDirectory('io')
            .childDirectory('flutter')
            .childDirectory('plugins')
            .childFile('GeneratedPluginRegistrant.java');

        final File legacyIosHeader = flutterProject.directory
            .childDirectory('ios')
            .childDirectory('Runner')
            .childFile('GeneratedPluginRegistrant.h');

        final File legacyIosImplementation = flutterProject.directory
            .childDirectory('ios')
            .childDirectory('Runner')
            .childFile('GeneratedPluginRegistrant.m');

        final File legacyMacosImplementation = flutterProject.directory
            .childDirectory('macos')
            .childDirectory('Flutter')
            .childFile('GeneratedPluginRegistrant.swift');

        // Assert that they do NOT exist in the legacy locations
        expect(
          legacyAndroidRegistrant,
          isNot(exists),
          reason: 'Android registrant should not be in user source directory',
        );
        expect(
          legacyIosHeader,
          isNot(exists),
          reason: 'iOS registrant header should not be in user source directory',
        );
        expect(
          legacyIosImplementation,
          isNot(exists),
          reason: 'iOS registrant implementation should not be in user source directory',
        );
        expect(
          legacyMacosImplementation,
          isNot(exists),
          reason: 'macOS registrant should not be in user source directory',
        );

        // Define the new ephemeral/build locations
        final File ephemeralAndroidRegistrant = flutterProject.directory
            .childDirectory('.dart_tool')
            .childDirectory('flutter_build')
            .childDirectory('android')
            .childDirectory('io')
            .childDirectory('flutter')
            .childDirectory('plugins')
            .childFile('GeneratedPluginRegistrant.java');

        final File ephemeralIosHeader = flutterProject.directory
            .childDirectory('.dart_tool')
            .childDirectory('flutter_build')
            .childDirectory('ios')
            .childFile('GeneratedPluginRegistrant.h');

        final File ephemeralIosImplementation = flutterProject.directory
            .childDirectory('.dart_tool')
            .childDirectory('flutter_build')
            .childDirectory('ios')
            .childFile('GeneratedPluginRegistrant.m');

        final File ephemeralMacosImplementation = flutterProject.directory
            .childDirectory('.dart_tool')
            .childDirectory('flutter_build')
            .childDirectory('macos')
            .childFile('GeneratedPluginRegistrant.swift');

        // Assert they DO exist in the new ephemeral locations
        expect(
          ephemeralAndroidRegistrant,
          exists,
          reason: 'Android registrant should be in ephemeral directory',
        );
        expect(
          ephemeralIosHeader,
          exists,
          reason: 'iOS registrant header should be in ephemeral directory',
        );
        expect(
          ephemeralIosImplementation,
          exists,
          reason: 'iOS registrant implementation should be in ephemeral directory',
        );
        expect(
          ephemeralMacosImplementation,
          exists,
          reason: 'macOS registrant should be in ephemeral directory',
        );
      },
      overrides: <Type, Generator>{
        FileSystem: () => fs,
        ProcessManager: () => FakeProcessManager.any(),
        Pub: () => const ThrowingPub(),
      },
    );
  });
}

class FakeFlutterManifest extends Fake implements FlutterManifest {
  @override
  String appName = 'my_app';

  @override
  Set<String> dependencies = <String>{};
}

class FakeFlutterProject extends Fake implements FlutterProject {
  FakeFlutterProject(this.fileSystem) {
    directory = fileSystem.directory('/path/to/project');
    flutterPluginsDependenciesFile = directory.childFile('.flutter-plugins-dependencies');

    ios = FakeIosProject(directory.childDirectory('ios'));
    android = FakeAndroidProject(directory.childDirectory('android'));
    macos = FakeMacOSProject(directory.childDirectory('macos'));
    web = FakeWebProject(directory.childDirectory('web'));
    linux = FakeLinuxProject(directory.childDirectory('linux'));
    windows = FakeWindowsProject(directory.childDirectory('windows'));
  }

  final FileSystem fileSystem;

  @override
  bool isModule = false;

  @override
  late FlutterManifest manifest;

  @override
  late Directory directory;

  @override
  late File flutterPluginsDependenciesFile;

  @override
  late IosProject ios;

  @override
  late AndroidProject android;

  @override
  late WebProject web;

  @override
  late MacOSProject macos;

  @override
  late LinuxProject linux;

  @override
  late WindowsProject windows;

  @override
  File get packageConfig => directory.childDirectory('.dart_tool').childFile('package_config.json');
}

class FakeAndroidProject extends Fake implements AndroidProject {
  FakeAndroidProject(this.hostAppGradleRoot) {
    pluginRegistrantHost = hostAppGradleRoot.childDirectory('app');
  }

  @override
  String pluginConfigKey = 'android';

  @override
  late Directory pluginRegistrantHost;

  @override
  late Directory hostAppGradleRoot;

  @override
  bool existsSync() => true;

  @override
  AndroidEmbeddingVersion getEmbeddingVersion() => AndroidEmbeddingVersion.v2;

  @override
  AndroidEmbeddingVersionResult computeEmbeddingVersion() =>
      AndroidEmbeddingVersionResult(AndroidEmbeddingVersion.v2, 'reasons');

  @override
  File get generatedPluginRegistrantFile => hostAppGradleRoot.parent
      .childDirectory('.dart_tool')
      .childDirectory('flutter_build')
      .childDirectory('android')
      .childDirectory('io')
      .childDirectory('flutter')
      .childDirectory('plugins')
      .childFile('GeneratedPluginRegistrant.java');

  @override
  File get legacyGeneratedPluginRegistrantFile => pluginRegistrantHost
      .childDirectory('src')
      .childDirectory('main')
      .childDirectory('java')
      .childDirectory('io')
      .childDirectory('flutter')
      .childDirectory('plugins')
      .childFile('GeneratedPluginRegistrant.java');
}

class FakeIosProject extends Fake implements IosProject {
  FakeIosProject(this.hostAppRoot) {
    pluginRegistrantHost = hostAppRoot.childDirectory('Runner');
    podfile = hostAppRoot.childFile('Podfile');
    podManifestLock = hostAppRoot.childFile('Podfile.lock');
  }

  @override
  final Directory hostAppRoot;

  @override
  String pluginConfigKey = 'ios';

  @override
  bool existsSync() => true;

  @override
  bool get exists => true;

  @override
  late Directory pluginRegistrantHost;

  @override
  File get pluginRegistrantHeader => hostAppRoot.parent
      .childDirectory('.dart_tool')
      .childDirectory('flutter_build')
      .childDirectory('ios')
      .childFile('GeneratedPluginRegistrant.h');

  @override
  File get pluginRegistrantImplementation => hostAppRoot.parent
      .childDirectory('.dart_tool')
      .childDirectory('flutter_build')
      .childDirectory('ios')
      .childFile('GeneratedPluginRegistrant.m');

  @override
  File get legacyPluginRegistrantHeader =>
      pluginRegistrantHost.childFile('GeneratedPluginRegistrant.h');

  @override
  File get legacyPluginRegistrantImplementation =>
      pluginRegistrantHost.childFile('GeneratedPluginRegistrant.m');

  @override
  late File podfile;

  @override
  late File podManifestLock;

  @override
  bool usesSwiftPackageManager = false;

  @override
  bool flutterPluginSwiftPackageInProjectSettings = false;
}

class FakeMacOSProject extends Fake implements MacOSProject {
  FakeMacOSProject(this.hostAppRoot) {
    managedDirectory = hostAppRoot.childDirectory('Flutter');
    podfile = hostAppRoot.childFile('Podfile');
    podManifestLock = hostAppRoot.childFile('Podfile.lock');
  }

  @override
  final Directory hostAppRoot;

  @override
  String pluginConfigKey = 'macos';

  @override
  bool existsSync() => true;

  @override
  late File podfile;

  @override
  late File podManifestLock;

  @override
  bool usesSwiftPackageManager = false;

  @override
  bool flutterPluginSwiftPackageInProjectSettings = false;

  @override
  late Directory managedDirectory;

  @override
  File get pluginRegistrantImplementation => hostAppRoot.parent
      .childDirectory('.dart_tool')
      .childDirectory('flutter_build')
      .childDirectory('macos')
      .childFile('GeneratedPluginRegistrant.swift');

  @override
  File get legacyPluginRegistrantImplementation =>
      managedDirectory.childFile('GeneratedPluginRegistrant.swift');
}

class FakeWebProject extends Fake implements WebProject {
  FakeWebProject(Directory _);
  @override
  String pluginConfigKey = 'web';
  @override
  bool existsSync() => false;
}

class FakeWindowsProject extends Fake implements WindowsProject {
  FakeWindowsProject(Directory _);
  @override
  String pluginConfigKey = 'windows';
  @override
  bool existsSync() => false;
}

class FakeLinuxProject extends Fake implements LinuxProject {
  FakeLinuxProject(Directory _);
  @override
  String pluginConfigKey = 'linux';
  @override
  bool existsSync() => false;
}

class FakeDarwinDependencyManagement extends Fake implements DarwinDependencyManagement {
  final List<FlutterDarwinPlatform> setupPlatforms = <FlutterDarwinPlatform>[];

  @override
  Future<void> setUp({
    required FlutterDarwinPlatform platform,
    required List<Plugin> plugins,
  }) async {
    setupPlatforms.add(platform);
  }
}
