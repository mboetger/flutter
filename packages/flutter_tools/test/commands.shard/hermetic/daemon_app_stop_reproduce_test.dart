// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/commands/daemon.dart';
import 'package:flutter_tools/src/daemon.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/context.dart';

class FakeDaemonStreams implements DaemonStreams {
  final inputs = StreamController<DaemonMessage>();
  final outputs = StreamController<DaemonMessage>();

  @override
  Stream<DaemonMessage> get inputStream => inputs.stream;

  @override
  void send(Map<String, Object?> message, [List<int>? binary]) {
    outputs.add(DaemonMessage(message, binary != null ? Stream<List<int>>.value(binary) : null));
  }

  @override
  Future<void> dispose() async {
    await inputs.close();
    unawaited(outputs.close());
  }
}

class FakeAppInstance extends Fake implements AppInstance {
  FakeAppInstance({this.shouldThrow = false});

  final bool shouldThrow;
  bool stopCalled = false;

  @override
  Future<void> stop() async {
    stopCalled = true;
    if (shouldThrow) {
      throw Exception('Failed to stop app');
    }
  }
}

void main() {
  Daemon? daemon;
  late NotifyingLogger notifyingLogger;
  late FakeDaemonStreams daemonStreams;
  late DaemonConnection daemonConnection;

  setUp(() {
    final bufferLogger = BufferLogger.test();
    notifyingLogger = NotifyingLogger(verbose: false, parent: bufferLogger);
    daemonStreams = FakeDaemonStreams();
    daemonConnection = DaemonConnection(daemonStreams: daemonStreams, logger: bufferLogger);
  });

  tearDown(() async {
    await daemon?.shutdown();
    notifyingLogger.dispose();
    await daemonConnection.dispose();
  });

  testUsingContext('AppDomain.dispose stops all running apps and is resilient to errors', () async {
    daemon = Daemon(daemonConnection, notifyingLogger: notifyingLogger);
    final AppDomain appDomain = daemon!.appDomain;

    final mockApp1 = FakeAppInstance();
    final mockApp2 = FakeAppInstance(shouldThrow: true);
    final mockApp3 = FakeAppInstance();

    appDomain.apps.addAll(<AppInstance>[mockApp1, mockApp2, mockApp3]);

    expect(appDomain.apps, containsAll(<AppInstance>[mockApp1, mockApp2, mockApp3]));
    expect(mockApp1.stopCalled, false);
    expect(mockApp2.stopCalled, false);
    expect(mockApp3.stopCalled, false);

    // Shut down the daemon (which disposes all domains, including AppDomain).
    await daemon!.shutdown();

    // Verify that all mock apps were stopped, even though mockApp2 threw an exception.
    expect(mockApp1.stopCalled, true);
    expect(mockApp2.stopCalled, true);
    expect(mockApp3.stopCalled, true);
  });
}
