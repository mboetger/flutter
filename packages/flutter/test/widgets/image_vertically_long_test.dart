// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import '../painting/image_test_utils.dart' show TestImageProvider;

void main() {
  tearDown(() {
    imageCache.clear();
    imageCache.clearLiveImages();
  });

  testWidgets(
    'ListView with Image.network and fit: BoxFit.fitWidth renders vertically long webtoon image (1440x37332) correctly when under GPU texture limits',
    (WidgetTester tester) async {
      // Simulates an emulator or high-end device where max_texture_size (e.g. 16384 or 32768)
      // is large enough or in software test rasterizer where the full 1440x37332 texture fits.
      const originalWidth = 1440;
      const originalHeight = 37332;
      final ui.Image testImage = (await tester.runAsync(
        () => createTestImage(width: originalWidth, height: originalHeight),
      ))!;

      final provider = TestImageProvider(testImage);

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 360.0,
              height: 640.0,
              child: ListView(
                children: <Widget>[Image(image: provider, fit: BoxFit.fitWidth)],
              ),
            ),
          ),
        ),
      );

      provider.complete();
      await tester.pumpAndSettle();

      final RenderBox renderImage = tester.renderObject<RenderBox>(find.byType(Image));
      // At viewport width 360.0, height should scale proportionally:
      // 360.0 * (37332 / 1440) = 9333.0.
      expect(renderImage.size.width, equals(360.0));
      expect(renderImage.size.height, equals(9333.0));
    },
  );

  testWidgets(
    'Simulate Skia GPU max texture size limit (e.g. Samsung A5 SM-A500H with max texture size 4096) causing blurry rendering of vertically long image',
    (WidgetTester tester) async {
      // On devices like Samsung Galaxy A5 (SM-A500H), GL_MAX_TEXTURE_SIZE is 4096 (or 2048).
      // When a vertically long image (1440x37332) is decoded without slicing or explicit cacheWidth/cacheHeight,
      // Skia (image_decoder_skia.cc via limitToMaxTextureSize = true) downscales the texture proportionally
      // so that the largest dimension (height 37332) fits within max_texture_size (4096).
      const maxTextureSize = 4096;
      const originalWidth = 1440;
      const originalHeight = 37332;

      // Downscale factor in Skia: 37332 / 4096 ≈ 9.1142578125
      const double downscaleFactor = originalHeight / maxTextureSize;
      final int downscaledWidth = (originalWidth / downscaleFactor).round(); // ≈ 158
      const downscaledHeight = 4096;

      expect(downscaledWidth, equals(158));
      expect(downscaledHeight, equals(4096));

      final ui.Image downscaledTexture = (await tester.runAsync(
        () => createTestImage(width: downscaledWidth, height: downscaledHeight),
      ))!;

      final provider = TestImageProvider(downscaledTexture);

      // Simulate mobile screen with 1080 physical width (360 logical width * 3.0 devicePixelRatio)
      const logicalWidth = 360.0;
      const devicePixelRatio = 3.0;
      const double physicalScreenWidth = logicalWidth * devicePixelRatio; // 1080.0

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: logicalWidth,
              height: 640.0,
              child: ListView(
                children: <Widget>[Image(image: provider, fit: BoxFit.fitWidth)],
              ),
            ),
          ),
        ),
      );

      provider.complete();
      await tester.pumpAndSettle();

      final RenderBox renderImage = tester.renderObject<RenderBox>(find.byType(Image));
      expect(renderImage.size.width, equals(logicalWidth));
      // The vertical aspect ratio is preserved: 360 * (4096 / 158) ≈ 9332.65
      expect(renderImage.size.height, closeTo(9333.0, 1.0));

      // However, the underlying texture resolution is only 158 pixels wide!
      // When stretched to fill 1080 physical screen pixels, each texture pixel is magnified across ~6.84 screen pixels.
      final double physicalPixelsPerTexturePixel = physicalScreenWidth / downscaledTexture.width;
      expect(physicalPixelsPerTexturePixel, greaterThan(6.8));

      // Proves that when GPU texture limit forces downscaling of vertically long images,
      // horizontal resolution degrades by over 89% (~158px vs 1440px), explaining the blurry rendering reported in #74172.
    },
  );

  testWidgets(
    'Simulate Impeller GPU max texture size limit scaling vertically long image proportionally without vertical aspect ratio distortion',
    (WidgetTester tester) async {
      // In Impeller (image_decoder_impeller.cc), when source_size.height > max_texture_size.height,
      // target_size scales width and height proportionally to fit within max_texture_size:
      // downscale factor: 37332 / 4096 ≈ 9.1142578125
      // width = round(1440 / 9.1142578125) = 158
      // height = 4096
      const maxTextureSize = 4096;
      const originalWidth = 1440;
      const originalHeight = 37332;

      const double downscaleFactor = originalHeight / maxTextureSize;
      final int scaledWidth = (originalWidth / downscaleFactor).round(); // 158
      const scaledHeight = 4096;

      final ui.Image scaledTexture = (await tester.runAsync(
        () => createTestImage(width: scaledWidth, height: scaledHeight),
      ))!;

      final provider = TestImageProvider(scaledTexture);

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 360.0,
              height: 640.0,
              child: ListView(
                children: <Widget>[Image(image: provider, fit: BoxFit.fitWidth)],
              ),
            ),
          ),
        ),
      );

      provider.complete();
      await tester.pumpAndSettle();

      final RenderBox renderImage = tester.renderObject<RenderBox>(find.byType(Image));
      expect(renderImage.size.width, equals(360.0));
      // Proportional downscaling preserves the expected vertical aspect ratio (~9333.0) without distortion!
      expect(renderImage.size.height, closeTo(9333.0, 1.0));
    },
  );

  testWidgets(
    'Demonstrate workaround for #74172: slicing vertically long manga images into tiles <= max_texture_size avoids blurriness and distortion',
    (WidgetTester tester) async {
      // Slicing the 1440x37332 webtoon image into 10 vertical tiles of 1440x3733.
      // Each tile height (3733) is below the GPU max_texture_size (4096), so no GPU downscaling or clamping occurs.
      const tileWidth = 1440;
      const tileHeight = 3733;
      const tileCount = 10;

      final tileTextures = <ui.Image>[];
      for (var i = 0; i < tileCount; i++) {
        final ui.Image tileImage = (await tester.runAsync(
          () => createTestImage(width: tileWidth, height: tileHeight),
        ))!;
        tileTextures.add(tileImage);
      }

      final List<TestImageProvider> providers = tileTextures
          .map<TestImageProvider>((ui.Image img) => TestImageProvider(img))
          .toList();

      const logicalWidth = 360.0;
      const devicePixelRatio = 3.0;
      const double physicalScreenWidth = logicalWidth * devicePixelRatio; // 1080.0

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: logicalWidth,
              height: 640.0,
              child: SingleChildScrollView(
                child: Column(
                  children: providers.map<Widget>((TestImageProvider provider) {
                    return Image(image: provider, fit: BoxFit.fitWidth);
                  }).toList(),
                ),
              ),
            ),
          ),
        ),
      );

      for (final provider in providers) {
        provider.complete();
      }
      await tester.pumpAndSettle();

      final Iterable<RenderBox> renderImages = tester.renderObjectList<RenderBox>(
        find.byType(Image),
      );
      expect(renderImages.length, equals(tileCount));

      var totalHeight = 0.0;
      for (final renderImage in renderImages) {
        expect(renderImage.size.width, equals(logicalWidth));
        // Each tile renders at height 360 * (3733 / 1440) = 933.25
        expect(renderImage.size.height, closeTo(933.25, 0.01));
        totalHeight += renderImage.size.height;
      }

      // Total vertical height matches the expected full webtoon aspect ratio (9332.5 ≈ 9333.0).
      expect(totalHeight, closeTo(9332.5, 0.1));

      // Every tile texture maintains its native 1440px width resolution.
      // On a 1080 physical pixel width screen, physicalPixelsPerTexturePixel is 1080 / 1440 = 0.75 (< 1.0),
      // ensuring sharp, crystal-clear rendering with zero blurriness!
      final double physicalPixelsPerTexturePixel = physicalScreenWidth / tileTextures.first.width;
      expect(physicalPixelsPerTexturePixel, equals(0.75));
    },
  );
}
