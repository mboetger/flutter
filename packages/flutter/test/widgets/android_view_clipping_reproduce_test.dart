// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('AndroidView clipBehavior propagation', () {
    final List<MethodCall> log = <MethodCall>[];

    setUp(() {
      log.clear();
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform_views,
        (MethodCall methodCall) async {
          log.add(methodCall);
          if (methodCall.method == 'create') {
            return 0; // Return a texture ID
          }
          return null;
        },
      );
    });

    tearDown(() {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform_views,
        null,
      );
    });

    testWidgets('AndroidView passes clipBehavior: Clip.none to the platform', (WidgetTester tester) async {
      await tester.pumpWidget(
        const MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 200.0,
              height: 100.0,
              child: AndroidView(
                viewType: 'webview',
                layoutDirection: TextDirection.ltr,
                clipBehavior: Clip.none,
              ),
            ),
          ),
        ),
      );

      final MethodCall createCall = log.firstWhere(
        (MethodCall call) => call.method == 'create',
        orElse: () => fail('No create method call found'),
      );
      final Map<dynamic, dynamic> args = createCall.arguments as Map<dynamic, dynamic>;

      expect(args.containsKey('clipBehavior'), isTrue, reason: 'clipBehavior should be passed in the create arguments');
      expect(args['clipBehavior'], Clip.none.index, reason: 'clipBehavior should match the value passed to AndroidView');
    });

    testWidgets('AndroidView passes clipBehavior: Clip.hardEdge (default) to the platform', (WidgetTester tester) async {
      await tester.pumpWidget(
        const MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 200.0,
              height: 100.0,
              child: AndroidView(
                viewType: 'webview',
                layoutDirection: TextDirection.ltr,
              ),
            ),
          ),
        ),
      );

      final MethodCall createCall = log.firstWhere(
        (MethodCall call) => call.method == 'create',
        orElse: () => fail('No create method call found'),
      );
      final Map<dynamic, dynamic> args = createCall.arguments as Map<dynamic, dynamic>;

      expect(args.containsKey('clipBehavior'), isTrue, reason: 'clipBehavior should be passed in the create arguments');
      expect(args['clipBehavior'], Clip.hardEdge.index, reason: 'clipBehavior should match the value passed to AndroidView');
    });

    testWidgets('AndroidView propagates clipBehavior updates dynamically', (WidgetTester tester) async {
      await tester.pumpWidget(
        const MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 200.0,
              height: 100.0,
              child: AndroidView(
                viewType: 'webview',
                layoutDirection: TextDirection.ltr,
                clipBehavior: Clip.hardEdge,
              ),
            ),
          ),
        ),
      );

      // Verify initial create call.
      expect(log.where((MethodCall call) => call.method == 'create').length, 1);
      expect(log.where((MethodCall call) => call.method == 'setClipBehavior').length, 0);

      // Rebuild with different clipBehavior.
      await tester.pumpWidget(
        const MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 200.0,
              height: 100.0,
              child: AndroidView(
                viewType: 'webview',
                layoutDirection: TextDirection.ltr,
                clipBehavior: Clip.none,
              ),
            ),
          ),
        ),
      );

      // Verify setClipBehavior was called.
      final MethodCall setClipCall = log.firstWhere(
        (MethodCall call) => call.method == 'setClipBehavior',
        orElse: () => fail('No setClipBehavior method call found'),
      );
      final Map<dynamic, dynamic> args = setClipCall.arguments as Map<dynamic, dynamic>;
      final MethodCall createCall = log.firstWhere((MethodCall call) => call.method == 'create');
      final int viewId = (createCall.arguments as Map<dynamic, dynamic>)['id'] as int;
      expect(args['id'], viewId);
      expect(args['clipBehavior'], Clip.none.index);
    });

    testWidgets('AndroidViewSurface passes clipBehavior on creation and updates', (WidgetTester tester) async {
      final UniqueKey key = UniqueKey();
      final PlatformViewController controller = PlatformViewsService.initAndroidView(
        id: 0,
        viewType: 'webview',
        layoutDirection: TextDirection.ltr,
        clipBehavior: Clip.none,
      );

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 200.0,
              height: 100.0,
              child: AndroidViewSurface(
                key: key,
                controller: controller as AndroidViewController,
                hitTestBehavior: PlatformViewHitTestBehavior.opaque,
                gestureRecognizers: const <Factory<OneSequenceGestureRecognizer>>{},
                clipBehavior: Clip.none,
              ),
            ),
          ),
        ),
      );

      // Verify create call.
      final MethodCall createCall = log.firstWhere(
        (MethodCall call) => call.method == 'create',
        orElse: () => fail('No create method call found'),
      );
      final Map<dynamic, dynamic> args = createCall.arguments as Map<dynamic, dynamic>;
      expect(args['clipBehavior'], Clip.none.index);

      // Update clipBehavior.
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 200.0,
              height: 100.0,
              child: AndroidViewSurface(
                key: key,
                controller: controller,
                hitTestBehavior: PlatformViewHitTestBehavior.opaque,
                gestureRecognizers: const <Factory<OneSequenceGestureRecognizer>>{},
                clipBehavior: Clip.hardEdge,
              ),
            ),
          ),
        ),
      );

      // Verify setClipBehavior was called.
      final MethodCall setClipCall = log.firstWhere(
        (MethodCall call) => call.method == 'setClipBehavior',
        orElse: () => fail('No setClipBehavior method call found'),
      );
      final Map<dynamic, dynamic> setClipArgs = setClipCall.arguments as Map<dynamic, dynamic>;
      expect(setClipArgs['id'], 0);
      expect(setClipArgs['clipBehavior'], Clip.hardEdge.index);
    });
  });
}
