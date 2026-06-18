// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/device.dart';
import 'package:flutter_tools/src/protocol_discovery.dart';
import 'package:flutter_tools/src/vmservice.dart';

import '../src/common.dart';

void main() {
  group('ProtocolDiscovery reproduce hang on error', () {
    late FakeDeviceLogReader logReader;
    late ProtocolDiscovery discoverer;

    setUp(() {
      logReader = FakeDeviceLogReader();
      discoverer = ProtocolDiscovery.vmService(
        logReader,
        ipv6: false,
        throttleDuration: const Duration(milliseconds: 5),
        logger: BufferLogger.test(),
      );
    });

    tearDown(() async {
      await discoverer.cancel();
      await logReader.dispose();
    });

    testWithoutContext('uri future propagates error from logReader instead of hanging', () async {
      final Future<Uri?> uriFuture = discoverer.uri;
      final exception = Exception('Log reader error');
      logReader.addError(exception);

      // If the bug is present, this will hang. We use a timeout to prevent hanging indefinitely.
      await expectLater(
        uriFuture.timeout(const Duration(milliseconds: 500)),
        throwsA(equals(exception)),
      );
    });

    testWithoutContext(
      'uri future propagates error from logReader even if error occurs before listener is attached',
      () async {
        final exception = Exception('Log reader error');
        logReader.addError(exception);

        // Wait a microtask for the error to propagate to the ProtocolDiscovery's listener
        await Future<void>.delayed(Duration.zero);

        final Future<Uri?> uriFuture = discoverer.uri;
        await expectLater(uriFuture, throwsA(equals(exception)));
      },
    );
  });
}

class FakeDeviceLogReader extends DeviceLogReader {
  final StreamController<String> _linesController = StreamController<String>.broadcast();

  @override
  Stream<String> get logLines => _linesController.stream;

  void addError(Object error) => _linesController.addError(error);

  @override
  Future<void> provideVmService(FlutterVmService connectedVmService) async {}

  @override
  String get name => 'Fake';

  @override
  Future<void> dispose() async {
    await _linesController.close();
  }
}
