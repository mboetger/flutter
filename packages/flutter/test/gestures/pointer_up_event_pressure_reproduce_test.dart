// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/gestures.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('PointerUpEvent can contain non-zero pressure', () {
    // 1. Verify default pressure is 0.0
    const defaultEvent = PointerUpEvent();
    expect(defaultEvent.pressure, 0.0);

    // 2. Verify non-zero pressure is preserved by the constructor
    const event = PointerUpEvent(pressure: 0.5);
    expect(event.pressure, 0.5);

    // 3. Verify copyWith preserves the non-zero pressure
    final PointerUpEvent copied = event.copyWith();
    expect(copied.pressure, 0.5);

    // 4. Verify copyWith can update the pressure
    final PointerUpEvent copiedWithNewPressure = event.copyWith(pressure: 0.7);
    expect(copiedWithNewPressure.pressure, 0.7);
  });
}
