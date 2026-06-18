// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('AppBar back button pops the route while physical back button is intercepted', (
    WidgetTester tester,
  ) async {
    var customBackCount = 0;

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: Builder(
            builder: (BuildContext context) {
              return Center(
                child: ElevatedButton(
                  onPressed: () {
                    Navigator.of(context).push(
                      MaterialPageRoute<void>(
                        builder: (BuildContext context) {
                          return Scaffold(
                            appBar: AppBar(title: const Text('Detail Page')),
                            body: BackButtonListener(
                              onBackButtonPressed: () async {
                                customBackCount++;
                                return true;
                              },
                              child: const Center(child: Text('Detail Content')),
                            ),
                          );
                        },
                      ),
                    );
                  },
                  child: const Text('Go to Detail'),
                ),
              );
            },
          ),
        ),
      ),
    );

    // Go to detail page.
    await tester.tap(find.text('Go to Detail'));
    await tester.pumpAndSettle();
    expect(find.text('Detail Page'), findsOneWidget);

    // 1. Simulate physical back button press.
    // This should NOT pop the route (intercepted) and should trigger the custom back logic.
    await tester.binding.handlePopRoute();
    await tester.pumpAndSettle();

    // Verify that the physical back button was intercepted and did not pop.
    expect(find.text('Detail Page'), findsOneWidget);
    expect(customBackCount, 1);

    // 2. Press AppBar back button.
    // The developer wants this to successfully pop the route.
    await tester.tap(find.backButton());
    await tester.pumpAndSettle();

    // Verify that the AppBar back button successfully popped the detail page.
    expect(find.text('Detail Page'), findsNothing);
    expect(customBackCount, 1);
  });
}
