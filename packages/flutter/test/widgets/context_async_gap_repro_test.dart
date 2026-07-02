// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Using BuildContext after widget is unmounted throws assertion error', (
    WidgetTester tester,
  ) async {
    final asyncGap = Completer<void>();
    late BuildContext leakedContext;
    var mountChild = true;
    late StateSetter triggerRebuild;

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: StatefulBuilder(
            builder: (BuildContext context, StateSetter setState) {
              triggerRebuild = setState;
              if (mountChild) {
                return Builder(
                  builder: (BuildContext childContext) {
                    leakedContext = childContext;
                    return const Text('Child Widget');
                  },
                );
              } else {
                return const Text('Placeholder Widget');
              }
            },
          ),
        ),
      ),
    );

    expect(find.text('Child Widget'), findsOneWidget);

    // Start an async operation that simulates the login flow.
    // It awaits an async gap (simulating GoogleSignIn.signIn()) and then
    // attempts to use the leaked BuildContext.
    dynamic caughtException;
    final Future<void> asyncProcess = () async {
      await asyncGap.future; // Async gap
      try {
        // Attempt to look up an ancestor using the now-deactivated context.
        // This is equivalent to `context.repository<UserStorage>()` or `Scaffold.of(context)`.
        Scaffold.of(leakedContext);
      } catch (e) {
        caughtException = e;
      }
    }();

    // Simulate the state change that occurs during the async gap.
    // The widget tree rebuilds and unmounts the child widget.
    triggerRebuild(() {
      mountChild = false;
    });
    await tester.pump(); // Rebuild to unmount the child

    expect(find.text('Child Widget'), findsNothing);
    expect(find.text('Placeholder Widget'), findsOneWidget);

    // Complete the async gap, allowing the async process to resume.
    asyncGap.complete();

    // Wait for the microtasks/futures to propagate.
    await tester.idle();
    await asyncProcess;

    // Verify that the expected assertion error was thrown.
    expect(caughtException, isNotNull);
    expect(
      caughtException.toString(),
      contains("Looking up a deactivated widget's ancestor is unsafe"),
    );
    expect(caughtException.toString(), contains('asynchronous gap'));
  });
}
