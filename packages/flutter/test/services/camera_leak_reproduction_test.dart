// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:leak_tracker/leak_tracker.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();

  test('MethodChannel image stream does not leak message payloads', () async {
    const channel = MethodChannel('plugins.flutter.io/camera_leak_test');

    final payloadRefs = <WeakReference<Uint8List>>[];
    var frameCount = 0;

    // Simulate startImageStream listener
    channel.setMethodCallHandler((MethodCall call) async {
      if (call.method == 'yuv_images') {
        final args = call.arguments as Map<dynamic, dynamic>;
        final planes = args['planes'] as Uint8List;
        payloadRefs.add(WeakReference<Uint8List>(planes));
        frameCount++;
      }
      return null;
    });

    const codec = StandardMethodCodec();

    // Simulate 50 frames being sent from the platform side
    for (var i = 0; i < 50; i++) {
      final largePayload = Uint8List(3 * 1024 * 1024); // 3MB
      final ByteData message = codec.encodeMethodCall(
        MethodCall('yuv_images', <dynamic, dynamic>{'planes': largePayload}),
      );

      // Push the message into the channel (simulating native -> Dart)
      await ServicesBinding.instance.defaultBinaryMessenger.handlePlatformMessage(
        channel.name,
        message,
        (ByteData? reply) {},
      );
    }

    // Allow microtasks to run so the method call handlers are executed
    await Future<void>.delayed(Duration.zero);

    expect(frameCount, 50);
    expect(payloadRefs.length, 50);

    // Force garbage collection
    await forceGC(fullGcCycles: 3);

    // Verify that the payloads have been garbage collected
    var leakedCount = 0;
    for (final ref in payloadRefs) {
      if (ref.target != null) {
        leakedCount++;
      }
    }

    expect(leakedCount, 0, reason: 'Some image stream payloads were not garbage collected.');
  });
}
