// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:file/memory.dart';
import 'package:flutter_tools/src/application_package.dart';
import 'package:flutter_tools/src/asset.dart';
import 'package:flutter_tools/src/base/dds.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/build_system/tools/shader_compiler.dart';
import 'package:flutter_tools/src/compile.dart';
import 'package:flutter_tools/src/devfs.dart';
import 'package:flutter_tools/src/device.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:flutter_tools/src/resident_runner.dart';
import 'package:flutter_tools/src/run_cold.dart';
import 'package:flutter_tools/src/run_hot.dart';
import 'package:flutter_tools/src/vmservice.dart';
import 'package:package_config/package_config.dart';
import 'package:test/fake.dart';
import 'package:unified_analytics/unified_analytics.dart';
import 'package:vm_service/vm_service.dart' as vm_service;

import '../src/common.dart';
import '../src/context.dart';

void main() {
  group('logging during attach', () {
    late MemoryFileSystem fileSystem;
    late FakeDevice device;
    late FakeFlutterDevice flutterDevice;

    setUp(() {
      fileSystem = MemoryFileSystem.test();
      device = FakeDevice();
      flutterDevice = FakeFlutterDevice(device);
    });

    testUsingContext(
      'HotRunner.attach() starts echoing device log',
      () async {
        final runner = TestHotRunner(
          <FlutterDevice>[flutterDevice],
          target: 'main.dart',
          debuggingOptions: DebuggingOptions.enabled(BuildInfo.debug),
          analytics: const NoOpAnalytics(),
        );

        await runner.attach();

        expect(device.logReader.logLinesListened, true);
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: FakeProcessManager.empty,
      },
    );

    testUsingContext(
      'ColdRunner.attach() starts echoing device log',
      () async {
        final runner = TestColdRunner(
          <FlutterDevice>[flutterDevice],
          target: 'main.dart',
          debuggingOptions: DebuggingOptions.enabled(BuildInfo.debug),
        );

        await runner.attach();

        expect(device.logReader.logLinesListened, true);
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: FakeProcessManager.empty,
      },
    );
  });
}

class TestHotRunner extends HotRunner {
  TestHotRunner(
    super.flutterDevices, {
    required super.target,
    required super.debuggingOptions,
    required super.analytics,
  });

  @override
  Future<int> waitForAppToFinish() async {
    return 0;
  }
}

class TestColdRunner extends ColdRunner {
  TestColdRunner(super.flutterDevices, {required super.target, required super.debuggingOptions});

  @override
  Future<int> waitForAppToFinish() async {
    return 0;
  }
}

class FakeDevice extends Fake implements Device {
  final FakeDeviceLogReader logReader = FakeDeviceLogReader();

  @override
  Future<bool> isSupported() async => true;

  @override
  bool supportsHotReload = true;

  @override
  bool supportsHotRestart = true;

  @override
  Future<String> get sdkNameAndVersion async => 'Android 10';

  @override
  String get name => 'test';

  @override
  String get displayName => name;

  @override
  Future<TargetPlatform> get targetPlatform async => TargetPlatform.tester;

  @override
  DartDevelopmentService get dds => FakeDartDevelopmentService();

  @override
  Future<bool> get isLocalEmulator async => false;

  @override
  Future<DeviceLogReader> getLogReader({
    ApplicationPackage? app,
    bool includePastLogs = false,
  }) async {
    return logReader;
  }

  @override
  Future<void> dispose() async {}
}

class FakeDeviceLogReader extends Fake implements DeviceLogReader {
  bool logLinesListened = false;

  @override
  Stream<String> get logLines {
    logLinesListened = true;
    return const Stream<String>.empty();
  }

  @override
  Future<void> provideVmService(FlutterVmService vmService) async {}
}

class FakeDartDevelopmentService extends Fake implements DartDevelopmentService {
  @override
  late Future<void> done = Completer<void>().future;

  @override
  Uri? uri;

  @override
  Uri? devToolsUri;

  @override
  Uri? dtdUri;

  @override
  Future<void> startDartDevelopmentService(
    Uri vmServiceUri, {
    String? appName = 'Fake App',
    int? ddsPort,
    FlutterDevice? device,
    bool? ipv6,
    bool? disableServiceAuthCodes,
    bool enableDevTools = false,
    bool cacheStartupProfile = false,
    String? google3WorkspaceRoot,
    Uri? devToolsServerAddress,
  }) async {}

  @override
  Future<void> shutdown() async {}
}

class FakeFlutterDevice extends Fake implements FlutterDevice {
  FakeFlutterDevice(this.device);

  @override
  final Device device;

  @override
  Stream<Uri> get vmServiceUris => const Stream<Uri>.empty();

  @override
  FlutterVmService get vmService => FakeFlutterVmService();

  @override
  DevFS? devFS = FakeDevFS();

  @override
  ResidentCompiler? get generator => null;

  @override
  DevelopmentShaderCompiler get developmentShaderCompiler => const FakeShaderCompiler();

  @override
  TargetPlatform get targetPlatform => TargetPlatform.android_arm;

  @override
  Future<void> connect({
    ReloadSources? reloadSources,
    Restart? restart,
    CompileExpression? compileExpression,
    FlutterProject? flutterProject,
    PrintStructuredErrorLogMethod? printStructuredErrorLogMethod,
    required DebuggingOptions debuggingOptions,
    int? hostVmServicePort,
  }) async {}

  @override
  Future<void> startEchoingDeviceLog(DebuggingOptions debuggingOptions) async {
    final DeviceLogReader logReader = await device.getLogReader();
    logReader.logLines.listen((String line) {});
  }

  @override
  Future<void> stopEchoingDeviceLog() async {}

  @override
  Future<Uri?> setupDevFS(String fsName, Directory rootDirectory) async {
    devFS = FakeDevFS();
    return Uri.parse('file:///devfs');
  }

  @override
  Future<UpdateFSReport> updateDevFS({
    required Uri mainUri,
    String? target,
    AssetBundle? bundle,
    bool bundleFirstUpload = false,
    bool bundleDirty = false,
    bool fullRestart = false,
    required String pathToReload,
    required String dillOutputPath,
    required List<Uri> invalidatedFiles,
    required PackageConfig packageConfig,
  }) async {
    return UpdateFSReport(success: true);
  }
}

class FakeDevFS extends Fake implements DevFS {
  @override
  final Uri? baseUri = Uri();

  @override
  DateTime? lastCompiled;

  @override
  PackageConfig? lastPackageConfig;

  @override
  List<Uri> get sources => const <Uri>[];

  @override
  Future<void> destroy() async {}

  @override
  Future<UpdateFSReport> update({
    required Uri mainUri,
    required ResidentCompiler generator,
    required bool trackWidgetCreation,
    required String pathToReload,
    required List<Uri> invalidatedFiles,
    required PackageConfig packageConfig,
    required String dillOutputPath,
    required DevelopmentShaderCompiler shaderCompiler,
    DevFSWriter? devFSWriter,
    String? target,
    AssetBundle? bundle,
    bool bundleFirstUpload = false,
    bool fullRestart = false,
    bool resetCompiler = false,
    File? dartPluginRegistrant,
  }) async {
    return UpdateFSReport(success: true);
  }
}

class FakeFlutterVmService extends Fake implements FlutterVmService {
  @override
  vm_service.VmService get service => FakeVmService();

  @override
  Future<List<FlutterView>> getFlutterViews({
    bool returnEarly = false,
    Duration delay = const Duration(milliseconds: 50),
  }) async {
    return <FlutterView>[];
  }
}

class FakeVmService extends Fake implements vm_service.VmService {
  @override
  Future<vm_service.Success> streamListen(String streamId) async => vm_service.Success();

  @override
  Stream<vm_service.Event> get onExtensionEvent {
    return Stream<vm_service.Event>.fromIterable(<vm_service.Event>[
      vm_service.Event(kind: 'Extension', extensionKind: 'Flutter.FirstFrame', timestamp: 1),
    ]);
  }

  @override
  Future<void> get onDone => Completer<void>().future;
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
