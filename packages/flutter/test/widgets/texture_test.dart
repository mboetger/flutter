// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Texture with freeze set to true', (WidgetTester tester) async {
    await tester.pumpWidget(const Center(child: Texture(textureId: 1, freeze: true)));

    final Texture texture = tester.firstWidget(find.byType(Texture));
    expect(texture, isNotNull);
    expect(texture.textureId, 1);
    expect(texture.freeze, true);

    final RenderObject renderObject = tester.firstRenderObject(find.byType(Texture));
    expect(renderObject, isNotNull);
    final textureBox = renderObject as TextureBox;
    expect(textureBox, isNotNull);
    expect(textureBox.textureId, 1);
    expect(textureBox.freeze, true);

    final containerLayer = ContainerLayer();
    addTearDown(containerLayer.dispose);
    final paintingContext = PaintingContext(containerLayer, Rect.zero);
    textureBox.paint(paintingContext, Offset.zero);
    final Layer layer = containerLayer.lastChild!;
    expect(layer, isNotNull);
    final textureLayer = layer as TextureLayer;
    expect(textureLayer, isNotNull);
    expect(textureLayer.textureId, 1);
    expect(textureLayer.freeze, true);
  });

  testWidgets('Texture with default FilterQuality', (WidgetTester tester) async {
    await tester.pumpWidget(const Center(child: Texture(textureId: 1)));

    final Texture texture = tester.firstWidget(find.byType(Texture));
    expect(texture, isNotNull);
    expect(texture.textureId, 1);
    expect(texture.filterQuality, FilterQuality.low);

    final RenderObject renderObject = tester.firstRenderObject(find.byType(Texture));
    expect(renderObject, isNotNull);
    final textureBox = renderObject as TextureBox;
    expect(textureBox, isNotNull);
    expect(textureBox.textureId, 1);
    expect(textureBox.filterQuality, FilterQuality.low);

    final containerLayer = ContainerLayer();
    addTearDown(containerLayer.dispose);
    final paintingContext = PaintingContext(containerLayer, Rect.zero);
    textureBox.paint(paintingContext, Offset.zero);
    final Layer layer = containerLayer.lastChild!;
    expect(layer, isNotNull);
    final textureLayer = layer as TextureLayer;
    expect(textureLayer, isNotNull);
    expect(textureLayer.textureId, 1);
    expect(textureLayer.filterQuality, FilterQuality.low);
  });

  testWidgets('Texture with FilterQuality.none', (WidgetTester tester) async {
    await tester.pumpWidget(
      const Center(child: Texture(textureId: 1, filterQuality: FilterQuality.none)),
    );

    final Texture texture = tester.firstWidget(find.byType(Texture));
    expect(texture, isNotNull);
    expect(texture.textureId, 1);
    expect(texture.filterQuality, FilterQuality.none);

    final RenderObject renderObject = tester.firstRenderObject(find.byType(Texture));
    expect(renderObject, isNotNull);
    final textureBox = renderObject as TextureBox;
    expect(textureBox, isNotNull);
    expect(textureBox.textureId, 1);
    expect(textureBox.filterQuality, FilterQuality.none);

    final containerLayer = ContainerLayer();
    addTearDown(containerLayer.dispose);
    final paintingContext = PaintingContext(containerLayer, Rect.zero);
    textureBox.paint(paintingContext, Offset.zero);
    final Layer layer = containerLayer.lastChild!;
    expect(layer, isNotNull);
    final textureLayer = layer as TextureLayer;
    expect(textureLayer, isNotNull);
    expect(textureLayer.textureId, 1);
    expect(textureLayer.filterQuality, FilterQuality.none);
  });

  testWidgets('Texture with FilterQuality.low', (WidgetTester tester) async {
    await tester.pumpWidget(const Center(child: Texture(textureId: 1)));

    final Texture texture = tester.firstWidget(find.byType(Texture));
    expect(texture, isNotNull);
    expect(texture.textureId, 1);
    expect(texture.filterQuality, FilterQuality.low);

    final RenderObject renderObject = tester.firstRenderObject(find.byType(Texture));
    expect(renderObject, isNotNull);
    final textureBox = renderObject as TextureBox;
    expect(textureBox, isNotNull);
    expect(textureBox.textureId, 1);
    expect(textureBox.filterQuality, FilterQuality.low);

    final containerLayer = ContainerLayer();
    addTearDown(containerLayer.dispose);
    final paintingContext = PaintingContext(containerLayer, Rect.zero);
    textureBox.paint(paintingContext, Offset.zero);
    final Layer layer = containerLayer.lastChild!;
    expect(layer, isNotNull);
    final textureLayer = layer as TextureLayer;
    expect(textureLayer, isNotNull);
    expect(textureLayer.textureId, 1);
    expect(textureLayer.filterQuality, FilterQuality.low);
  });

  testWidgets('Texture screenshot via RepaintBoundary completes successfully - issue #63265', (
    WidgetTester tester,
  ) async {
    final GlobalKey key = GlobalKey();
    await tester.pumpWidget(
      Center(
        child: RepaintBoundary(
          key: key,
          child: const SizedBox(width: 100, height: 100, child: Texture(textureId: 1)),
        ),
      ),
    );

    final RenderObject renderObject = tester.firstRenderObject(find.byType(Texture));
    expect(renderObject, isNotNull);
    final textureBox = renderObject as TextureBox;
    expect(textureBox.isRepaintBoundary, true);
    expect(textureBox.alwaysNeedsCompositing, true);

    final containerLayer = ContainerLayer();
    addTearDown(containerLayer.dispose);
    final paintingContext = PaintingContext(containerLayer, Rect.zero);
    textureBox.paint(paintingContext, Offset.zero);
    final Layer layer = containerLayer.lastChild!;
    expect(layer, isNotNull);
    final textureLayer = layer as TextureLayer;
    expect(textureLayer.supportsRasterization(), true);

    final boundary = key.currentContext!.findRenderObject()! as RenderRepaintBoundary;
    final ui.Image image = (await tester.runAsync<ui.Image>(() => boundary.toImage()))!;
    expect(image.width, 100);
    expect(image.height, 100);

    final ByteData? byteData = await tester.runAsync<ByteData?>(() => image.toByteData());
    expect(byteData, isNotNull);

    // In a headless test environment, no backend texture is registered for textureId 1
    // in the engine's TextureRegistry, so rasterizing the layer produces empty/transparent
    // bytes (all zeros). On a real device with a registered backend texture (such as
    // video_player or camera), TextureLayer rasterizes the texture surface into the image
    // (supported since PR #109900 / commit 44d3181deb9936d6c72a786a5acc06c669680e43).
    var hasNonZeroPixel = false;
    final Uint8List bytes = byteData!.buffer.asUint8List();
    for (var i = 0; i < bytes.length; i++) {
      if (bytes[i] != 0) {
        hasNonZeroPixel = true;
        break;
      }
    }
    expect(
      hasNonZeroPixel,
      isFalse,
      reason:
          'In headless test mode without a registered backend texture, screenshot bytes are all zeros.',
    );
  });
}
