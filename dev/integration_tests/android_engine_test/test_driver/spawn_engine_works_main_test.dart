// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';

import 'package:android_driver_extensions/native_driver.dart';
import 'package:flutter_driver/flutter_driver.dart';
import 'package:test/test.dart';

void main() async {
  late final FlutterDriver flutterDriver;
  late final NativeDriver nativeDriver;

  setUpAll(() async {
    flutterDriver = await FlutterDriver.connect();
    nativeDriver = await AndroidNativeDriver.connect(flutterDriver);
  });

  tearDownAll(() async {
    await nativeDriver.close();
    await flutterDriver.close();
  });

  test('verify that spawned engine works after spawner is destroyed', () async {
    await flutterDriver.waitFor(find.text('One more thing...'));

    final response = json.decode(await flutterDriver.requestData('')) as Map<String, Object?>;
    final engineId = response['engineId'] as int?;
    expect(
      engineId,
      greaterThan(1),
      reason:
          'engineId should be greater than 1 after spawning a new engine and destroying the spawner',
    );
    expect(response['status'], 'ready');
  }, timeout: Timeout.none);
}
