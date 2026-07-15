// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/java.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_studio.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart' as io;
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/globals.dart' as globals;
import 'package:flutter_tools/src/project.dart';
import 'package:process/process.dart';
import 'package:test/fake.dart';
import 'package:unified_analytics/unified_analytics.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/context.dart' as test_context;
import '../../src/fakes.dart';

void main() {
  group('AndroidGradleBuilder Concurrency', () {
    late BufferLogger logger;
    late FakeAnalytics fakeAnalytics;
    late MemoryFileSystem fileSystem;
    late ConcurrencyTrackingProcessManager processManager;

    setUp(() {
      logger = BufferLogger.test();
      fileSystem = MemoryFileSystem.test();
      processManager = ConcurrencyTrackingProcessManager();
      Cache.flutterRoot = '';
      fakeAnalytics = getInitializedFakeAnalyticsInstance(
        fs: fileSystem,
        fakeFlutterVersion: FakeFlutterVersion(),
      );
    });

    String sdkPath() => fileSystem.directory('android-sdk').absolute.path;
    String sdkManagerPath() => fileSystem.path.join(
      sdkPath(),
      'cmdline-tools',
      'latest',
      'bin',
      globals.platform.isWindows ? 'sdkmanager.bat' : 'sdkmanager',
    );
    String sdkLicensesPath() => fileSystem.path.join(sdkPath(), 'licenses');

    void testUsingContext(
      String description,
      dynamic Function() body, {
      Map<Type, Generator> overrides = const <Type, Generator>{},
    }) {
      test_context.testUsingContext(
        description,
        body,
        overrides: <Type, Generator>{
          Java: () => FakeJava(),
          AndroidSdk: () {
            fileSystem.directory(sdkPath()).createSync(recursive: true);
            fileSystem.directory(sdkLicensesPath()).createSync(recursive: true);
            fileSystem
                .directory(fileSystem.path.join(sdkPath(), 'cmdline-tools', 'latest', 'bin'))
                .childFile(globals.platform.isWindows ? 'sdkmanager.bat' : 'sdkmanager')
                .createSync(recursive: true);
            return AndroidSdk(
              fileSystem.directory(sdkPath()),
              java: FakeJava(),
              fileSystem: fileSystem,
            );
          },
          ProcessManager: () => processManager,
          FileSystem: () => fileSystem,
          ...overrides,
        },
      );
    }

    testUsingContext(
      'buildApk does not execute concurrently on the same project directory',
      () async {
        final builder = AndroidGradleBuilder(
          java: FakeJava(),
          logger: logger,
          processManager: processManager,
          fileSystem: fileSystem,
          artifacts: Artifacts.test(),
          analytics: fakeAnalytics,
          gradleUtils: FakeGradleUtils(),
          platform: FakePlatform(),
          androidStudio: FakeAndroidStudio(),
          androidSdk: globals.androidSdk,
        );

        fileSystem.file('android/gradlew').createSync(recursive: true);
        fileSystem.directory('android').childFile('gradle.properties').createSync(recursive: true);
        fileSystem.file('android/build.gradle').createSync(recursive: true);
        fileSystem.directory('android').childDirectory('app').childFile('build.gradle')
          ..createSync(recursive: true)
          ..writeAsStringSync('apply from: irrelevant/flutter.gradle');

        fileSystem
            .directory('build')
            .childDirectory('app')
            .childDirectory('outputs')
            .childDirectory('flutter-apk')
            .childFile('app-dev-release.apk')
            .createSync(recursive: true);

        final FlutterProject project = FlutterProject.fromDirectoryTest(
          fileSystem.currentDirectory,
        );
        project.android.appManifestFile
          ..createSync(recursive: true)
          ..writeAsStringSync(r'''
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <application
        android:name="${applicationName}">
        <meta-data
            android:name="flutterEmbedding"
            android:value="2" />
    </application>
</manifest>
''');

        // Start two builds concurrently.
        final Future<void> build1 = builder.buildApk(
          project: project,
          androidBuildInfo: const AndroidBuildInfo(
            BuildInfo(
              BuildMode.release,
              'dev',
              treeShakeIcons: false,
              packageConfigPath: '.dart_tool/package_config.json',
            ),
          ),
          target: 'lib/main.dart',
        );

        final Future<void> build2 = builder.buildApk(
          project: project,
          androidBuildInfo: const AndroidBuildInfo(
            BuildInfo(
              BuildMode.release,
              'dev',
              treeShakeIcons: false,
              packageConfigPath: '.dart_tool/package_config.json',
            ),
          ),
          target: 'lib/main.dart',
        );

        // Give a short delay to allow both buildApk tasks to start the process.
        await Future<void>.delayed(const Duration(milliseconds: 50));

        // Release the processes.
        processManager.releaseAll();

        await Future.wait<void>([build1, build2]);

        // Expect that the peak concurrency of Gradle process invocations was 1.
        // If it is 2, it means they ran concurrently, which is the bug!
        expect(processManager.maxConcurrent, 1);
      },
    );
  });
}

class FakeProcess extends Fake implements io.Process {
  FakeProcess(this.onExit);

  final Future<void> Function() onExit;

  @override
  final int pid = 1234;

  @override
  Stream<List<int>> get stdout => const Stream<List<int>>.empty();

  @override
  Stream<List<int>> get stderr => const Stream<List<int>>.empty();

  @override
  io.IOSink get stdin => io.IOSink(StreamController<List<int>>().sink);

  @override
  Future<int> get exitCode async {
    await onExit();
    return 0;
  }
}

class ConcurrencyTrackingProcessManager extends Fake implements ProcessManager {
  int activeProcesses = 0;
  int maxConcurrent = 0;
  final List<Completer<void>> _completers = [];
  bool _released = false;

  void releaseAll() {
    _released = true;
    for (final completer in _completers) {
      if (!completer.isCompleted) {
        completer.complete();
      }
    }
  }

  @override
  bool canRun(dynamic executable, {String? workingDirectory}) => true;

  @override
  Future<io.Process> start(
    List<dynamic> command, {
    String? workingDirectory,
    Map<String, String>? environment,
    bool includeParentEnvironment = true,
    bool runInShell = false,
    io.ProcessStartMode mode = io.ProcessStartMode.normal,
  }) async {
    activeProcesses++;
    if (activeProcesses > maxConcurrent) {
      maxConcurrent = activeProcesses;
    }

    final completer = Completer<void>();
    _completers.add(completer);
    if (_released) {
      completer.complete();
    }

    return FakeProcess(() async {
      await completer.future;
      activeProcesses--;
    });
  }
}

class FakeGradleUtils extends Fake implements GradleUtils {
  @override
  String getExecutable(FlutterProject project) => 'gradlew';
}
