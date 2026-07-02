// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:android_driver_extensions/native_driver.dart';
import 'package:flutter_driver/flutter_driver.dart';
import 'package:test/test.dart';

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

  test('runs continuously and monitors performance', () async {
    // Trace the performance of the spinning animation.
    final Timeline timeline = await flutterDriver.traceAction(() async {
      // Wait for 5 seconds to collect performance data.
      await Future<void>.delayed(const Duration(seconds: 5));
    });

    final summary = TimelineSummary.summarize(timeline);
    await summary.writeTimelineToFile('continuous_gl_surface_view_perf', pretty: true);

    print('Average Frame Build Time (ms): ${summary.computeAverageFrameBuildTimeMillis()}');
    print(
      'Average Frame Rasterizer Time (ms): ${summary.computeAverageFrameRasterizerTimeMillis()}',
    );
  }, timeout: Timeout.none);
}
