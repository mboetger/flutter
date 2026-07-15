// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import 'package:android_driver_extensions/native_driver.dart';
import 'package:flutter_driver/flutter_driver.dart';
import 'package:test/test.dart';

class CustomNativeCommand extends Command {
  final String method;
  final Map<String, dynamic>? arguments;

  CustomNativeCommand(this.method, [this.arguments]);

  @override
  String get kind => 'native_driver';

  @override
  Map<String, String> serialize() {
    final Map<String, String> serialized = super.serialize();
    serialized['method'] = method;
    if (arguments != null) {
      serialized['arguments'] = jsonEncode(arguments);
    }
    return serialized;
  }
}

void main() async {
  late final FlutterDriver flutterDriver;
  late final NativeDriver nativeDriver;

  setUpAll(() async {
    flutterDriver = await FlutterDriver.connect();
    nativeDriver = await AndroidNativeDriver.connect(flutterDriver);
    await flutterDriver.waitUntilFirstFrameRasterized();
  });

  tearDownAll(() async {
    await nativeDriver.close();
    await flutterDriver.close();
  });

  test('asserts that Window.OnFrameMetricsAvailableListener gets notified', () async {
    // 1. Register the frame metrics listener
    final registerResponse = await flutterDriver.sendCommand(
      CustomNativeCommand('register_frame_metrics_listener'),
    );
    print('Register listener response: $registerResponse');

    // 2. Wait for 5 seconds to let the timer in the app render frames
    print('Waiting for frames to render...');
    await Future<void>.delayed(const Duration(seconds: 5));

    // 3. Get the frame metrics count
    final countResponse = await flutterDriver.sendCommand(
      CustomNativeCommand('get_frame_metrics_count'),
    );
    print('Count response: $countResponse');

    // Unregister listener
    await flutterDriver.sendCommand(
      CustomNativeCommand('unregister_frame_metrics_listener'),
    );

    // The countResponse will be decoded as a map: {"count": X}
    final int count = countResponse['count'] as int;
    print('Frame metrics callback count: $count');

    // We expect the listener to be called (count > 0).
    // Currently, it fails because Flutter renders directly to a SurfaceView / bypasses Window renderer.
    expect(count, greaterThan(0), reason: 'Window.OnFrameMetricsAvailableListener was not called');
  }, timeout: Timeout.none);
}
