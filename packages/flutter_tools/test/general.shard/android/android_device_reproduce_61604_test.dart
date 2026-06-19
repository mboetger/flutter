// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter_tools/src/android/android_device.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/device_port_forwarder.dart';
import 'package:flutter_tools/src/protocol_discovery.dart';

import '../../src/common.dart';
import '../../src/fake_devices.dart';
import '../../src/fake_process_manager.dart';

void main() {
  group('AndroidDevicePortForwarder - Custom ADB Host Support (Issue #61604)', () {
    testWithoutContext('AndroidDevicePortForwarder defaults host to 127.0.0.1', () async {
      final forwarder = AndroidDevicePortForwarder(
        adbPath: 'adb',
        deviceId: '1',
        processManager: FakeProcessManager.any(),
        logger: BufferLogger.test(),
        platform: FakePlatform(),
      );
      expect(forwarder.host, '127.0.0.1');
    });

    testWithoutContext(
      'AndroidDevicePortForwarder uses custom host from ADB_SERVER_SOCKET',
      () async {
        final platform = FakePlatform(
          environment: <String, String>{'ADB_SERVER_SOCKET': 'tcp:host.docker.internal:5037'},
        );
        final forwarder = AndroidDevicePortForwarder(
          adbPath: 'adb',
          deviceId: '1',
          processManager: FakeProcessManager.any(),
          logger: BufferLogger.test(),
          platform: platform,
        );
        expect(forwarder.host, 'host.docker.internal');
      },
    );

    testWithoutContext(
      'AndroidDevicePortForwarder uses custom host from ANDROID_ADB_SERVER_ADDRESS',
      () async {
        final platform = FakePlatform(
          environment: <String, String>{'ANDROID_ADB_SERVER_ADDRESS': '192.168.1.100'},
        );
        final forwarder = AndroidDevicePortForwarder(
          adbPath: 'adb',
          deviceId: '1',
          processManager: FakeProcessManager.any(),
          logger: BufferLogger.test(),
          platform: platform,
        );
        expect(forwarder.host, '192.168.1.100');
      },
    );

    testWithoutContext(
      'AndroidDevicePortForwarder uses custom host from ANDROID_ADB_SERVER_ADDRESS when port is also set',
      () async {
        final platform = FakePlatform(
          environment: <String, String>{
            'ANDROID_ADB_SERVER_ADDRESS': '192.168.1.100',
            'ANDROID_ADB_SERVER_PORT': '5037',
          },
        );
        final forwarder = AndroidDevicePortForwarder(
          adbPath: 'adb',
          deviceId: '1',
          processManager: FakeProcessManager.any(),
          logger: BufferLogger.test(),
          platform: platform,
        );
        expect(forwarder.host, '192.168.1.100');
      },
    );

    testWithoutContext(
      'AndroidDevicePortForwarder defaults host to 127.0.0.1 when ADB_SERVER_SOCKET is non-TCP/Unix',
      () async {
        final platform = FakePlatform(
          environment: <String, String>{'ADB_SERVER_SOCKET': 'unix:/var/run/adb.sock'},
        );
        final forwarder = AndroidDevicePortForwarder(
          adbPath: 'adb',
          deviceId: '1',
          processManager: FakeProcessManager.any(),
          logger: BufferLogger.test(),
          platform: platform,
        );
        expect(forwarder.host, '127.0.0.1');
      },
    );

    testWithoutContext(
      'AndroidDevicePortForwarder defaults host to 127.0.0.1 when ADB_SERVER_SOCKET is malformed (missing host)',
      () async {
        final platform = FakePlatform(
          environment: <String, String>{'ADB_SERVER_SOCKET': 'tcp::5037'},
        );
        final forwarder = AndroidDevicePortForwarder(
          adbPath: 'adb',
          deviceId: '1',
          processManager: FakeProcessManager.any(),
          logger: BufferLogger.test(),
          platform: platform,
        );
        expect(forwarder.host, '127.0.0.1');
      },
    );

    testWithoutContext(
      'AndroidDevicePortForwarder defaults host to 127.0.0.1 when ADB_SERVER_SOCKET is malformed (missing tcp prefix)',
      () async {
        final platform = FakePlatform(environment: <String, String>{'ADB_SERVER_SOCKET': '5037'});
        final forwarder = AndroidDevicePortForwarder(
          adbPath: 'adb',
          deviceId: '1',
          processManager: FakeProcessManager.any(),
          logger: BufferLogger.test(),
          platform: platform,
        );
        expect(forwarder.host, '127.0.0.1');
      },
    );

    testWithoutContext('ProtocolDiscovery uses custom host from portForwarder', () async {
      final portForwarder = FakePortForwarder('host.docker.internal', 99);
      final logReader = FakeDeviceLogReader();
      final discoverer = ProtocolDiscovery.vmService(
        logReader,
        portForwarder: portForwarder,
        ipv6: false,
        logger: BufferLogger.test(),
      );

      final Future<Uri?> nextUri = discoverer.uri;
      logReader.addLine('The Dart VM service is listening on http://127.0.0.1:12345');
      final Uri uri = (await nextUri)!;
      expect(uri.host, 'host.docker.internal');
      expect(uri.port, 99);
      expect('$uri', 'http://host.docker.internal:99');

      await discoverer.cancel();
      await logReader.dispose();
    });
  });
}

class FakePortForwarder extends DevicePortForwarder {
  FakePortForwarder(this.host, [this.availablePort]);

  @override
  final String host;
  final int? availablePort;

  @override
  Future<int> forward(int devicePort, {int? hostPort}) async {
    hostPort ??= 0;
    if (hostPort == 0) {
      return availablePort!;
    }
    return hostPort;
  }

  @override
  List<ForwardedPort> get forwardedPorts => throw UnimplementedError();

  @override
  Future<void> unforward(ForwardedPort forwardedPort) {
    throw UnimplementedError();
  }

  @override
  Future<void> dispose() async {}
}
