// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:args/command_runner.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_builder.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/commands/build_apk.dart';
import 'package:flutter_tools/src/globals.dart' as globals;
import 'package:flutter_tools/src/project.dart';
import 'package:test/fake.dart';
import 'package:unified_analytics/unified_analytics.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fakes.dart';
import '../../src/package_config.dart';
import '../../src/test_flutter_command_runner.dart';

void main() {
  late MemoryFileSystem fileSystem;
  late BufferLogger logger;
  late FakeAnalytics fakeAnalytics;
  late CaptureAndroidBuilder builder;

  setUpAll(() {
    Cache.disableLocking();
  });

  setUp(() {
    fileSystem = MemoryFileSystem.test();
    logger = BufferLogger.test();
    fakeAnalytics = getInitializedFakeAnalyticsInstance(
      fs: fileSystem,
      fakeFlutterVersion: FakeFlutterVersion(),
    );
    builder = CaptureAndroidBuilder();
  });

  void createMockProject() {
    fileSystem.file('pubspec.yaml').writeAsStringSync('name: my_app');
    fileSystem.file('.packages').writeAsStringSync('\n');
    writePackageConfigFiles(directory: fileSystem.currentDirectory, mainLibName: 'my_app');
    fileSystem.file('lib/main.dart').createSync(recursive: true);
    fileSystem.file('lib/main.dart').writeAsStringSync('void main() {}');
    fileSystem.file('android/app/src/main/AndroidManifest.xml').createSync(recursive: true);
    fileSystem
        .file('android/app/src/main/AndroidManifest.xml')
        .writeAsStringSync(
          '<manifest xmlns:android="http://schemas.android.com/apk/res/android">\n'
          '    <application>\n'
          '        <meta-data android:name="flutterEmbedding" android:value="2"/>\n'
          '    </application>\n'
          '</manifest>',
        );
    fileSystem.file('android/build.gradle').createSync(recursive: true);
    fileSystem.file('android/app/build.gradle').createSync(recursive: true);
    fileSystem.file('android/key.properties').createSync(recursive: true);
  }

  testUsingContext(
    'build apk passes test tags and exclude-tags to dartDefines',
    () async {
      createMockProject();

      final command = BuildApkCommand(logger: logger);
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>[
        'apk',
        '--no-pub',
        '--tags',
        'foo && bar',
        '--exclude-tags',
        'baz',
        'lib/main.dart',
      ]);

      expect(builder.capturedAndroidBuildInfo, isNotNull);
      final List<String> dartDefines = builder.capturedAndroidBuildInfo!.buildInfo.dartDefines;
      expect(dartDefines, contains('integration_test.tags=foo && bar'));
      expect(dartDefines, contains('integration_test.exclude-tags=baz'));
    },
    overrides: <Type, Generator>{
      AndroidBuilder: () => builder,
      FileSystem: () => fileSystem,
      ProcessManager: () => FakeProcessManager.any(),
      AndroidSdk: () => FakeAndroidSdk(),
      Analytics: () => fakeAnalytics,
      FlutterProjectFactory: () => FlutterProjectFactory(logger: logger, fileSystem: fileSystem),
    },
  );

  testUsingContext(
    'build apk throws ToolExit on invalid tags',
    () async {
      createMockProject();

      final command = BuildApkCommand(logger: logger);
      final CommandRunner<void> runner = createTestCommandRunner(command);

      expect(
        runner.run(<String>['apk', '--no-pub', '--tags', 'foo && (bar', 'lib/main.dart']),
        throwsToolExit(message: 'Invalid --tags selector'),
      );
    },
    overrides: <Type, Generator>{
      AndroidBuilder: () => builder,
      FileSystem: () => fileSystem,
      ProcessManager: () => FakeProcessManager.any(),
      AndroidSdk: () => FakeAndroidSdk(),
      Analytics: () => fakeAnalytics,
      FlutterProjectFactory: () => FlutterProjectFactory(logger: logger, fileSystem: fileSystem),
    },
  );

  testUsingContext(
    'build apk throws ToolExit on invalid exclude-tags',
    () async {
      createMockProject();

      final command = BuildApkCommand(logger: logger);
      final CommandRunner<void> runner = createTestCommandRunner(command);

      expect(
        runner.run(<String>['apk', '--no-pub', '--exclude-tags', 'foo || !', 'lib/main.dart']),
        throwsToolExit(message: 'Invalid --exclude-tags selector'),
      );
    },
    overrides: <Type, Generator>{
      AndroidBuilder: () => builder,
      FileSystem: () => fileSystem,
      ProcessManager: () => FakeProcessManager.any(),
      AndroidSdk: () => FakeAndroidSdk(),
      Analytics: () => fakeAnalytics,
      FlutterProjectFactory: () => FlutterProjectFactory(logger: logger, fileSystem: fileSystem),
    },
  );

  testUsingContext(
    'build apk overrides conflicting manual dart-defines',
    () async {
      createMockProject();

      final command = BuildApkCommand(logger: logger);
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>[
        'apk',
        '--no-pub',
        '--tags',
        'foo',
        '--exclude-tags',
        'bar',
        '--dart-define',
        'integration_test.tags=manual_foo',
        '--dart-define',
        'integration_test.exclude-tags=manual_bar',
        'lib/main.dart',
      ]);

      expect(builder.capturedAndroidBuildInfo, isNotNull);
      final List<String> dartDefines = builder.capturedAndroidBuildInfo!.buildInfo.dartDefines;
      expect(dartDefines, contains('integration_test.tags=foo'));
      expect(dartDefines, contains('integration_test.exclude-tags=bar'));
      expect(dartDefines, isNot(contains('integration_test.tags=manual_foo')));
      expect(dartDefines, isNot(contains('integration_test.exclude-tags=manual_bar')));
    },
    overrides: <Type, Generator>{
      AndroidBuilder: () => builder,
      FileSystem: () => fileSystem,
      ProcessManager: () => FakeProcessManager.any(),
      AndroidSdk: () => FakeAndroidSdk(),
      Analytics: () => fakeAnalytics,
      FlutterProjectFactory: () => FlutterProjectFactory(logger: logger, fileSystem: fileSystem),
    },
  );
}

class CaptureAndroidBuilder extends Fake implements AndroidBuilder {
  AndroidBuildInfo? capturedAndroidBuildInfo;
  String? capturedTarget;

  @override
  Future<void> buildApk({
    required FlutterProject project,
    required AndroidBuildInfo androidBuildInfo,
    required String target,
    bool configOnly = false,
  }) async {
    capturedAndroidBuildInfo = androidBuildInfo;
    capturedTarget = target;
  }
}

class FakeAndroidSdk extends Fake implements AndroidSdk {
  @override
  Directory get directory => globals.fs.directory('sdk');
}
