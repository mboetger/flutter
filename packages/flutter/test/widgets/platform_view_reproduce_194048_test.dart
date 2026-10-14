// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

import '../services/fake_platform_views.dart';

Widget _buildPlatformViewColumn({required bool showSecond}) {
  return Directionality(
    textDirection: TextDirection.ltr,
    child: Column(
      children: <Widget>[
        const Expanded(
          child: AndroidView(
            key: ValueKey<String>('first_view'),
            viewType: 'webview',
            layoutDirection: TextDirection.ltr,
          ),
        ),
        if (showSecond)
          const SizedBox(
            width: 200.0,
            height: 160.0,
            child: AndroidView(
              key: ValueKey<String>('second_view'),
              viewType: 'red_surface_view',
              layoutDirection: TextDirection.ltr,
            ),
          ),
      ],
    ),
  );
}

Widget _buildSurfacePlatformView({required Key key, required String viewType}) {
  return PlatformViewLink(
    key: key,
    viewType: viewType,
    surfaceFactory: (BuildContext context, PlatformViewController controller) {
      return AndroidViewSurface(
        controller: controller as AndroidViewController,
        gestureRecognizers: const <Factory<OneSequenceGestureRecognizer>>{},
        hitTestBehavior: PlatformViewHitTestBehavior.opaque,
      );
    },
    onCreatePlatformView: (PlatformViewCreationParams params) {
      return PlatformViewsService.initSurfaceAndroidView(
          id: params.id,
          viewType: viewType,
          layoutDirection: TextDirection.ltr,
          onFocus: () {
            params.onFocusChanged(true);
          },
        )
        ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
        ..create();
    },
  );
}

Widget _buildSurfacePlatformViewColumn({required bool showSecond}) {
  return Directionality(
    textDirection: TextDirection.ltr,
    child: Column(
      children: <Widget>[
        Expanded(
          child: _buildSurfacePlatformView(
            key: const ValueKey<String>('first_surface_view'),
            viewType: 'webview',
          ),
        ),
        if (showSecond)
          SizedBox(
            width: 200.0,
            height: 160.0,
            child: _buildSurfacePlatformView(
              key: const ValueKey<String>('second_surface_view'),
              viewType: 'red_surface_view',
            ),
          ),
      ],
    ),
  );
}

/// Computes the total vertical height covered by painted [TextureLayer]s and
/// [PlatformViewLayer]s in [tester], accounting for any ancestor [ClipRectLayer]s
/// applied during asynchronous resizing.
double _computeCoveredHeight(WidgetTester tester) {
  var totalHeight = 0.0;
  for (final Layer layer in tester.layers) {
    final Rect? layerRect = switch (layer) {
      TextureLayer(:final Rect rect) => rect,
      PlatformViewLayer(:final Rect rect) => rect,
      _ => null,
    };
    if (layerRect == null) {
      continue;
    }
    Rect visibleRect = layerRect;
    ContainerLayer? parent = layer.parent;
    while (parent != null) {
      if (parent is ClipRectLayer && parent.clipRect != null) {
        visibleRect = visibleRect.intersect(parent.clipRect!);
      }
      parent = parent.parent;
    }
    if (!visibleRect.isEmpty) {
      totalHeight += visibleRect.height;
    }
  }
  return totalHeight;
}

void main() {
  testWidgets('Showing a second PlatformView in a Column with an Expanded PlatformView '
      'does not leave an uncovered gap while platform view creation/resize is in flight '
      '(reproduces #194048)', (WidgetTester tester) async {
    final viewsController = FakeAndroidPlatformViewsController();
    viewsController.registerViewType('webview');
    viewsController.registerViewType('red_surface_view');

    // 1. Initial state (showSecond = false): first PlatformView fills 800x600.
    await tester.pumpWidget(_buildPlatformViewColumn(showSecond: false));
    await tester.pump();

    List<TextureLayer> textureLayers = tester.layers.whereType<TextureLayer>().toList();
    expect(textureLayers, hasLength(1));
    expect(textureLayers.single.rect, const Rect.fromLTWH(0.0, 0.0, 800.0, 600.0));
    expect(_computeCoveredHeight(tester), equals(600.0));

    // 2. Show second PlatformView (showSecond: false -> true) while create/resize are in flight.
    viewsController.createCompleter = Completer<void>();
    viewsController.resizeCompleter = Completer<void>();

    await tester.pumpWidget(_buildPlatformViewColumn(showSecond: true));

    final double showInFlightCoveredHeight = _computeCoveredHeight(tester);

    // Even if resizeCompleter completes before createCompleter, the first view must not
    // shrink or clip before the second view is created.
    viewsController.resizeCompleter!.complete();
    viewsController.resizeCompleter = null;
    await tester.pump();
    expect(_computeCoveredHeight(tester), equals(600.0));

    // Complete create and verify settled post-transition state.
    viewsController.createCompleter!.complete();
    viewsController.createCompleter = null;
    await tester.pump();
    await tester.pump();

    textureLayers = tester.layers.whereType<TextureLayer>().toList();
    expect(textureLayers, hasLength(2));
    expect(textureLayers[0].rect, const Rect.fromLTWH(0.0, 0.0, 800.0, 440.0));
    expect(textureLayers[1].rect.size, const Size(200.0, 160.0));
    expect(_computeCoveredHeight(tester), equals(600.0));

    expect(
      showInFlightCoveredHeight,
      equals(600.0),
      reason:
          'When showing the second PlatformView, the first PlatformView clipped to '
          '$showInFlightCoveredHeight before the second PlatformView was created, leaving a '
          '${600.0 - showInFlightCoveredHeight}px uncovered gap at the bottom.',
    );
  });

  testWidgets('Hiding and re-showing a second PlatformView in a Column with an Expanded PlatformView '
      'does not leave an uncovered gap while platform view resize/creation is in flight '
      '(reproduces #194048)', (WidgetTester tester) async {
    final viewsController = FakeAndroidPlatformViewsController();
    viewsController.registerViewType('webview');
    viewsController.registerViewType('red_surface_view');

    // 1. Start at showSecond = false (800x600), then transition to showSecond = true (800x440 + 200x160).
    await tester.pumpWidget(_buildPlatformViewColumn(showSecond: false));
    await tester.pump();
    await tester.pumpWidget(_buildPlatformViewColumn(showSecond: true));
    await tester.pump();
    await tester.pump();

    List<TextureLayer> textureLayers = tester.layers.whereType<TextureLayer>().toList();
    expect(textureLayers, hasLength(2));
    expect(textureLayers[0].rect, const Rect.fromLTWH(0.0, 0.0, 800.0, 440.0));
    expect(textureLayers[1].rect.size, const Size(200.0, 160.0));
    expect(_computeCoveredHeight(tester), equals(600.0));

    // 2. Hide second PlatformView (showSecond: true -> false) while resize back to 800x600 is in flight.
    viewsController.resizeCompleter = Completer<void>();

    await tester.pumpWidget(_buildPlatformViewColumn(showSecond: false));

    final double hideInFlightCoveredHeight = _computeCoveredHeight(tester);

    // Complete resize and verify settled post-hide state (1 TextureLayer of 800x600, no ClipRectLayer).
    viewsController.resizeCompleter!.complete();
    viewsController.resizeCompleter = null;
    await tester.pump();
    await tester.pump();

    textureLayers = tester.layers.whereType<TextureLayer>().toList();
    expect(textureLayers, hasLength(1));
    expect(textureLayers.single.rect, const Rect.fromLTWH(0.0, 0.0, 800.0, 600.0));
    expect(tester.layers.whereType<ClipRectLayer>(), isEmpty);
    expect(_computeCoveredHeight(tester), equals(600.0));

    // 3. Re-show second PlatformView (showSecond: false -> true) while create/resize are in flight.
    viewsController.createCompleter = Completer<void>();
    viewsController.resizeCompleter = Completer<void>();

    await tester.pumpWidget(_buildPlatformViewColumn(showSecond: true));

    final double reshowInFlightCoveredHeight = _computeCoveredHeight(tester);

    viewsController.createCompleter!.complete();
    viewsController.resizeCompleter!.complete();
    viewsController.createCompleter = null;
    viewsController.resizeCompleter = null;
    await tester.pump();
    await tester.pump();

    expect(_computeCoveredHeight(tester), equals(600.0));

    expect(
      hideInFlightCoveredHeight,
      equals(600.0),
      reason:
          'When hiding the second PlatformView, it was removed immediately while the first '
          'PlatformView still painted its TextureLayer at height $hideInFlightCoveredHeight, '
          'leaving a ${600.0 - hideInFlightCoveredHeight}px uncovered gap at the bottom.',
    );
    expect(
      reshowInFlightCoveredHeight,
      equals(600.0),
      reason:
          'When re-showing the second PlatformView, the first PlatformView clipped to '
          '$reshowInFlightCoveredHeight before the second PlatformView was created, leaving a '
          '${600.0 - reshowInFlightCoveredHeight}px uncovered gap at the bottom.',
    );
  });

  testWidgets('PlatformViewLink + AndroidViewSurface in a Column does not leave an uncovered gap '
      'across show and hide transitions in both texture and hybrid fallback modes '
      '(reproduces #194048)', (WidgetTester tester) async {
    for (final allowTextureLayerMode in <bool>[true, false]) {
      final viewsController = FakeAndroidPlatformViewsController();
      viewsController.registerViewType('webview');
      viewsController.registerViewType('red_surface_view');

      // 1. Initial state (showSecond = false): first PlatformView fills 800x600.
      await tester.pumpWidget(_buildSurfacePlatformViewColumn(showSecond: false));
      await tester.pump();
      await tester.pump();
      expect(_computeCoveredHeight(tester), equals(600.0));

      // 2. Show second PlatformViewLink while create and resize are in flight.
      viewsController.allowTextureLayerMode = allowTextureLayerMode;
      viewsController.createCompleter = Completer<void>();
      viewsController.resizeCompleter = Completer<void>();

      await tester.pumpWidget(_buildSurfacePlatformViewColumn(showSecond: true));
      // Frame 1: second view is a PlatformViewPlaceholderBox; first view must cover 600px.
      expect(_computeCoveredHeight(tester), equals(600.0));

      // Unblock resize before create; first view must still cover 600px while placeholder awaits create.
      viewsController.resizeCompleter!.complete();
      viewsController.resizeCompleter = Completer<void>();
      await tester.pump();
      expect(_computeCoveredHeight(tester), equals(600.0));

      // Complete create; PlatformViewLink rebuilds to AndroidViewSurface while first view's resize is in flight.
      viewsController.createCompleter!.complete();
      viewsController.createCompleter = null;
      await tester.pump();
      expect(_computeCoveredHeight(tester), equals(600.0));

      // Complete first view's resize and verify settled state.
      viewsController.resizeCompleter!.complete();
      viewsController.resizeCompleter = null;
      await tester.pump();
      await tester.pump();
      expect(_computeCoveredHeight(tester), equals(600.0));

      // 3. Hide second PlatformViewLink while first view's resize back to 800x600 is in flight.
      viewsController.resizeCompleter = Completer<void>();
      await tester.pumpWidget(_buildSurfacePlatformViewColumn(showSecond: false));
      expect(_computeCoveredHeight(tester), equals(600.0));

      viewsController.resizeCompleter!.complete();
      viewsController.resizeCompleter = null;
      await tester.pump();
      await tester.pump();
      expect(_computeCoveredHeight(tester), equals(600.0));

      // Unmount before next iteration.
      await tester.pumpWidget(const SizedBox.shrink());
      await tester.pump();
    }
  });
}
