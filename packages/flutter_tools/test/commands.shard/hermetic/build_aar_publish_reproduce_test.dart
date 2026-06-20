// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_system/build_system.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/commands/build.dart';

import '../../src/context.dart';
import '../../src/fake_process_manager.dart';
import '../../src/fakes.dart';
import '../../src/test_build_system.dart';
import '../../src/test_flutter_command_runner.dart';

void main() {
  late BufferLogger logger;
  late MemoryFileSystem fs;
  late FakeProcessManager processManager;
  late Platform platform;
  late Cache cache;

  setUpAll(() {
    Cache.disableLocking();
  });

  setUp(() {
    fs = MemoryFileSystem.test();
    final Directory flutterRoot = fs.directory('flutter');
    Cache.flutterRoot = flutterRoot.path;
    logger = BufferLogger.test();
    platform = FakePlatform(environment: const <String, String>{'PATH': ''});
    processManager = FakeProcessManager.empty();
    cache = Cache.test(rootOverride: flutterRoot, logger: logger, processManager: processManager);
  });

  testUsingContext(
    'BuildAarCommand supports --publish flag and passes credentials to gradle',
    () async {
      // 1. Set up a dummy module project
      fs.file('pubspec.yaml').writeAsStringSync('''
name: foo_bar

flutter:
  module:
    foo: bar
''');
      final Directory dotAndroidDir = fs.directory('.android')..createSync(recursive: true);
      dotAndroidDir.childFile('gradlew').createSync();

      // 2. Create a dummy publish_info.json
      final File publishInfo = fs.file('publish_info.json')
        ..writeAsStringSync('''
{
  "repoUrl": "https://my.maven.repo",
  "username": "my-user",
  "password": "my-password"
}
''');

      // 3. Set up expectations for the gradle call.
      // We expect the tool to parse the JSON and pass these properties to gradle.
      processManager.addCommands(<FakeCommand>[
        const FakeCommand(command: <String>['chmod', '755', 'flutter/bin/cache/artifacts']),
        const FakeCommand(command: <String>['which', 'java']),
        ...<String>['Debug', 'Profile', 'Release'].map(
          (String buildMode) => FakeCommand(
            command: <Pattern>[
              '/.android/gradlew',
              '-I=/flutter/packages/flutter_tools/gradle/aar_init_script.gradle',
              ...List<RegExp>.filled(4, RegExp(r'-P[a-zA-Z-]+=.*')),
              '-q',
              // Assert that the publish properties are passed to Gradle
              '-Prepository-url=https://my.maven.repo',
              '-Prepository-username=my-user',
              '-Prepository-password=my-password',
              ...List<RegExp>.filled(6, RegExp(r'-P[a-zA-Z-]+=.*')),
              'assembleAar$buildMode',
            ],
            onRun: (_) => fs.directory('/build/host/outputs/repo').createSync(recursive: true),
          ),
        ),
      ]);

      cache.getArtifactDirectory('gradle_wrapper').createSync(recursive: true);

      final command = BuildCommand(
        androidSdk: FakeAndroidSdk(),
        buildSystem: TestBuildSystem.all(BuildResult(success: true)),
        fileSystem: fs,
        logger: logger,
        osUtils: FakeOperatingSystemUtils(),
        config: FakeConfig(),
        platform: FakePlatform(),
        fileSystemUtils: FakeFileSystemUtils(),
        terminal: FakeTerminal(),
        plistParser: FakePlistParser(),
        processUtils: FakeProcessUtils(),
        processManager: FakeProcessManager.any(),
        templateRenderer: FakeTemplateRenderer(),
        xcode: FakeXcode(),
        artifacts: FakeArtifacts(),
        cache: FakeCache(),
        flutterVersion: FakeFlutterVersion(),
      );

      // 4. Run the command.
      // This will FAIL currently with FormatException (option not recognized).
      // Once implemented, it should pass.
      await createTestCommandRunner(
        command,
      ).run(<String>['build', 'aar', '--no-pub', '--publish', publishInfo.path]);

      expect(processManager, hasNoRemainingExpectations);
    },
    overrides: <Type, Generator>{
      Cache: () => cache,
      FileSystem: () => fs,
      Platform: () => platform,
      ProcessManager: () => processManager,
    },
  );
}
