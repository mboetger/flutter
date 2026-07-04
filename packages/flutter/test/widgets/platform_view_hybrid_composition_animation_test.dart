// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@TestOn('!chrome')
library;

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

import '../services/fake_platform_views.dart';

void main() {
  group('issue #68747: Hybrid Composition animation slowdown and gesture reproduction', () {
    testWidgets(
      'Hybrid Composition (SurfaceAndroidWebView simulation) forces PlatformViewLayer in render tree',
      (WidgetTester tester) async {
        final viewsController = FakeAndroidPlatformViewsController();
        viewsController.registerViewType('webview');

        await tester.pumpWidget(
          Directionality(
            textDirection: TextDirection.ltr,
            child: Center(
              child: SizedBox(
                width: 300.0,
                height: 200.0,
                child: PlatformViewLink(
                  viewType: 'webview',
                  surfaceFactory: (BuildContext context, PlatformViewController controller) {
                    return AndroidViewSurface(
                      controller: controller as AndroidViewController,
                      gestureRecognizers: const <Factory<OneSequenceGestureRecognizer>>{},
                      hitTestBehavior: PlatformViewHitTestBehavior.opaque,
                    );
                  },
                  onCreatePlatformView: (PlatformViewCreationParams params) {
                    return PlatformViewsService.initExpensiveAndroidView(
                        id: params.id,
                        viewType: 'webview',
                        layoutDirection: TextDirection.ltr,
                      )
                      ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
                      ..create();
                  },
                ),
              ),
            ),
          ),
        );

        await tester.pumpAndSettle();

        // Verify that in Hybrid Composition mode (initExpensiveAndroidView / SurfaceAndroidWebView),
        // the widget builds _PlatformLayerBasedAndroidViewSurface instead of _TextureBasedAndroidViewSurface.
        expect(
          find.byWidgetPredicate(
            (Widget widget) =>
                widget.runtimeType.toString() == '_PlatformLayerBasedAndroidViewSurface',
          ),
          findsOneWidget,
        );
        expect(
          find.byWidgetPredicate(
            (Widget widget) => widget.runtimeType.toString() == '_TextureBasedAndroidViewSurface',
          ),
          findsNothing,
        );

        // Verify that the platform view created in FakeAndroidPlatformViewsController has hybrid mode enabled.
        expect(viewsController.views.first.hybrid, isTrue);

        // Verify that in the rendering layer tree, a PlatformViewLayer is created,
        // which forces synchronous frame composition on the Android main platform thread.
        final RenderObject renderObject = tester.renderObject(find.byType(AndroidViewSurface));
        expect(renderObject.runtimeType.toString(), 'PlatformViewRenderBox');
      },
    );

    testWidgets('Default texture mode uses TextureLayer and asynchronous raster thread rendering', (
      WidgetTester tester,
    ) async {
      final viewsController = FakeAndroidPlatformViewsController();
      viewsController.registerViewType('webview');

      await tester.pumpWidget(
        Directionality(
          textDirection: TextDirection.ltr,
          child: Center(
            child: SizedBox(
              width: 300.0,
              height: 200.0,
              child: PlatformViewLink(
                viewType: 'webview',
                surfaceFactory: (BuildContext context, PlatformViewController controller) {
                  return AndroidViewSurface(
                    controller: controller as AndroidViewController,
                    gestureRecognizers: const <Factory<OneSequenceGestureRecognizer>>{},
                    hitTestBehavior: PlatformViewHitTestBehavior.opaque,
                  );
                },
                onCreatePlatformView: (PlatformViewCreationParams params) {
                  return PlatformViewsService.initAndroidView(
                      id: params.id,
                      viewType: 'webview',
                      layoutDirection: TextDirection.ltr,
                    )
                    ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
                    ..create();
                },
              ),
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      // Verify that without Hybrid Composition, texture-based rendering is used.
      expect(
        find.byWidgetPredicate(
          (Widget widget) => widget.runtimeType.toString() == '_TextureBasedAndroidViewSurface',
        ),
        findsOneWidget,
      );
      expect(
        find.byWidgetPredicate(
          (Widget widget) =>
              widget.runtimeType.toString() == '_PlatformLayerBasedAndroidViewSurface',
        ),
        findsNothing,
      );

      // Verify that hybrid mode is not enabled on the fake platform view.
      expect(viewsController.views.first.hybrid, isNot(true));
    });

    testWidgets(
      'Slow swipe gesture across Hybrid Composition view requires synchronous layer offset updates on every frame',
      (WidgetTester tester) async {
        final viewsController = FakeAndroidPlatformViewsController();
        viewsController.registerViewType('webview');

        // Simulate webview_gesture.repro: swiping across a container with an embedded Hybrid Composition view.
        final scrollController = ScrollController();
        await tester.pumpWidget(
          Directionality(
            textDirection: TextDirection.ltr,
            child: ListView.builder(
              controller: scrollController,
              scrollDirection: Axis.horizontal,
              itemCount: 3,
              itemBuilder: (BuildContext context, int index) {
                if (index == 0) {
                  return SizedBox(
                    width: 400.0,
                    height: 600.0,
                    child: PlatformViewLink(
                      viewType: 'webview',
                      surfaceFactory: (BuildContext context, PlatformViewController controller) {
                        return AndroidViewSurface(
                          controller: controller as AndroidViewController,
                          gestureRecognizers: <Factory<OneSequenceGestureRecognizer>>{
                            Factory<OneSequenceGestureRecognizer>(() => EagerGestureRecognizer()),
                          },
                          hitTestBehavior: PlatformViewHitTestBehavior.opaque,
                        );
                      },
                      onCreatePlatformView: (PlatformViewCreationParams params) {
                        return PlatformViewsService.initExpensiveAndroidView(
                            id: params.id,
                            viewType: 'webview',
                            layoutDirection: TextDirection.ltr,
                          )
                          ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
                          ..create();
                      },
                    ),
                  );
                }
                return Container(width: 400.0, height: 600.0, color: const Color(0xFF00FF00));
              },
            ),
          ),
        );

        await tester.pumpAndSettle();

        // Verify initial state: Hybrid composition surface is present.
        expect(
          find.byWidgetPredicate(
            (Widget widget) =>
                widget.runtimeType.toString() == '_PlatformLayerBasedAndroidViewSurface',
          ),
          findsOneWidget,
        );
        expect(viewsController.views.first.hybrid, isTrue);

        // Simulate step 2 & 4 of issue #68747: swipe slow from right to left across the view.
        final TestGesture gesture = await tester.startGesture(const Offset(350.0, 300.0));
        await tester.pump();

        // Perform a slow swipe across 10 animation frames.
        // In Hybrid Composition mode, every frame of this swipe forces synchronous layer
        // composition and position synchronization on the Android UI thread.
        for (var i = 0; i < 10; i++) {
          await gesture.moveBy(const Offset(-20.0, 0.0));
          await tester.pump(const Duration(milliseconds: 16)); // ~60fps frame tick

          // Verify that during the swipe animation across all frames, the platform view
          // remains in hybrid composition layer mode, requiring synchronous OS thread rendering.
          expect(
            find.byWidgetPredicate(
              (Widget widget) =>
                  widget.runtimeType.toString() == '_PlatformLayerBasedAndroidViewSurface',
            ),
            findsOneWidget,
          );
          expect(viewsController.views.first.hybrid, isTrue);
        }

        await gesture.up();
        await tester.pumpAndSettle();
      },
    );

    testWidgets(
      'Continuous animation alongside Hybrid Composition platform view forces synchronous platform thread composition on every frame',
      (WidgetTester tester) async {
        final viewsController = FakeAndroidPlatformViewsController();
        viewsController.registerViewType('webview');

        await tester.pumpWidget(
          Directionality(
            textDirection: TextDirection.ltr,
            child: Stack(
              children: <Widget>[
                SizedBox(
                  width: 300.0,
                  height: 300.0,
                  child: PlatformViewLink(
                    viewType: 'webview',
                    surfaceFactory: (BuildContext context, PlatformViewController controller) {
                      return AndroidViewSurface(
                        controller: controller as AndroidViewController,
                        gestureRecognizers: const <Factory<OneSequenceGestureRecognizer>>{},
                        hitTestBehavior: PlatformViewHitTestBehavior.opaque,
                      );
                    },
                    onCreatePlatformView: (PlatformViewCreationParams params) {
                      return PlatformViewsService.initExpensiveAndroidView(
                          id: params.id,
                          viewType: 'webview',
                          layoutDirection: TextDirection.ltr,
                        )
                        ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
                        ..create();
                    },
                  ),
                ),
                TweenAnimationBuilder<double>(
                  tween: Tween<double>(begin: 0.0, end: 100.0),
                  duration: const Duration(seconds: 1),
                  builder: (BuildContext context, double value, Widget? child) {
                    return Positioned(
                      left: value,
                      top: 0,
                      child: const SizedBox(width: 50, height: 50),
                    );
                  },
                ),
              ],
            ),
          ),
        );

        // Advance animation frame by frame.
        for (var i = 0; i < 30; i++) {
          await tester.pump(const Duration(milliseconds: 33)); // ~30fps ticks
          // On every animation frame, check that the Hybrid Composition surface is active and requiring
          // view composition (which in the real engine forces synchronous platform thread rendering).
          expect(
            find.byWidgetPredicate(
              (Widget widget) =>
                  widget.runtimeType.toString() == '_PlatformLayerBasedAndroidViewSurface',
            ),
            findsOneWidget,
          );
          expect(viewsController.views.first.hybrid, isTrue);
        }
      },
    );
  });
}
