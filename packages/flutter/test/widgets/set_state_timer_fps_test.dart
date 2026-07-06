// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

class TimerCounterWidget extends StatefulWidget {
  const TimerCounterWidget({super.key, required this.onBuild});

  final VoidCallback onBuild;

  @override
  State<TimerCounterWidget> createState() => _TimerCounterWidgetState();
}

class _TimerCounterWidgetState extends State<TimerCounterWidget> {
  Timer? _timer;
  int count = 0;

  @override
  void initState() {
    super.initState();
    // Simulate the user's sample: calling setState every 200ms to update count.
    _timer = Timer.periodic(const Duration(milliseconds: 200), (Timer timer) {
      if (!mounted) {
        return;
      }
      setState(() {
        count++;
      });
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    _timer = null;
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    widget.onBuild();
    return Text('Count: $count', textDirection: TextDirection.ltr);
  }
}

void main() {
  testWidgets(
    'setState() called every 200ms schedules exactly 6 frames per second, demonstrating on-demand rendering (flutter/flutter#72451)',
    (WidgetTester tester) async {
      var buildCount = 0;
      await tester.pumpWidget(TimerCounterWidget(onBuild: () => buildCount++));

      // Initial pump builds the first frame (count = 0).
      expect(buildCount, equals(1));
      expect(find.text('Count: 0'), findsOneWidget);

      // Simulate 1 second (1000ms) of time on a 60Hz display by pumping 60 times
      // in discrete ~16.67ms VSYNC increments (1000ms / 60 ≈ 16.667ms per frame).
      //
      // Why pump in 16.67ms VSYNC increments instead of a single 1-second jump?
      // In Flutter, if time is advanced in a single large jump (e.g. 1 second),
      // all 5 periodic timer callbacks fire in the background during that elapsed
      // time, but the framework coalesces all dirty marks into a single frame build
      // at the end of the pump. Pumping in discrete VSYNC intervals simulates a
      // physical display refresh loop without collapsing frames.
      for (var i = 0; i < 60; i++) {
        await tester.pump(const Duration(microseconds: 16667));
      }

      // In Flutter, frames are only scheduled and rendered when the widget tree is dirty.
      // Calling setState() every 200ms over 1 second schedules exactly 5 periodic updates
      // (at 200ms, 400ms, 600ms, 800ms, and 1000ms). Including the initial frame, buildCount is 6.
      //
      // Unlike continuous-rendering game engines that repaint 60 or 120 times per second
      // regardless of visual changes, Flutter uses an energy-efficient on-demand rendering
      // model. It idles between updates to conserve CPU and battery life.
      expect(
        buildCount,
        equals(6),
        reason:
            "Calling setState every 200ms over 1 second schedules 5 periodic updates plus 1 initial build (6 frames total), demonstrating Flutter's on-demand rendering model rather than continuous rendering.",
      );
      expect(find.text('Count: 5'), findsOneWidget);
    },
  );

  testWidgets(
    'multiple setState calls within a single frame interval are coalesced into a single build',
    (WidgetTester tester) async {
      var buildCount = 0;
      await tester.pumpWidget(TimerCounterWidget(onBuild: () => buildCount++));

      expect(buildCount, equals(1));
      expect(find.text('Count: 0'), findsOneWidget);

      // Advance time by a full 1 second (1000ms) in a single pump.
      // The periodic timer will fire 5 times (at 200ms, 400ms, 600ms, 800ms, and 1000ms),
      // calling setState() 5 times. However, because all 5 state updates occur before
      // the single scheduled frame is rendered at the end of the pump duration, the
      // framework coalesces them into a single build.
      await tester.pump(const Duration(seconds: 1));

      // Only 1 additional build occurs (2 frames total: initial build + 1 coalesced build),
      // even though setState() was called 5 times and count reached 5.
      expect(buildCount, equals(2));
      expect(find.text('Count: 5'), findsOneWidget);
    },
  );

  testWidgets(
    'periodic timer is cancelled on dispose without memory leak or lifecycle exceptions',
    (WidgetTester tester) async {
      var buildCount = 0;
      await tester.pumpWidget(TimerCounterWidget(onBuild: () => buildCount++));

      expect(buildCount, equals(1));

      // Advance time by 400ms in two discrete 200ms pumps (2 timer ticks, 2 frame builds).
      await tester.pump(const Duration(milliseconds: 200));
      await tester.pump(const Duration(milliseconds: 200));
      expect(buildCount, equals(3));
      expect(find.text('Count: 2'), findsOneWidget);

      // Unmount the widget to trigger dispose().
      await tester.pumpWidget(const Placeholder());

      // Advance time by another 1 second to verify the periodic timer was cleanly cancelled
      // and does not trigger further setState() calls or lifecycle exceptions.
      await tester.pump(const Duration(seconds: 1));
      expect(buildCount, equals(3));
    },
  );
}
