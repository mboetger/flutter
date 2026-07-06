// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('RawKeyboard Android Emulator Key Repeat (Issue flutter/flutter#72816)', () {
    testWidgets(
      'Demonstrates that holding a key on Android Emulator generates alternating down and up events instead of repeats',
      (WidgetTester tester) async {
        final events = <RawKeyEvent>[];
        void handleKey(RawKeyEvent event) {
          events.add(event);
        }

        RawKeyboard.instance.addListener(handleKey);
        addTearDown(() => RawKeyboard.instance.removeListener(handleKey));

        // On Android Emulator, holding down a key causes the emulator's input bridge
        // to inject alternating ACTION_DOWN and ACTION_UP events (with repeatCount = 0)
        // for each auto-repeat cycle from the host keyboard.
        // We simulate this sequence of raw messages from the Android platform:
        await simulateKeyDownEvent(LogicalKeyboardKey.keyA, platform: 'android');
        await simulateKeyUpEvent(LogicalKeyboardKey.keyA, platform: 'android');
        await simulateKeyDownEvent(LogicalKeyboardKey.keyA, platform: 'android');
        await simulateKeyUpEvent(LogicalKeyboardKey.keyA, platform: 'android');
        await simulateKeyDownEvent(LogicalKeyboardKey.keyA, platform: 'android');

        // Verify the actual behavior demonstrated on Android Emulator:
        // The callback receives multiple key up and key down events one after another,
        // and none of the down events are marked as repeat events (repeat: false).
        expect(events.length, 5);
        expect(events[0], isA<RawKeyDownEvent>());
        expect(events[0].repeat, isFalse);
        expect(events[1], isA<RawKeyUpEvent>());
        expect(events[2], isA<RawKeyDownEvent>());
        expect(events[2].repeat, isFalse);
        expect(events[3], isA<RawKeyUpEvent>());
        expect(events[4], isA<RawKeyDownEvent>());
        expect(events[4].repeat, isFalse);
      },
      variant: KeySimulatorTransitModeVariant.rawKeyData(),
    );

    testWidgets(
      'Verifies that on real Android hardware, holding a key generates repeat events without premature key up events',
      (WidgetTester tester) async {
        final events = <RawKeyEvent>[];
        void handleKey(RawKeyEvent event) {
          events.add(event);
        }

        RawKeyboard.instance.addListener(handleKey);
        addTearDown(() => RawKeyboard.instance.removeListener(handleKey));

        // On real Android hardware (or when using physical keyboards on real Android
        // devices), holding down a key generates consecutive ACTION_DOWN events without
        // ACTION_UP events until the key is physically released by the user.
        await simulateKeyDownEvent(LogicalKeyboardKey.keyA, platform: 'android');
        await simulateKeyDownEvent(LogicalKeyboardKey.keyA, platform: 'android');
        await simulateKeyDownEvent(LogicalKeyboardKey.keyA, platform: 'android');

        // Unlike on the Android Emulator (demonstrated in the test above), real hardware
        // does not trigger premature key up events while a key is being held down.
        expect(
          events.whereType<RawKeyUpEvent>(),
          isEmpty,
          reason:
              'Holding down a key on real Android hardware should not trigger key up events until released.',
        );
        expect(events.length, 3);
        expect(events[0], isA<RawKeyDownEvent>());
        expect(events[0].repeat, isFalse);
        expect(events[1], isA<RawKeyDownEvent>());
        expect(events[1].repeat, isTrue);
        expect(events[2], isA<RawKeyDownEvent>());
        expect(events[2].repeat, isTrue);

        // When the key is finally released, a single up event is received:
        await simulateKeyUpEvent(LogicalKeyboardKey.keyA, platform: 'android');
        expect(events.length, 4);
        expect(events[3], isA<RawKeyUpEvent>());
      },
      variant: KeySimulatorTransitModeVariant.rawKeyData(),
    );
  });
}
