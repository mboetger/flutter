// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

class _LifecycleObserver with WidgetsBindingObserver {
  _LifecycleObserver(this.channel, this.log);

  final MethodChannel channel;
  final List<String> log;

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.inactive) {
      channel.invokeMethod<String>('check', 'inactive').then((String? res) {
        log.add('inactive: $res');
      });
    }
    if (state == AppLifecycleState.paused) {
      channel.invokeMethod<String>('check', 'paused').then((String? res) {
        log.add('paused: $res');
      });
    }
    if (state == AppLifecycleState.detached) {
      channel.invokeMethod<String>('check', 'detached').then((String? res) {
        log.add('detached: $res');
      });
    }
  }
}

void main() {
  testWidgets('MethodChannel invokeMethod during lifecycle changes', (WidgetTester tester) async {
    const channel = MethodChannel('flutter/test_lifecycle_channel');
    final log = <String>[];
    final observer = _LifecycleObserver(channel, log);

    WidgetsBinding.instance.addObserver(observer);
    addTearDown(() => WidgetsBinding.instance.removeObserver(observer));

    tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(channel, (
      MethodCall methodCall,
    ) async {
      return 'OK(${methodCall.arguments})';
    });

    Future<void> setAppLifeCycleState(AppLifecycleState state) async {
      final ByteData? message = const StringCodec().encodeMessage(state.toString());
      await tester.binding.defaultBinaryMessenger.handlePlatformMessage(
        'flutter/lifecycle',
        message,
        (_) {},
      );
    }

    // Go through inactive, paused, and detached states.
    await setAppLifeCycleState(AppLifecycleState.inactive);
    await setAppLifeCycleState(AppLifecycleState.paused);
    await setAppLifeCycleState(AppLifecycleState.detached);

    expect(
      log,
      equals(<String>['inactive: OK(inactive)', 'paused: OK(paused)', 'detached: OK(detached)']),
    );
  });
}
