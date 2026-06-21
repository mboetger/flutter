// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/base/dds.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';
import 'package:flutter_tools/src/base/platform.dart';
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
import 'package:test/fake.dart';
import 'package:unified_analytics/unified_analytics.dart';
import 'package:vm_service/vm_service.dart' as vm_service;

import '../src/common.dart';
import '../src/context.dart';
import '../src/fakes.dart';

void main() {
  group('Connection error reproduction tests', () {
    late FileSystem fileSystem;

    setUp(() {
      fileSystem = MemoryFileSystem.test();
    });

    testUsingContext('ColdRunner attach prints user-friendly error on HttpException', () async {
      final residentCompiler = FakeResidentCompiler();
      final device = FakeDevice()
        ..supportsHotReload = true
        ..supportsHotRestart = false;

      final devices = <FlutterDevice>[
        TestFlutterDevice(
          device: device,
          generator: residentCompiler,
          exception: const HttpException(
            'Connection closed before full header was received, '
            'uri = http://127.0.0.1:63394/5ZmLv8A59xY=/ws',
          ),
        ),
      ];

      final int exitCode = await ColdRunner(
        devices,
        debuggingOptions: DebuggingOptions.enabled(BuildInfo.debug),
        target: 'main.dart',
      ).attach();

      expect(exitCode, 2);

      // We expect a user-friendly error message indicating the device might be offline.
      // Currently, it prints the raw HttpException details, which is the bug.
      expect(testLogger.errorText, contains('Device is offline'));
      expect(
        testLogger.errorText,
        isNot(contains('Connection closed before full header was received')),
      );
    });

    testUsingContext(
      'HotRunner attach prints user-friendly error on HttpException',
      () async {
        final residentCompiler = FakeResidentCompiler();
        final device = FakeDevice()
          ..supportsHotReload = true
          ..supportsHotRestart = true;

        final devices = <FlutterDevice>[
          TestFlutterDevice(
            device: device,
            generator: residentCompiler,
            exception: const HttpException(
              'Connection closed before full header was received, '
              'uri = http://127.0.0.1:63394/5ZmLv8A59xY=/ws',
            ),
          ),
        ];

        final FakeAnalytics fakeAnalytics = getInitializedFakeAnalyticsInstance(
          fs: fileSystem as MemoryFileSystem,
          fakeFlutterVersion: FakeFlutterVersion(),
        );

        final int exitCode = await HotRunner(
          devices,
          debuggingOptions: DebuggingOptions.enabled(BuildInfo.debug),
          target: 'main.dart',
          analytics: fakeAnalytics,
        ).attach(needsFullRestart: false);

        expect(exitCode, 2);

        // We expect a user-friendly error message indicating the device might be offline.
        expect(testLogger.errorText, contains('Device is offline'));
        expect(
          testLogger.errorText,
          isNot(contains('Connection closed before full header was received')),
        );
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        Platform: () => FakePlatform(),
        ProcessManager: () => FakeProcessManager.any(),
      },
    );

    testUsingContext('ColdRunner attach prints user-friendly error on SocketException', () async {
      final residentCompiler = FakeResidentCompiler();
      final device = FakeDevice()
        ..supportsHotReload = true
        ..supportsHotRestart = false;

      final devices = <FlutterDevice>[
        TestFlutterDevice(
          device: device,
          generator: residentCompiler,
          exception: const SocketException('Connection refused'),
        ),
      ];

      final int exitCode = await ColdRunner(
        devices,
        debuggingOptions: DebuggingOptions.enabled(BuildInfo.debug),
        target: 'main.dart',
      ).attach();

      expect(exitCode, 2);
      expect(testLogger.errorText, contains('Device is offline'));
      expect(testLogger.errorText, isNot(contains('Connection refused')));
    });

    testUsingContext(
      'ColdRunner attach prints user-friendly error on WebSocketException',
      () async {
        final residentCompiler = FakeResidentCompiler();
        final device = FakeDevice()
          ..supportsHotReload = true
          ..supportsHotRestart = false;

        final devices = <FlutterDevice>[
          TestFlutterDevice(
            device: device,
            generator: residentCompiler,
            exception: const WebSocketException('Connection reset by peer'),
          ),
        ];

        final int exitCode = await ColdRunner(
          devices,
          debuggingOptions: DebuggingOptions.enabled(BuildInfo.debug),
          target: 'main.dart',
        ).attach();

        expect(exitCode, 2);
        expect(testLogger.errorText, contains('Device is offline'));
        expect(testLogger.errorText, isNot(contains('Connection reset by peer')));
      },
    );

    testUsingContext(
      'ColdRunner attach prints user-friendly error on connection disposed RPCError',
      () async {
        final residentCompiler = FakeResidentCompiler();
        final device = FakeDevice()
          ..supportsHotReload = true
          ..supportsHotRestart = false;

        final devices = <FlutterDevice>[
          TestFlutterDevice(
            device: device,
            generator: residentCompiler,
            // RPCError is not const, so we instantiate it dynamically
            exception: vm_service.RPCError(
              'getFlutterViews',
              vm_service.RPCErrorKind.kConnectionDisposed.code,
              'Service connection disposed',
            ),
          ),
        ];

        final int exitCode = await ColdRunner(
          devices,
          debuggingOptions: DebuggingOptions.enabled(BuildInfo.debug),
          target: 'main.dart',
        ).attach();

        expect(exitCode, 2);
        expect(testLogger.errorText, contains('Device is offline'));
        expect(testLogger.errorText, isNot(contains('Service connection disposed')));
      },
    );

    testUsingContext(
      'ColdRunner attach prints user-friendly error on generic exception with connection string',
      () async {
        final residentCompiler = FakeResidentCompiler();
        final device = FakeDevice()
          ..supportsHotReload = true
          ..supportsHotRestart = false;

        final devices = <FlutterDevice>[
          TestFlutterDevice(
            device: device,
            generator: residentCompiler,
            exception: Exception('failed to connect to 127.0.0.1'),
          ),
        ];

        final int exitCode = await ColdRunner(
          devices,
          debuggingOptions: DebuggingOptions.enabled(BuildInfo.debug),
          target: 'main.dart',
        ).attach();

        expect(exitCode, 2);
        expect(testLogger.errorText, contains('Device is offline'));
        expect(testLogger.errorText, isNot(contains('failed to connect to')));
      },
    );
  });
}

class TestFlutterDevice extends FlutterDevice {
  TestFlutterDevice({
    required Device device,
    required this.exception,
    required ResidentCompiler generator,
  }) : super(
         targetPlatform: TargetPlatform.android_arm,
         device,
         buildInfo: BuildInfo.debug,
         generator: generator,
         developmentShaderCompiler: const FakeShaderCompiler(),
       );

  final Exception exception;

  @override
  Future<void> connect({
    ReloadSources? reloadSources,
    Restart? restart,
    CompileExpression? compileExpression,
    FlutterProject? flutterProject,
    PrintStructuredErrorLogMethod? printStructuredErrorLogMethod,
    required DebuggingOptions debuggingOptions,
    int? hostVmServicePort,
  }) async {
    throw exception;
  }
}

class FakeDevice extends Fake implements Device {
  @override
  Future<bool> isSupported() async => true;

  @override
  bool supportsHotReload = false;

  @override
  bool supportsHotRestart = false;

  @override
  Future<String> get sdkNameAndVersion async => 'Android 10';

  @override
  String get name => 'test';

  @override
  String get displayName => name;

  @override
  Future<TargetPlatform> get targetPlatform async => TargetPlatform.android_arm;

  @override
  DartDevelopmentService get dds => FakeDartDevelopmentService();

  @override
  Future<void> dispose() async {}
}

class FakeDartDevelopmentService extends Fake implements DartDevelopmentService {
  @override
  late Future<void> done;

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

class FakeResidentCompiler extends Fake implements ResidentCompiler {}

class FakeShaderCompiler implements DevelopmentShaderCompiler {
  const FakeShaderCompiler();

  @override
  void configureCompiler(TargetPlatform? platform) {}

  @override
  Future<DevFSContent> recompileShader(DevFSContent inputShader) {
    throw UnimplementedError();
  }
}
