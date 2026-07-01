// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import 'dart:ui' as ui;

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('PlatformAssetBundle Android res/ fallback', () {
    final expectedData = Uint8List.fromList(<int>[1, 2, 3, 4]);
    late List<String> channelCalls;

    setUp(() {
      channelCalls = <String>[];
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMessageHandler(
        'flutter/assets',
        (ByteData? message) async {
          channelCalls.add('flutter/assets');
          return null; // Simulate not found in assets
        },
      );

      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMessageHandler(
        'flutter/resources',
        (ByteData? message) async {
          channelCalls.add('flutter/resources');
          if (message != null) {
            final String key = utf8.decode(
              message.buffer.asUint8List(message.offsetInBytes, message.lengthInBytes),
            );
            if (key == 'res/drawable/my_icon.png') {
              return ByteData.sublistView(expectedData);
            }
          }
          return null;
        },
      );
    });

    tearDown(() {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMessageHandler(
        'flutter/assets',
        null,
      );
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMessageHandler(
        'flutter/resources',
        null,
      );
    });

    test('load() falls back to flutter/resources when asset not in flutter/assets', () async {
      final bundle = PlatformAssetBundle();
      final ByteData data = await bundle.load('res/drawable/my_icon.png');
      expect(data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes), equals(expectedData));
      expect(channelCalls, equals(<String>['flutter/assets', 'flutter/resources']));
    });

    test('loadBuffer() falls back to flutter/resources when asset not in flutter/assets', () async {
      final bundle = PlatformAssetBundle();
      final ui.ImmutableBuffer buffer = await bundle.loadBuffer('res/drawable/my_icon.png');
      expect(buffer.length, equals(expectedData.length));
      expect(channelCalls, contains('flutter/resources'));
    });

    test('does not fall back to flutter/resources if asset is found in flutter/assets', () async {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMessageHandler(
        'flutter/assets',
        (ByteData? message) async {
          channelCalls.add('flutter/assets');
          return ByteData.sublistView(expectedData);
        },
      );

      final bundle = PlatformAssetBundle();
      final ByteData data = await bundle.load('res/drawable/my_icon.png');
      expect(data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes), equals(expectedData));
      expect(channelCalls, equals(<String>['flutter/assets'])); // No 'flutter/resources'
    });

    test('throws descriptive FlutterError if resource not found in either channel', () async {
      final bundle = PlatformAssetBundle();
      expect(
        () => bundle.load('res/drawable/non_existent.png'),
        throwsA(
          isA<FlutterError>().having(
            (FlutterError e) => e.message,
            'message',
            contains('The platform resource does not exist or has empty data'),
          ),
        ),
      );
    });
  });
}
