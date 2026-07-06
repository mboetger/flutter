// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';

import 'rendering_tester.dart';

void main() {
  TestRenderingFlutterBinding.ensureInitialized();

  test(
    'FlutterJNI.getBitmap / uncompressed screenshot with alpha channel regression test for #73036',
    () async {
      // Regression test for https://github.com/flutter/flutter/issues/73036.
      // When taking an uncompressed screenshot of a widget tree with an alpha channel
      // (e.g., 50% alpha white, #80FFFFFF), Skia/Impeller renders to a surface with
      // premultiplied alpha (kPremul_SkAlphaType), so R, G, B are multiplied by A (0.5),
      // resulting in raw pixel values of 128, 128, 128, 128 (#80808080).
      //
      // In FlutterJNI.getBitmap() on Android (step 4 of issue #73036), the engine calls
      // Screenshot(ScreenshotType::UncompressedImage, false), which returns these raw
      // unmodified (premultiplied) bytes from OffscreenSurface::GetRasterData(false).
      // Those bytes are directly copied into an Android Bitmap (ARGB_8888) via
      // Bitmap.copyPixelsFromBuffer(). When the Bitmap is exported to PNG or inspected,
      // the RGB values remain premultiplied (#80808080) instead of the expected
      // straight/unpremultiplied alpha (#80FFFFFF).
      //
      // In Dart, ui.ImageByteFormat.rawUnmodified corresponds to the exact raw buffer
      // returned by ScreenshotType::UncompressedImage in the engine.
      final boundary = RenderRepaintBoundary();
      final container = RenderDecoratedBox(
        decoration: const BoxDecoration(color: Color.fromARGB(128, 255, 255, 255)),
        child: RenderConstrainedBox(
          additionalConstraints: BoxConstraints.tight(const Size.square(50.0)),
        ),
      );
      boundary.child = container;

      layout(boundary, constraints: BoxConstraints.tight(const Size.square(50.0)));
      pumpFrame(phase: EnginePhase.composite);

      final ui.Image image = await boundary.toImage();

      // Request raw unmodified image data (which corresponds to ScreenshotType::UncompressedImage
      // in native C++ used by FlutterJNI.getBitmap()).
      final ByteData? rawUnmodifiedData = await image.toByteData(
        format: ui.ImageByteFormat.rawUnmodified,
      );
      expect(rawUnmodifiedData, isNotNull);

      // Find the center pixel of the 50x50 white container with 50% alpha.
      final int centerX = image.width ~/ 2;
      final int centerY = image.height ~/ 2;
      final int pixelOffset = (centerY * image.width + centerX) * 4;

      final int r = rawUnmodifiedData!.getUint8(pixelOffset);
      final int g = rawUnmodifiedData.getUint8(pixelOffset + 1);
      final int b = rawUnmodifiedData.getUint8(pixelOffset + 2);
      final int a = rawUnmodifiedData.getUint8(pixelOffset + 3);

      // On the unmodified codebase, because rawUnmodified / UncompressedImage returns
      // premultiplied alpha from the Skia rendering surface, R, G, and B are 128 (0x80),
      // causing this test to fail and reproducing issue #73036.
      expect(a, equals(128));
      expect(
        r,
        equals(255),
        reason:
            'Red channel should be unpremultiplied (255) for #80FFFFFF, but got $r (premultiplied #80808080)',
      );
      expect(
        g,
        equals(255),
        reason:
            'Green channel should be unpremultiplied (255) for #80FFFFFF, but got $g (premultiplied #80808080)',
      );
      expect(
        b,
        equals(255),
        reason:
            'Blue channel should be unpremultiplied (255) for #80FFFFFF, but got $b (premultiplied #80808080)',
      );
    },
  );
}
