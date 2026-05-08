// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Scaffold provides AnnotatedRegion when AppBar is absent', (WidgetTester tester) async {
    await tester.pumpWidget(
      const MaterialApp(
        home: Scaffold(
          body: Center(child: Text('No AppBar')),
        ),
      ),
    );

    expect(find.byType(AnnotatedRegion<SystemUiOverlayStyle>), findsOneWidget);
  });

  testWidgets('Scaffold updates status bar color when AppBar is removed', (WidgetTester tester) async {
    bool hasAppBar = true;
    late StateSetter setState;

    await tester.pumpWidget(
      MaterialApp(
        home: StatefulBuilder(
          builder: (BuildContext context, StateSetter setter) {
            setState = setter;
            return Scaffold(
              appBar: hasAppBar ? AppBar(title: const Text('Title')) : null,
              body: const Center(child: Text('Body')),
            );
          },
        ),
      ),
    );

    // One from AppBar, Scaffold does not add its own if AppBar is present.
    expect(find.byType(AnnotatedRegion<SystemUiOverlayStyle>), findsOneWidget);

    await tester.runAsync(() async {
      setState(() => hasAppBar = false);
    });
    await tester.pumpAndSettle();

    // Now AppBar is gone, only Scaffold's one remains.
    expect(find.byType(AnnotatedRegion<SystemUiOverlayStyle>), findsOneWidget);
  });
}
