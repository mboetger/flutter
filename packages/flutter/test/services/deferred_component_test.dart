// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('installDeferredComponent test', () async {
    final log = <MethodCall>[];

    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.deferredComponent,
      (MethodCall methodCall) async {
        log.add(methodCall);
        return null;
      },
    );

    await DeferredComponent.installDeferredComponent(componentName: 'testComponentName');

    expect(log, hasLength(1));
    expect(
      log.single,
      isMethodCall(
        'installDeferredComponent',
        arguments: <String, dynamic>{'loadingUnitId': -1, 'componentName': 'testComponentName'},
      ),
    );
  });

  test('uninstallDeferredComponent test', () async {
    final log = <MethodCall>[];

    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.deferredComponent,
      (MethodCall methodCall) async {
        log.add(methodCall);
        return null;
      },
    );

    await DeferredComponent.uninstallDeferredComponent(componentName: 'testComponentName');

    expect(log, hasLength(1));
    expect(
      log.single,
      isMethodCall(
        'uninstallDeferredComponent',
        arguments: <String, dynamic>{'loadingUnitId': -1, 'componentName': 'testComponentName'},
      ),
    );
  });

  test('getDeferredComponentInstallState test', () async {
    final log = <MethodCall>[];

    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.deferredComponent,
      (MethodCall methodCall) async {
        log.add(methodCall);
        if (methodCall.method == 'getDeferredComponentInstallState') {
          return 'downloading';
        }
        return null;
      },
    );

    final String state = await DeferredComponent.getDeferredComponentInstallState(
      componentName: 'testComponentName',
    );

    expect(state, 'downloading');
    expect(log, hasLength(1));
    expect(
      log.single,
      isMethodCall(
        'getDeferredComponentInstallState',
        arguments: <String, dynamic>{'loadingUnitId': -1, 'componentName': 'testComponentName'},
      ),
    );
  });

  test('installStateEventStream test', () async {
    final events = <DeferredComponentEvent>[];
    final StreamSubscription<DeferredComponentEvent> subscription = DeferredComponent
        .installStateEventStream
        .listen(events.add);

    // Simulate a platform call to Dart
    final ByteData? response = await TestDefaultBinaryMessengerBinding
        .instance
        .defaultBinaryMessenger
        .handlePlatformMessage(
          SystemChannels.deferredComponent.name,
          const StandardMethodCodec().encodeMethodCall(
            const MethodCall('installStateChanged', <dynamic, dynamic>{
              'componentName': 'testComponentName',
              'state': 'downloading',
            }),
          ),
          null,
        );

    expect(response, isNotNull);
    // Wait for the microtasks/stream events to propagate
    await Future<void>.delayed(Duration.zero);

    expect(events, hasLength(1));
    expect(events.single.componentName, 'testComponentName');
    expect(events.single.state, 'downloading');

    await subscription.cancel();
  });
}
