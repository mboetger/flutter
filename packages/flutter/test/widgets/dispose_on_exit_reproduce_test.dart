// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  setUp(() {
    TestWidgetsFlutterBinding.ensureInitialized().defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.platform,
      (MethodCall methodCall) async {
        if (methodCall.method == 'SystemNavigator.pop') {
          return null;
        }
        return null;
      },
    );
  });

  tearDown(() {
    TestWidgetsFlutterBinding.ensureInitialized().defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.platform,
      null,
    );
  });

  testWidgets('StatefulWidget dispose is called when app is exited via SystemNavigator.pop', (
    WidgetTester tester,
  ) async {
    var disposed = false;

    await tester.pumpWidget(
      MaterialApp(
        home: TestWidget(
          onDispose: () {
            disposed = true;
          },
        ),
      ),
    );

    expect(disposed, isFalse);

    // Simulate the system popping the route (e.g. back button on Android)
    // which eventually calls SystemNavigator.pop().
    await tester.binding.handlePopRoute();

    // We expect that the widget tree is disposed when the app is exited.
    // If the bug is present, this will fail because the widget tree is not disposed.
    expect(disposed, isTrue);
  });

  testWidgets('StatefulWidget dispose is called when SystemNavigator.pop is called directly', (
    WidgetTester tester,
  ) async {
    var disposed = false;

    await tester.pumpWidget(
      MaterialApp(
        home: TestWidget(
          onDispose: () {
            disposed = true;
          },
        ),
      ),
    );

    expect(disposed, isFalse);

    await SystemNavigator.pop();

    // We expect that the widget tree is disposed when the app is exited.
    expect(disposed, isTrue);
  });

  testWidgets(
    'StatefulWidget dispose is NOT called when SystemNavigator.pop is called directly but disposeOnPlatformPop is false',
    (WidgetTester tester) async {
      var disposed = false;

      await tester.pumpWidget(
        MaterialApp(
          home: TestWidget(
            onDispose: () {
              disposed = true;
            },
          ),
        ),
      );

      expect(disposed, isFalse);

      tester.binding.disposeOnPlatformPop = false;
      addTearDown(() {
        tester.binding.disposeOnPlatformPop = true;
      });

      await SystemNavigator.pop();

      // We expect that the widget tree is NOT disposed.
      expect(disposed, isFalse);
    },
  );
}

class TestWidget extends StatefulWidget {
  const TestWidget({super.key, required this.onDispose});

  final VoidCallback onDispose;

  @override
  State<TestWidget> createState() => _TestWidgetState();
}

class _TestWidgetState extends State<TestWidget> {
  @override
  void dispose() {
    widget.onDispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return const Scaffold(body: Center(child: Text('Test')));
  }
}
