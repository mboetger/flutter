// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

import 'navigator_utils.dart';

// Reproduction test for GitHub issue flutter/flutter#66763:
// "Android: back button key events that are marked as handled are not always absorbed."
//
// When adding a root key listener (e.g. RawKeyboardListener or Focus/FocusNode with
// onKey/onKeyEvent) above MaterialApp, the Android back button works initially before
// any physical keyboard key is pressed, but stops working after a physical keyboard
// key is pressed.
//
// Root cause analysis demonstrated by these tests:
// 1. Before pressing a physical keyboard key (or when in Android touch mode), pressing
//    the system back button invokes Activity.onBackPressed(), which sends 'popRoute'
//    directly on SystemChannels.navigation. This bypasses key event routing entirely,
//    allowing navigation to pop even if a root key handler returns true.
// 2. After pressing a physical keyboard key (exiting touch mode and giving input focus
//    to FlutterView), the Android back button generates a hardware key event
//    (KEYCODE_BACK -> LogicalKeyboardKey.goBack). When routed through FocusManager,
//    a root key handler returning KeyEventResult.handled absorbs/eats the event. Because
//    Flutter reports the key as handled, the Android embedder never redispatches it,
//    Activity.onBackPressed() is never invoked, and back navigation stops working.

const PhysicalKeyboardKey kBackPhysicalKey = PhysicalKeyboardKey.escape;

void main() {
  group('Issue #66763: Android back button key absorption inconsistency', () {
    testWidgets(
      'RawKeyboardListener above MaterialApp does not absorb LogicalKeyboardKey.goBack after key press; navigation pops consistently',
      (WidgetTester tester) async {
        final receivedKeys = <RawKeyEvent>[];
        final rootFocusNode = FocusNode(
          debugLabel: 'RootKeyboardListenerFocus',
          onKey: (FocusNode node, RawKeyEvent event) {
            receivedKeys.add(event);
            return KeyEventResult.handled; // Always mark key events as handled, as in issue #66763.
          },
        );
        addTearDown(() async {
          await tester.pumpWidget(const SizedBox.shrink());
          rootFocusNode.dispose();
        });

        await tester.pumpWidget(
          RawKeyboardListener(
            focusNode: rootFocusNode,
            onKey: (_) {},
            child: MaterialApp(
              initialRoute: '/',
              routes: <String, WidgetBuilder>{
                '/': (BuildContext context) => Scaffold(
                  appBar: AppBar(title: const Text('Home Page')),
                  body: Center(
                    child: ElevatedButton(
                      onPressed: () => Navigator.pushNamed(context, '/second'),
                      child: const Text('Go to Second'),
                    ),
                  ),
                ),
                '/second': (BuildContext context) => Scaffold(
                  appBar: AppBar(title: const Text('Second Page')),
                  body: const Center(child: Text('Second Page Content')),
                ),
              },
            ),
          ),
        );

        // Step 1: Push second page.
        await tester.tap(find.text('Go to Second'));
        await tester.pumpAndSettle();
        expect(find.text('Second Page Content'), findsOneWidget);

        // Step 2 (Before pressing a keyboard key / Android touch mode):
        // System back gesture sends 'popRoute' directly on navigation channel.
        // Even though rootFocusNode.onKey returns handled, 'popRoute' bypasses key event routing!
        await simulateSystemBack();
        await tester.pumpAndSettle();
        expect(find.text('Home Page'), findsOneWidget);
        expect(receivedKeys, isEmpty);

        // Step 3: Push second page again.
        await tester.tap(find.text('Go to Second'));
        await tester.pumpAndSettle();
        expect(find.text('Second Page Content'), findsOneWidget);

        // Step 4 (After pressing a physical key / Android keyboard mode):
        // Android Back button generates KEYCODE_BACK (LogicalKeyboardKey.goBack).
        // Send LogicalKeyboardKey.goBack key event.
        final bool handled = await tester.sendKeyEvent(
          LogicalKeyboardKey.goBack,
          physicalKey: kBackPhysicalKey,
        );
        await tester.pumpAndSettle();

        // Verify that WidgetsApp consumed the KeyDownEvent before it reached rootFocusNode,
        // and only the KeyUpEvent bubbled up.
        expect(handled, isTrue);
        expect(receivedKeys, hasLength(1));
        expect(receivedKeys.single, isA<RawKeyUpEvent>());
        expect(receivedKeys.single.logicalKey, LogicalKeyboardKey.goBack);

        // Route is popped because WidgetsApp handled LogicalKeyboardKey.goBack internally.
        expect(find.text('Home Page'), findsOneWidget);
      },
    );

    testWidgets(
      'Focus with onKeyEvent above MaterialApp does not absorb LogicalKeyboardKey.goBack; navigation pops consistently',
      (WidgetTester tester) async {
        final receivedEvents = <KeyEvent>[];
        final rootFocusNode = FocusNode(debugLabel: 'RootFocusNode');
        addTearDown(() async {
          await tester.pumpWidget(const SizedBox.shrink());
          rootFocusNode.dispose();
        });

        await tester.pumpWidget(
          Focus(
            focusNode: rootFocusNode,
            onKeyEvent: (FocusNode node, KeyEvent event) {
              receivedEvents.add(event);
              return KeyEventResult.handled; // Always absorb key events.
            },
            child: MaterialApp(
              initialRoute: '/',
              routes: <String, WidgetBuilder>{
                '/': (BuildContext context) => Scaffold(
                  appBar: AppBar(title: const Text('Home Page')),
                  body: Center(
                    child: ElevatedButton(
                      onPressed: () => Navigator.pushNamed(context, '/second'),
                      child: const Text('Go to Second'),
                    ),
                  ),
                ),
                '/second': (BuildContext context) => Scaffold(
                  appBar: AppBar(title: const Text('Second Page')),
                  body: const Center(child: Text('Second Page Content')),
                ),
              },
            ),
          ),
        );

        // Step 1: Push second page and verify system back works before keyboard input.
        await tester.tap(find.text('Go to Second'));
        await tester.pumpAndSettle();
        expect(find.text('Second Page Content'), findsOneWidget);

        await simulateSystemBack();
        await tester.pumpAndSettle();
        expect(find.text('Home Page'), findsOneWidget);
        expect(receivedEvents, isEmpty);

        // Step 2: Push second page again and verify key event back stops working.
        await tester.tap(find.text('Go to Second'));
        await tester.pumpAndSettle();
        expect(find.text('Second Page Content'), findsOneWidget);

        final bool handled = await tester.sendKeyEvent(
          LogicalKeyboardKey.goBack,
          physicalKey: kBackPhysicalKey,
        );
        await tester.pumpAndSettle();

        // Verify that WidgetsApp consumed the KeyDownEvent before it reached rootFocusNode,
        // and only the KeyUpEvent bubbled up.
        expect(handled, isTrue);
        expect(receivedEvents, hasLength(1));
        expect(receivedEvents.single, isA<KeyUpEvent>());
        expect(receivedEvents.single.logicalKey, LogicalKeyboardKey.goBack);

        // Route is popped because WidgetsApp handled LogicalKeyboardKey.goBack internally.
        expect(find.text('Home Page'), findsOneWidget);
      },
    );

    testWidgets(
      'Without root key absorption, LogicalKeyboardKey.goBack is handled directly by framework navigation',
      (WidgetTester tester) async {
        await tester.pumpWidget(
          MaterialApp(
            initialRoute: '/',
            routes: <String, WidgetBuilder>{
              '/': (BuildContext context) => Scaffold(
                appBar: AppBar(title: const Text('Home Page')),
                body: Center(
                  child: ElevatedButton(
                    onPressed: () => Navigator.pushNamed(context, '/second'),
                    child: const Text('Go to Second'),
                  ),
                ),
              ),
              '/second': (BuildContext context) => Scaffold(
                appBar: AppBar(title: const Text('Second Page')),
                body: const Center(child: Text('Second Page Content')),
              ),
            },
          ),
        );

        await tester.tap(find.text('Go to Second'));
        await tester.pumpAndSettle();
        expect(find.text('Second Page Content'), findsOneWidget);

        // LogicalKeyboardKey.goBack is handled directly by framework navigation in WidgetsApp.
        final bool handled = await tester.sendKeyEvent(
          LogicalKeyboardKey.goBack,
          physicalKey: kBackPhysicalKey,
        );
        expect(handled, isTrue);

        await tester.pumpAndSettle();
        expect(find.text('Home Page'), findsOneWidget);
      },
    );

    testWidgets(
      'Expected consistent back navigation succeeds even when root keyboard handler is present',
      (WidgetTester tester) async {
        final rootFocusNode = FocusNode(
          debugLabel: 'RootKeyboardListenerFocus',
          onKey: (FocusNode node, RawKeyEvent event) => KeyEventResult.handled,
        );
        addTearDown(() async {
          await tester.pumpWidget(const SizedBox.shrink());
          rootFocusNode.dispose();
        });

        await tester.pumpWidget(
          RawKeyboardListener(
            focusNode: rootFocusNode,
            onKey: (_) {},
            child: MaterialApp(
              initialRoute: '/',
              routes: <String, WidgetBuilder>{
                '/': (BuildContext context) => Scaffold(
                  appBar: AppBar(title: const Text('Home Page')),
                  body: Center(
                    child: ElevatedButton(
                      onPressed: () => Navigator.pushNamed(context, '/second'),
                      child: const Text('Go to Second'),
                    ),
                  ),
                ),
                '/second': (BuildContext context) => Scaffold(
                  appBar: AppBar(title: const Text('Second Page')),
                  body: const Center(child: Text('Second Page Content')),
                ),
              },
            ),
          ),
        );

        await tester.tap(find.text('Go to Second'));
        await tester.pumpAndSettle();
        expect(find.text('Second Page Content'), findsOneWidget);

        // Send LogicalKeyboardKey.goBack key event (simulating Android Back button after keyboard input).
        await tester.sendKeyEvent(LogicalKeyboardKey.goBack, physicalKey: kBackPhysicalKey);
        await tester.pumpAndSettle();

        // Pressing the back button (whether via system gesture or hardware key event)
        // reliably pops the route back to Home Page even when a root key handler is present.
        expect(
          find.text('Home Page'),
          findsOneWidget,
          reason:
              'Back button key event should reliably pop navigation even when root key handler is present.',
        );
      },
    );
  });
}
