// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('Android Wear support reproduction tests', () {
    test('MediaQueryData has isScreenRound property', () {
      final dynamic data = const MediaQueryData();
      
      // Verify that category is watch/wearable.
      // This will throw NoSuchMethodError and fail on the current codebase.
      expect(data.isScreenRound, isFalse);
    });

    test('SystemChrome / services has wake lock capability', () async {
      final log = <MethodCall>[];
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        (MethodCall methodCall) async {
          log.add(methodCall);
          return null;
        },
      );

      await SystemChrome.setWakeLock(true);

      expect(log, hasLength(1));
      expect(
        log.single,
        isMethodCall(
          'SystemChrome.setWakeLock',
          arguments: <String, dynamic>{'enabled': true},
        ),
      );
    });
  });
}
