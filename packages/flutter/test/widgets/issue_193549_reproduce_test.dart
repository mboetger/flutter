import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Subsequent taps fail if three-finger swipe drops terminal events', (
    WidgetTester tester,
  ) async {
    int tapCount = 0;

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: ListView.builder(
            itemCount: 10,
            itemBuilder: (BuildContext context, int index) {
              return GestureDetector(
                onTap: () {
                  tapCount++;
                },
                child: SizedBox(height: 100, child: Text('Card $index')),
              );
            },
          ),
        ),
      ),
    );

    // 1. A normal tap runs the callback.
    await tester.tap(find.text('Card 0'));
    await tester.pumpAndSettle();
    expect(tapCount, 1);

    // 2. Send DOWN/MOVE for three pointers and deliberately omit UP/CANCEL.
    final TestGesture gesture1 = await tester.startGesture(const Offset(100, 100), pointer: 1);
    final TestGesture gesture2 = await tester.startGesture(const Offset(150, 100), pointer: 2);
    final TestGesture gesture3 = await tester.startGesture(const Offset(200, 100), pointer: 3);

    await gesture1.moveBy(const Offset(0, 100));
    await gesture2.moveBy(const Offset(0, 100));
    await gesture3.moveBy(const Offset(0, 100));

    // The list scrolls
    await tester.pumpAndSettle();

    // 3. Send a complete DOWN/UP for a new pointer. We simulate Android reusing the pointerId (device).
    // The previous gesture1 had pointer: 1, so it used device: 1.
    final TestPointer newPointer = TestPointer(4, PointerDeviceKind.touch, 1);
    tester.binding.handlePointerEvent(newPointer.addPointer(location: const Offset(100, 150)));
    tester.binding.handlePointerEvent(newPointer.down(const Offset(100, 150)));
    await tester.pumpAndSettle();
    tester.binding.handlePointerEvent(newPointer.up());
    tester.binding.handlePointerEvent(newPointer.removePointer());
    await tester.pumpAndSettle();

    // 4. The card callback should run (Expected behavior).
    expect(tapCount, 2);
  });
}
