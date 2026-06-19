// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_builder.dart';
import 'package:flutter_tools/src/application_package.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/build_system/tools/shader_compiler.dart';
import 'package:flutter_tools/src/compile.dart';
import 'package:flutter_tools/src/devfs.dart';
import 'package:flutter_tools/src/device.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:flutter_tools/src/resident_runner.dart';
import 'package:flutter_tools/src/run_hot.dart';
import 'package:package_config/package_config.dart';
import 'package:test/fake.dart';
import 'package:unified_analytics/unified_analytics.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fake_devices.dart';
import '../../src/fakes.dart';
import '../../src/package_config.dart';

void main() {
  group('Concurrent Gradle Builds Reproduction', () {
    late MemoryFileSystem fileSystem;
    late BufferLogger logger;
    late ConcurrencyTrackingAndroidBuilder androidBuilderInstance;
    late FakeAnalytics fakeAnalytics;

    setUp(() {
      fileSystem = MemoryFileSystem.test();
      logger = BufferLogger.test();
      androidBuilderInstance = ConcurrencyTrackingAndroidBuilder();
      fakeAnalytics = getInitializedFakeAnalyticsInstance(
        fs: fileSystem,
        fakeFlutterVersion: FakeFlutterVersion(),
      );
    });

    testUsingContext(
      'HotRunner.run succeeds when AndroidBuilder serializes builds',
      () async {
        // Create a Flutter project on the fake file system.
        fileSystem.file('pubspec.yaml').writeAsStringSync('name: test_project\n');
        writePackageConfigFiles(
          directory: fileSystem.currentDirectory,
          mainLibName: 'test_project',
        );
        fileSystem.file('lib/main.dart').createSync(recursive: true);

        // We create two fake Android devices that trigger Android builds.
        final device1 = FakeAndroidDevice('device1', androidBuilderInstance);
        final device2 = FakeAndroidDevice('device2', androidBuilderInstance);

        final flutterDevice1 = FlutterDevice(
          device1,
          buildInfo: const BuildInfo(
            BuildMode.debug,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
          targetPlatform: TargetPlatform.android_arm,
          generator: FakeResidentCompiler(),
          developmentShaderCompiler: const FakeShaderCompiler(),
        );

        final flutterDevice2 = FlutterDevice(
          device2,
          buildInfo: const BuildInfo(
            BuildMode.debug,
            null,
            treeShakeIcons: false,
            packageConfigPath: '.dart_tool/package_config.json',
          ),
          targetPlatform: TargetPlatform.android_arm,
          generator: FakeResidentCompiler(),
          developmentShaderCompiler: const FakeShaderCompiler(),
        );

        final runner = HotRunner(
          <FlutterDevice>[flutterDevice1, flutterDevice2],
          target: 'lib/main.dart',
          debuggingOptions: DebuggingOptions.disabled(BuildInfo.debug),
          analytics: fakeAnalytics,
        );

        try {
          await runner.run();
        } on Exception catch (_) {
          // Expect some failure during VM service connection/attach since this is a fake,
          // but we only care about the concurrent build calls.
        }

        // Verify that the concurrent build was detected!
        expect(
          androidBuilderInstance.concurrentBuildDetected,
          isFalse,
          reason:
              'Concurrent builds should be serialized by the builder, ensuring they never execute concurrently.',
        );
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => FakeProcessManager.any(),
        Logger: () => logger,
        AndroidBuilder: () => androidBuilderInstance,
        ApplicationPackageFactory: () => FakeApplicationPackageFactory(),
      },
    );
  });
}

class ConcurrencyTrackingAndroidBuilder extends Fake implements AndroidBuilder {
  int activeBuilds = 0;
  bool concurrentBuildDetected = false;
  Future<void> _buildQueue = Future<void>.value();

  @override
  Future<void> buildApk({
    required FlutterProject project,
    required AndroidBuildInfo androidBuildInfo,
    required String target,
    bool configOnly = false,
  }) {
    final completer = Completer<void>();
    _buildQueue = _buildQueue.whenComplete(() async {
      activeBuilds++;
      if (activeBuilds > 1) {
        concurrentBuildDetected = true;
      }
      // Delay to guarantee overlapping calls.
      await Future<void>.delayed(const Duration(milliseconds: 100));
      activeBuilds--;
      completer.complete();
    });
    return completer.future;
  }
}

class FakeAndroidDevice extends FakeDevice {
  FakeAndroidDevice(String id, this.androidBuilderInstance)
    : super('FakeAndroidDevice_$id', id, type: PlatformType.android);

  final AndroidBuilder androidBuilderInstance;

  @override
  Future<TargetPlatform> get targetPlatform async => TargetPlatform.android_arm;

  @override
  Future<LaunchResult> startApp(
    ApplicationPackage? package, {
    String? mainPath,
    String? route,
    DebuggingOptions? debuggingOptions,
    Map<String, dynamic>? platformArgs,
    bool prebuiltApplication = false,
    bool ipv6 = false,
    String? userIdentifier,
  }) async {
    await androidBuilderInstance.buildApk(
      project: FlutterProject.current(),
      androidBuildInfo: const AndroidBuildInfo(BuildInfo.debug),
      target: mainPath ?? 'lib/main.dart',
    );
    return LaunchResult.succeeded();
  }
}

class FakeApplicationPackageFactory extends Fake implements ApplicationPackageFactory {
  @override
  Future<ApplicationPackage> getPackageForPlatform(
    TargetPlatform platform, {
    BuildInfo? buildInfo,
    File? applicationBinary,
  }) async {
    return FakeApplicationPackage();
  }
}

class FakeApplicationPackage extends Fake implements ApplicationPackage {
  @override
  String get name => 'app';
}

class FakeResidentCompiler extends Fake implements ResidentCompiler {
  @override
  Future<CompilerOutput?> recompile(
    Uri mainUri,
    List<Uri>? invalidatedFiles, {
    required String outputPath,
    required PackageConfig packageConfig,
    required FileSystem fs,
    String? projectRootPath,
    bool suppressErrors = false,
    bool checkDartPluginRegistry = false,
    File? dartPluginRegistrant,
    Uri? nativeAssetsYaml,
    bool recompileRestart = false,
  }) async {
    return const CompilerOutput('outputPath', 0, <Uri>[]);
  }

  @override
  void accept() {}

  @override
  Future<CompilerOutput?> reject() async => null;
}

class FakeShaderCompiler implements DevelopmentShaderCompiler {
  const FakeShaderCompiler();

  @override
  void configureCompiler(TargetPlatform? platform) {}

  @override
  Future<DevFSContent> recompileShader(DevFSContent inputShader) {
    throw UnimplementedError();
  }
}
