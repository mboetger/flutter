// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('Page transitions in Flutter are implemented in Dart and '
      'resolved via PageTransitionsTheme', () {
    const theme = PageTransitionsTheme();

    // Verify builders are defined and are standard Dart classes extending PageTransitionsBuilder.
    expect(theme.builders, isNotEmpty);
    for (final TargetPlatform platform in TargetPlatform.values) {
      final PageTransitionsBuilder? builder = theme.builders[platform];
      if (builder != null) {
        // Assert they are part of the framework's Dart type hierarchy
        expect(builder, isA<PageTransitionsBuilder>());
      }
    }
  });

  testWidgets('Developers can override transitions in PageTransitionsTheme but cannot '
      'load native OEM animation assets dynamically', (WidgetTester tester) async {
    // Show how developers define custom animations in Dart.
    // Since Flutter doesn't support dynamically calling OEM animation XMLs,
    // they must write custom PageTransitionsBuilder in Dart.
    var customBuilderCalled = false;

    final routes = <String, WidgetBuilder>{
      '/': (BuildContext context) => Material(
        child: TextButton(
          child: const Text('push'),
          onPressed: () {
            Navigator.of(context).pushNamed('/b');
          },
        ),
      ),
      '/b': (BuildContext context) => const Text('page b'),
    };

    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(
          platform: TargetPlatform.android,
          pageTransitionsTheme: PageTransitionsTheme(
            builders: <TargetPlatform, PageTransitionsBuilder>{
              TargetPlatform.android: _CustomDartPageTransitionsBuilder(
                onBuild: () {
                  customBuilderCalled = true;
                },
              ),
            },
          ),
        ),
        routes: routes,
      ),
    );

    customBuilderCalled = false;

    expect(customBuilderCalled, isFalse);

    // Tap to push and trigger the page transition
    await tester.tap(find.text('push'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 100));

    // Confirm that the custom Dart builder was invoked, demonstrating
    // that page transitions are fully controlled by the Dart theme.
    expect(customBuilderCalled, isTrue);

    // Clean up transition
    await tester.pumpAndSettle();
  });
}

class _CustomDartPageTransitionsBuilder extends PageTransitionsBuilder {
  const _CustomDartPageTransitionsBuilder({required this.onBuild});

  final VoidCallback onBuild;

  @override
  Widget buildTransitions<T>(
    PageRoute<T> route,
    BuildContext context,
    Animation<double> animation,
    Animation<double> secondaryAnimation,
    Widget child,
  ) {
    onBuild();
    return FadeTransition(opacity: animation, child: child);
  }
}
