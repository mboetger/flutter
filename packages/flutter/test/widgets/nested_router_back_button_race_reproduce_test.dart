// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ignore_for_file: prefer_const_literals_to_create_immutables, prefer_const_constructors

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Hardware back button race condition with nested Router - slow clicks', (
    WidgetTester tester,
  ) async {
    final log = <String>[];

    await tester.pumpWidget(MainApp(onPopLog: log.add));

    expect(find.text('Inner screen'), findsOneWidget);
    expect(find.text('Main screen'), findsNothing);

    // First back button press (slow)
    final bool handled1 = await tester.binding.handlePopRoute();
    expect(handled1, isTrue);
    await tester.pumpAndSettle(); // Wait for the transition to finish completely

    expect(log, <String>['MainRouter#onPopPage']);
    log.clear();

    // Second back button press (slow)
    // Since the pages list is hardcoded, the Navigator rebuilds and re-pushes InnerScreen after pumpAndSettle.
    // So the second press should also be handled and pop the page again.
    final bool handled2 = await tester.binding.handlePopRoute();
    expect(handled2, isTrue);
    await tester.pumpAndSettle();

    expect(log, <String>['MainRouter#onPopPage']);
  });

  testWidgets('Hardware back button race condition with nested Router - quick clicks', (
    WidgetTester tester,
  ) async {
    final log = <String>[];

    await tester.pumpWidget(MainApp(onPopLog: log.add));

    expect(find.text('Inner screen'), findsOneWidget);
    expect(find.text('Main screen'), findsNothing);

    // First back button press (quick)
    final Future<bool> handled1 = tester.binding.handlePopRoute();

    // Second back button press (quick)
    // Because the frame hasn't been pumped yet, this second press will be queued
    // and processed after the first pop's frame has finished drawing.
    final Future<bool> handled2 = tester.binding.handlePopRoute();

    // Pump a frame to allow the first pop's scheduled transition to complete and
    // the queue to process the second press.
    await tester.pump();

    // Both should be successfully handled!
    expect(await handled1, isTrue);
    expect(
      await handled2,
      isTrue,
      reason: 'The second back button press should be handled consistently',
    );

    await tester.pumpAndSettle();
  });

  testWidgets('Hardware back button race condition with nested Router - three quick clicks', (
    WidgetTester tester,
  ) async {
    final log = <String>[];

    await tester.pumpWidget(MainApp(onPopLog: log.add));

    expect(find.text('Inner screen'), findsOneWidget);
    expect(find.text('Main screen'), findsNothing);

    // Three rapid back button presses
    final Future<bool> handled1 = tester.binding.handlePopRoute();
    final Future<bool> handled2 = tester.binding.handlePopRoute();
    final Future<bool> handled3 = tester.binding.handlePopRoute();

    // The first pump processes the first pop
    await tester.pump();
    expect(await handled1, isTrue);

    // The second pump processes the second pop (queued)
    await tester.pump();
    expect(await handled2, isTrue);

    // The third pump processes the third pop (queued)
    await tester.pump();
    expect(await handled3, isTrue);

    await tester.pumpAndSettle();
  });
}

class MainApp extends StatefulWidget {
  const MainApp({super.key, required this.onPopLog});

  final ValueChanged<String> onPopLog;

  @override
  State<MainApp> createState() => _MainAppState();
}

class _MainAppState extends State<MainApp> {
  late final MainRouterDelegate _routerDelegate;

  @override
  void initState() {
    super.initState();
    _routerDelegate = MainRouterDelegate(onPopLog: widget.onPopLog);
  }

  @override
  void dispose() {
    _routerDelegate.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => MaterialApp.router(
    routerDelegate: _routerDelegate,
    backButtonDispatcher: RootBackButtonDispatcher(),
  );
}

class MainRouterDelegate extends RouterDelegate<Object>
    with ChangeNotifier, PopNavigatorRouterDelegateMixin<Object> {
  MainRouterDelegate({required this.onPopLog}) : navigatorKey = GlobalKey<NavigatorState>();

  @override
  final GlobalKey<NavigatorState> navigatorKey;
  final ValueChanged<String> onPopLog;

  @override
  Widget build(BuildContext context) {
    return Navigator(
      key: navigatorKey,
      pages: <Page<Object?>>[
        MaterialPage<void>(child: MainScreen(key: ValueKey<String>('mainScreen'))),
        MaterialPage<void>(child: InnerScreen(text: 'Inner screen')),
      ],
      onPopPage: (Route<dynamic> route, dynamic result) {
        onPopLog('MainRouter#onPopPage');

        if (!route.didPop(result)) {
          return false;
        }

        return true;
      },
    );
  }

  @override
  Future<void> setNewRoutePath(Object configuration) async {}
}

class MainScreen extends StatefulWidget {
  const MainScreen({super.key});

  @override
  State<MainScreen> createState() => _MainScreenState();
}

class _MainScreenState extends State<MainScreen> {
  late final InnerRouterDelegate _innerRouterDelegate;

  @override
  void initState() {
    super.initState();
    _innerRouterDelegate = InnerRouterDelegate();
  }

  @override
  void dispose() {
    _innerRouterDelegate.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(body: Router<Object>(routerDelegate: _innerRouterDelegate));
  }
}

class InnerRouterDelegate extends RouterDelegate<Object>
    with ChangeNotifier, PopNavigatorRouterDelegateMixin<Object> {
  InnerRouterDelegate() : navigatorKey = GlobalKey<NavigatorState>();

  @override
  final GlobalKey<NavigatorState> navigatorKey;

  @override
  Widget build(BuildContext context) {
    return Navigator(
      key: navigatorKey,
      pages: const <Page<Object?>>[
        MaterialPage<void>(
          child: InnerScreen(key: ValueKey<String>('innerScreen'), text: 'Main screen'),
        ),
      ],
      onPopPage: (Route<dynamic> route, dynamic result) {
        if (!route.didPop(result)) {
          return false;
        }

        return true;
      },
    );
  }

  @override
  Future<void> setNewRoutePath(Object configuration) async {}
}

class InnerScreen extends StatelessWidget {
  const InnerScreen({super.key, required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(),
      body: Center(child: Text(text)),
    );
  }
}
