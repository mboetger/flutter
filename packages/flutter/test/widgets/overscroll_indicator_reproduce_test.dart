// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

Future<void> slowDrag(WidgetTester tester, Offset start, Offset offset) async {
  final TestGesture gesture = await tester.startGesture(start);
  for (var index = 0; index < 10; index += 1) {
    await gesture.moveBy(offset);
    await tester.pump(const Duration(milliseconds: 20));
  }
  await gesture.up();
}

void main() {
  testWidgets(
    'OverscrollIndicator with custom paintOffset does not move into forbidden region when scrolling in opposite direction',
    (WidgetTester tester) async {
      // Regression test for https://github.com/flutter/flutter/issues/96057
      await tester.pumpWidget(
        Directionality(
          textDirection: TextDirection.ltr,
          child: NotificationListener<OverscrollIndicatorNotification>(
            onNotification: (OverscrollIndicatorNotification notification) {
              notification.paintOffset = 50.0; // Set a custom paint offset
              return false;
            },
            child: const CustomScrollView(
              slivers: <Widget>[SliverToBoxAdapter(child: SizedBox(height: 2000.0))],
            ),
          ),
        ),
      );

      final RenderObject painter = tester.renderObject(find.byType(CustomPaint));

      // Trigger leading overscroll (pulling down).
      await slowDrag(tester, const Offset(200.0, 200.0), const Offset(0.0, 5.0));
      expect(
        painter,
        paints
          ..save()
          ..translate(y: 50.0)
          ..scale()
          ..circle(),
      );

      // Drag in the opposite direction (scrolling back up) by 30 pixels.
      await tester.dragFrom(const Offset(200.0, 200.0), const Offset(0.0, -30.0));
      await tester.pump();

      // The glow effect should stay at the set paintOffset position (50.0) and not move into the [0.0, 50.0] forbidden region.
      // If the bug is present, the translation will be at 20.0 (50.0 - 30.0) instead of 50.0.
      expect(
        painter,
        paints
          ..save()
          ..translate(y: 50.0)
          ..scale()
          ..circle(),
      );
    },
  );
}
