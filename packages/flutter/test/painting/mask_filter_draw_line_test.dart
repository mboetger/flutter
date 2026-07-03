// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter_test/flutter_test.dart';

void main() {
  test('Short lines with stroke cap and mask filter are drawn', () async {
    // This is a reproduction test for https://github.com/flutter/flutter/issues/60601
    final recorder = ui.PictureRecorder();
    final canvas = ui.Canvas(recorder);

    final paint = ui.Paint()
      ..color =
          const ui.Color(0xFF000000) // Black
      ..style = ui.PaintingStyle.stroke
      ..strokeWidth = 2.5
      ..strokeCap = ui.StrokeCap.round
      ..maskFilter = const ui.MaskFilter.blur(ui.BlurStyle.normal, 0.09);

    // Draw a long line (length 20)
    canvas.drawLine(const ui.Offset(8, 5), const ui.Offset(8, 25), paint);
    // Draw a short line (length 15)
    canvas.drawLine(const ui.Offset(24, 5), const ui.Offset(24, 20), paint);

    final ui.Picture picture = recorder.endRecording();
    final ui.Image image = await picture.toImage(32, 30);
    final ByteData? data = await image.toByteData();

    expect(data, isNotNull);

    int getAlpha(int x, int y) {
      final int offset = (x + y * image.width) * 4;
      return data!.getUint8(offset + 3);
    }

    // Verify the long line is drawn (around x=8, y=5..25)
    var longLineDrawn = false;
    for (var y = 5; y <= 25; y++) {
      if (getAlpha(8, y) > 0) {
        longLineDrawn = true;
        break;
      }
    }
    expect(longLineDrawn, isTrue, reason: 'Long line should be drawn');

    // Verify the short line is drawn (around x=24, y=5..20)
    var shortLineDrawn = false;
    for (var y = 5; y <= 20; y++) {
      if (getAlpha(24, y) > 0) {
        shortLineDrawn = true;
        break;
      }
    }
    expect(shortLineDrawn, isTrue, reason: 'Short line should be drawn');
  });
}
