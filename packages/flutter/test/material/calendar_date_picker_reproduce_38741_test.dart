// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('CalendarDatePicker day grid does not contain Scrollable or GridView', (WidgetTester tester) async {
    await tester.pumpWidget(
      MaterialApp(
        home: Material(
          child: CalendarDatePicker(
            initialDate: DateTime(2016, DateTime.january, 15),
            firstDate: DateTime(2001),
            lastDate: DateTime(2031, DateTime.december, 31),
            onDateChanged: (DateTime date) {},
          ),
        ),
      ),
    );

    // Find the PageView which is used to scroll between months.
    final Finder pageView = find.byType(PageView);
    expect(pageView, findsOneWidget);

    // The children of PageView (the individual month views) should not contain any Scrollables.
    // PageView itself contains a Scrollable.
    // If the day grid is a GridView, it will introduce additional Scrollable widgets.
    // We expect only 1 Scrollable (the one from the PageView itself).
    expect(
      find.descendant(
        of: pageView,
        matching: find.byType(Scrollable),
      ),
      findsOneWidget,
    );
  });
}
