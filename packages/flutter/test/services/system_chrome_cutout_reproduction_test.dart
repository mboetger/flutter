// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('display cutout mode configuration API compiles and sends correct message', () async {
    final log = <MethodCall>[];

    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.platform,
      (MethodCall methodCall) async {
        log.add(methodCall);
        return null;
      },
    );

    await SystemChrome.setEnabledSystemUIMode(
      SystemUiMode.immersive,
      cutoutMode: SystemUiLayoutCutoutMode.shortEdges,
    );

    expect(log, hasLength(1));
    expect(
      log.single,
      isMethodCall(
        'SystemChrome.setEnabledSystemUIMode',
        arguments: <String, dynamic>{
          'mode': 'SystemUiMode.immersive',
          'cutoutMode': 'SystemUiLayoutCutoutMode.shortEdges',
        },
      ),
    );
  });
}
