// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:android_driver_extensions/native_driver.dart';
import 'package:android_driver_extensions/skia_gold.dart';
import 'package:flutter_driver/flutter_driver.dart';
import 'package:test/test.dart';

import '_luci_skia_gold_prelude.dart';

void main() async {
  late final FlutterDriver flutterDriver;
  late final NativeDriver nativeDriver;

  setUpAll(() async {
    if (isLuci) {
      await enableSkiaGoldComparator(namePrefix: 'android_engine_test$goldenVariant');
    }
    flutterDriver = await FlutterDriver.connect();
    nativeDriver = await AndroidNativeDriver.connect(flutterDriver);
    await nativeDriver.configureForScreenshotTesting();
    await flutterDriver.waitUntilFirstFrameRasterized();
  });

  tearDownAll(() async {
    await nativeDriver.close();
    await flutterDriver.close();
  });

  test('transparent activity respects safe area', () async {
    // Wait for the transparent activity to be launched and render.
    final SerializableFinder transparentText = find.byValueKey('activity_text');
    await flutterDriver.waitFor(transparentText, timeout: const Duration(seconds: 15));

    // Take a screenshot and match it.
    await expectLater(nativeDriver.screenshot(), matchesGoldenFile('safe_area_transparency.png'));
  }, timeout: Timeout.none);
}
