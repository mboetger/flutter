// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('FlutterFragment back button navigation delegation (issue flutter/flutter#67011)', () {
    final frameworkHandlesBackCalls = <bool>[];

    setUp(() async {
      frameworkHandlesBackCalls.clear();
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        (MethodCall methodCall) async {
          if (methodCall.method == 'SystemNavigator.setFrameworkHandlesBack') {
            expect(methodCall.arguments, isA<bool>());
            frameworkHandlesBackCalls.add(methodCall.arguments as bool);
          }
          return null;
        },
      );
      await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.handlePlatformMessage(
        'flutter/lifecycle',
        const StringCodec().encodeMessage(AppLifecycleState.resumed.toString()),
        (ByteData? data) {},
      );
    });

    tearDown(() {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        null,
      );
    });

    testWidgets(
      'default single route sets frameworkHandlesBack to false so native Android handles back navigation',
      (WidgetTester tester) async {
        await tester.pumpWidget(const MaterialApp(home: Scaffold(body: Text('Home Page'))));

        expect(frameworkHandlesBackCalls, isNotEmpty);
        expect(frameworkHandlesBackCalls.last, isFalse);
      },
      variant: const TargetPlatformVariant(<TargetPlatform>{TargetPlatform.android}),
      skip: kIsWeb, // [intended] OnBackPressedCallback delegation is only non-web Android.
    );

    testWidgets(
      'pushing and popping routes delegates back navigation to Flutter when stack is non-empty, and back to native when empty',
      (WidgetTester tester) async {
        await tester.pumpWidget(
          MaterialApp(
            initialRoute: '/',
            routes: <String, WidgetBuilder>{
              '/': (BuildContext context) => Scaffold(
                body: Builder(
                  builder: (BuildContext context) => ElevatedButton(
                    onPressed: () {
                      Navigator.of(context).pushNamed('/second');
                    },
                    child: const Text('Push Route'),
                  ),
                ),
              ),
              '/second': (BuildContext context) => Scaffold(
                body: Builder(
                  builder: (BuildContext context) => ElevatedButton(
                    onPressed: () {
                      Navigator.of(context).pop();
                    },
                    child: const Text('Pop Route'),
                  ),
                ),
              ),
            },
          ),
        );

        // Initially on root route: Flutter cannot handle back, delegate to native Android.
        expect(frameworkHandlesBackCalls.last, isFalse);

        // Push a new route onto the stack.
        await tester.tap(find.text('Push Route'));
        await tester.pumpAndSettle();

        // Now Flutter has an inner stack and can handle back navigation: delegate to Flutter.
        expect(frameworkHandlesBackCalls.last, isTrue);

        // Pop the route back to root.
        await tester.tap(find.text('Pop Route'));
        await tester.pumpAndSettle();

        // Back on root route: Flutter cannot handle back anymore, delegate back to native Android.
        expect(frameworkHandlesBackCalls.last, isFalse);
      },
      variant: const TargetPlatformVariant(<TargetPlatform>{TargetPlatform.android}),
      skip: kIsWeb, // [intended] OnBackPressedCallback delegation is only non-web Android.
    );

    testWidgets(
      'multi-level route stack and route replacement update frameworkHandlesBack appropriately',
      (WidgetTester tester) async {
        await tester.pumpWidget(
          MaterialApp(
            initialRoute: '/',
            routes: <String, WidgetBuilder>{
              '/': (BuildContext context) => Scaffold(
                body: Builder(
                  builder: (BuildContext context) => ElevatedButton(
                    onPressed: () {
                      Navigator.of(context).pushNamed('/second');
                    },
                    child: const Text('Push Second'),
                  ),
                ),
              ),
              '/second': (BuildContext context) => Scaffold(
                body: Builder(
                  builder: (BuildContext context) => Column(
                    children: <Widget>[
                      ElevatedButton(
                        onPressed: () {
                          Navigator.of(context).pushNamed('/third');
                        },
                        child: const Text('Push Third'),
                      ),
                      ElevatedButton(
                        onPressed: () {
                          Navigator.of(context).pushReplacementNamed('/second_replaced');
                        },
                        child: const Text('Replace Second'),
                      ),
                    ],
                  ),
                ),
              ),
              '/third': (BuildContext context) => Scaffold(
                body: Builder(
                  builder: (BuildContext context) => ElevatedButton(
                    onPressed: () {
                      Navigator.of(context).pop();
                    },
                    child: const Text('Pop Third'),
                  ),
                ),
              ),
              '/second_replaced': (BuildContext context) => Scaffold(
                body: Builder(
                  builder: (BuildContext context) => ElevatedButton(
                    onPressed: () {
                      Navigator.of(context).pop();
                    },
                    child: const Text('Pop Replaced'),
                  ),
                ),
              ),
            },
          ),
        );

        expect(frameworkHandlesBackCalls.last, isFalse);

        // Push /second
        await tester.tap(find.text('Push Second'));
        await tester.pumpAndSettle();
        expect(frameworkHandlesBackCalls.last, isTrue);

        // Push /third
        await tester.tap(find.text('Push Third'));
        await tester.pumpAndSettle();
        expect(frameworkHandlesBackCalls.last, isTrue);

        // Pop /third back to /second
        await tester.tap(find.text('Pop Third'));
        await tester.pumpAndSettle();
        expect(frameworkHandlesBackCalls.last, isTrue);

        // Replace /second with /second_replaced
        await tester.tap(find.text('Replace Second'));
        await tester.pumpAndSettle();
        expect(frameworkHandlesBackCalls.last, isTrue);

        // Pop /second_replaced back to / (root)
        await tester.tap(find.text('Pop Replaced'));
        await tester.pumpAndSettle();
        expect(frameworkHandlesBackCalls.last, isFalse);
      },
      variant: const TargetPlatformVariant(<TargetPlatform>{TargetPlatform.android}),
      skip: kIsWeb, // [intended] OnBackPressedCallback delegation is only non-web Android.
    );

    test('SystemNavigator.setFrameworkHandlesBack sends platform message directly', () async {
      frameworkHandlesBackCalls.clear();

      await SystemNavigator.setFrameworkHandlesBack(true);
      expect(frameworkHandlesBackCalls, equals(<bool>[true]));

      await SystemNavigator.setFrameworkHandlesBack(false);
      expect(frameworkHandlesBackCalls, equals(<bool>[true, false]));
    });
  });
}
