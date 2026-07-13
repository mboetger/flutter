// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Pointer tracking latency test', (WidgetTester tester) async {
    Offset? paintedOffset;
    int paintCount = 0;

    final Widget app = MaterialApp(
      home: Material(
        child: StatefulBuilder(
          builder: (BuildContext context, StateSetter setState) {
            return Listener(
              onPointerMove: (PointerMoveEvent event) {
                setState(() {
                  paintedOffset = event.localPosition;
                });
              },
              child: CustomPaint(
                painter: TestOffsetPainter(
                  onPaint: (Offset? offset) {
                    paintedOffset = offset;
                    paintCount++;
                  },
                  point: paintedOffset,
                ),
                child: const SizedBox.expand(),
              ),
            );
          },
        ),
      ),
    );

    await tester.pumpWidget(app);

    // Initial paint with no pointer events.
    expect(paintCount, 1);
    expect(paintedOffset, isNull);

    // Start a gesture.
    final TestGesture gesture = await tester.startGesture(Offset.zero);
    // startGesture dispatches a down event, which should trigger a build and paint.
    // Wait, the Listener only listens to onPointerMove, so down event won't update paintedOffset.
    await tester.pump();
    expect(paintCount, 1); 
    expect(paintedOffset, isNull);

    // Now move the pointer. This dispatches a move event.
    // The move event is handled immediately and calls setState.
    await gesture.moveTo(const Offset(100, 100));

    // We check if the painter has painted the new offset.
    // Since we haven't pumped a frame yet, the paint count should still be 1,
    // and the painted offset (from the canvas paint) is still null.
    // (Note: the state variable `paintedOffset` is updated, but the painter hasn't run yet).
    expect(paintCount, 1);

    // Now pump one frame. This should run the build and paint phases.
    await tester.pump();

    // The paint count should increment to 2, and the painted offset should be updated.
    expect(paintCount, 2);
    expect(paintedOffset, const Offset(100, 100));

    // If we pump another frame without moving, it should NOT paint again (shouldRepaint returns false if point is same).
    await tester.pump();
    expect(paintCount, 2); // Paint count remains 2.
  });
}

class TestOffsetPainter extends CustomPainter {
  TestOffsetPainter({required this.onPaint, this.point});

  final ValueChanged<Offset?> onPaint;
  final Offset? point;

  @override
  void paint(Canvas canvas, Size size) {
    onPaint(point);
    if (point != null) {
      canvas.drawCircle(point!, 54, Paint()..color = Colors.red);
    }
  }

  @override
  bool shouldRepaint(TestOffsetPainter old) => point != old.point;
}
